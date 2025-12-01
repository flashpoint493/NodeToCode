// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#include "TagManager/Widgets/SN2CMainWindow.h"
#include "TagManager/Widgets/SN2CTagManager.h"
#include "TagManager/Widgets/SN2CTranslationViewer.h"
#include "TagManager/Widgets/SN2CBatchProgressModal.h"
#include "Core/N2CEditorIntegration.h"
#include "Core/N2CGraphStateManager.h"
#include "LLM/N2CLLMModule.h"
#include "LLM/N2CBatchTranslationOrchestrator.h"
#include "Core/N2CSerializer.h"
#include "Core/N2CNodeCollector.h"
#include "Core/N2CNodeTranslator.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Layout/Visibility.h"
#include "Styling/AppStyle.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Editor.h"

#define LOCTEXT_NAMESPACE "SN2CMainWindow"

// NodeToCode color scheme
namespace N2CMainWindowColors
{
	const FLinearColor BgPanel = FLinearColor::FromSRGBColor(FColor(37, 37, 38));
	const FLinearColor BgOverlay = FLinearColor(0.0f, 0.0f, 0.0f, 0.7f); // Semi-transparent black for overlay background
}

void SN2CMainWindow::Construct(const FArguments& InArgs)
{
	OnBatchCompleteDelegate = InArgs._OnBatchComplete;
	OnTranslationCompleteDelegate = InArgs._OnTranslationComplete;
	bShowTranslationViewer = false;
	bShowBatchProgress = false;
	bIsSingleTranslationInProgress = false;

	ChildSlot
	[
		SNew(SOverlay)
		// Base layer - Tag Manager
		+ SOverlay::Slot()
		[
			SAssignNew(TagManager, SN2CTagManager)
			.ShowSearchBar(InArgs._ShowSearchBar)
			.ShowActionBar(InArgs._ShowActionBar)
			.OnTagSelected(FSimpleDelegate::CreateSP(this, &SN2CMainWindow::HandleTagSelected))
			.OnCategorySelected(FSimpleDelegate::CreateSP(this, &SN2CMainWindow::HandleCategorySelected))
			.OnSelectionChanged(FSimpleDelegate::CreateSP(this, &SN2CMainWindow::HandleSelectionChanged))
			.OnGraphDoubleClicked(FSimpleDelegate::CreateSP(this, &SN2CMainWindow::HandleGraphDoubleClicked))
			.OnBatchTranslateRequested(FSimpleDelegate::CreateSP(this, &SN2CMainWindow::HandleBatchTranslateRequested))
			.OnExportJsonRequested(FSimpleDelegate::CreateSP(this, &SN2CMainWindow::HandleExportJsonRequested))
			.OnRemoveSelectedRequested(FSimpleDelegate::CreateSP(this, &SN2CMainWindow::HandleRemoveSelectedRequested))
			.OnSingleTranslateRequested(FSimpleDelegate::CreateSP(this, &SN2CMainWindow::HandleSingleTranslateRequested))
			.OnSingleJsonExportRequested(FSimpleDelegate::CreateSP(this, &SN2CMainWindow::HandleSingleJsonExportRequested))
			.OnViewTranslationRequested(FSimpleDelegate::CreateSP(this, &SN2CMainWindow::HandleViewTranslationRequested))
		]

		// Translation Viewer overlay layer
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.Visibility(this, &SN2CMainWindow::GetTranslationOverlayVisibility)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.BorderBackgroundColor(N2CMainWindowColors::BgOverlay)
			.Padding(20.0f)
			[
				SAssignNew(TranslationViewer, SN2CTranslationViewer)
				.OnCloseRequested(FSimpleDelegate::CreateSP(this, &SN2CMainWindow::HideTranslationViewer))
			]
		]

		// Batch Progress modal layer (centered)
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBorder)
			.Visibility(this, &SN2CMainWindow::GetBatchProgressVisibility)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.BorderBackgroundColor(N2CMainWindowColors::BgOverlay)
			.Padding(0.0f)
			[
				SAssignNew(BatchProgressModal, SN2CBatchProgressModal)
				.OnCancelRequested(FSimpleDelegate::CreateLambda([this]()
				{
					UN2CBatchTranslationOrchestrator::Get().CancelBatch();
					HideBatchProgress();
				}))
			]
		]
	];

	// Bind to batch orchestrator native delegates (for C++ Slate widgets)
	UN2CBatchTranslationOrchestrator& Orchestrator = UN2CBatchTranslationOrchestrator::Get();

	// Store weak reference to self for safe lambda captures (renamed to avoid shadowing)
	TWeakPtr<SN2CMainWindow> WeakSelf = SharedThis(this);

	BatchItemCompleteHandle = Orchestrator.OnItemCompleteNative.AddLambda(
		[WeakSelf](const FN2CTagInfo& TagInfo, const FN2CTranslationResponse& Response, bool bSuccess, int32 ItemIndex, int32 TotalCount)
		{
			if (TSharedPtr<SN2CMainWindow> StrongSelf = WeakSelf.Pin())
			{
				StrongSelf->OnBatchItemComplete(TagInfo, Response, bSuccess, ItemIndex, TotalCount);
			}
		}
	);

	BatchCompleteHandle = Orchestrator.OnBatchCompleteNative.AddLambda(
		[WeakSelf](const FN2CBatchTranslationResult& Result)
		{
			if (TSharedPtr<SN2CMainWindow> StrongSelf = WeakSelf.Pin())
			{
				StrongSelf->OnBatchComplete(Result);
			}
		}
	);

	BatchProgressHandle = Orchestrator.OnProgressNative.AddLambda(
		[WeakSelf](int32 CurrentIndex, int32 TotalCount, const FString& GraphName)
		{
			if (TSharedPtr<SN2CMainWindow> StrongSelf = WeakSelf.Pin())
			{
				StrongSelf->OnBatchProgress(CurrentIndex, TotalCount, GraphName);
			}
		}
	);
}

SN2CMainWindow::~SN2CMainWindow()
{
	// Unbind from batch orchestrator native delegates
	UN2CBatchTranslationOrchestrator& Orchestrator = UN2CBatchTranslationOrchestrator::Get();

	if (BatchItemCompleteHandle.IsValid())
	{
		Orchestrator.OnItemCompleteNative.Remove(BatchItemCompleteHandle);
	}
	if (BatchCompleteHandle.IsValid())
	{
		Orchestrator.OnBatchCompleteNative.Remove(BatchCompleteHandle);
	}
	if (BatchProgressHandle.IsValid())
	{
		Orchestrator.OnProgressNative.Remove(BatchProgressHandle);
	}
}

void SN2CMainWindow::RefreshData()
{
	if (TagManager.IsValid())
	{
		TagManager->RefreshData();
	}
}

TArray<FN2CTagInfo> SN2CMainWindow::GetSelectedGraphs() const
{
	if (TagManager.IsValid())
	{
		return TagManager->GetSelectedGraphs();
	}
	return TArray<FN2CTagInfo>();
}

int32 SN2CMainWindow::GetSelectedCount() const
{
	if (TagManager.IsValid())
	{
		return TagManager->GetSelectedCount();
	}
	return 0;
}

void SN2CMainWindow::ShowTranslationViewer(const FN2CTagInfo& GraphInfo)
{
	if (TranslationViewer.IsValid())
	{
		if (TranslationViewer->LoadTranslation(GraphInfo))
		{
			bShowTranslationViewer = true;
		}
	}
}

void SN2CMainWindow::ShowTranslationViewer(const FN2CGraphTranslation& Translation, const FString& GraphName, const FString& JsonContent)
{
	if (TranslationViewer.IsValid())
	{
		TranslationViewer->LoadTranslation(Translation, GraphName, JsonContent);
		bShowTranslationViewer = true;
	}
}

void SN2CMainWindow::HideTranslationViewer()
{
	bShowTranslationViewer = false;
	if (TranslationViewer.IsValid())
	{
		TranslationViewer->Clear();
	}
}

void SN2CMainWindow::ShowBatchProgress()
{
	bShowBatchProgress = true;
}

void SN2CMainWindow::HideBatchProgress()
{
	bShowBatchProgress = false;
	if (BatchProgressModal.IsValid())
	{
		BatchProgressModal->Reset();
	}
}

// ==================== Tag Manager Event Handlers ====================

void SN2CMainWindow::HandleTagSelected()
{
	// Tag selection is handled internally by tag manager
}

void SN2CMainWindow::HandleCategorySelected()
{
	// Category selection is handled internally by tag manager
}

void SN2CMainWindow::HandleSelectionChanged()
{
	// Selection changes are reflected in the action bar automatically
}

void SN2CMainWindow::HandleGraphDoubleClicked()
{
	if (TagManager.IsValid())
	{
		FN2CTagInfo Graph = TagManager->GetDoubleClickedGraph();
		if (!Graph.GraphGuid.IsEmpty())
		{
			NavigateToGraph(Graph);
		}
	}
}

// ==================== Single Graph Action Handlers ====================

void SN2CMainWindow::HandleSingleTranslateRequested()
{
	if (TagManager.IsValid())
	{
		FN2CTagInfo Graph = TagManager->GetTranslateRequestedGraph();
		if (!Graph.GraphGuid.IsEmpty())
		{
			TranslateSingleGraph(Graph);
		}
	}
}

void SN2CMainWindow::HandleSingleJsonExportRequested()
{
	if (TagManager.IsValid())
	{
		FN2CTagInfo Graph = TagManager->GetJsonExportRequestedGraph();
		if (!Graph.GraphGuid.IsEmpty())
		{
			ExportSingleGraphToJson(Graph);
		}
	}
}

void SN2CMainWindow::HandleViewTranslationRequested()
{
	if (TagManager.IsValid())
	{
		FN2CTagInfo Graph = TagManager->GetViewTranslationRequestedGraph();
		if (!Graph.GraphGuid.IsEmpty())
		{
			ShowTranslationViewer(Graph);
		}
	}
}

// ==================== Batch Action Handlers ====================

void SN2CMainWindow::HandleBatchTranslateRequested()
{
	TArray<FN2CTagInfo> SelectedGraphs = GetSelectedGraphs();
	if (SelectedGraphs.Num() == 0)
	{
		return;
	}

	// Initialize and show the batch progress modal
	TArray<FString> GraphNames;
	for (const FN2CTagInfo& Graph : SelectedGraphs)
	{
		GraphNames.Add(Graph.GraphName);
	}

	if (BatchProgressModal.IsValid())
	{
		BatchProgressModal->Initialize(GraphNames);
	}
	ShowBatchProgress();

	// Start the batch translation
	UN2CBatchTranslationOrchestrator::Get().StartBatchTranslation(SelectedGraphs);
}

void SN2CMainWindow::HandleExportJsonRequested()
{
	TArray<FN2CTagInfo> SelectedGraphs = GetSelectedGraphs();
	if (SelectedGraphs.Num() == 0)
	{
		return;
	}

	// Get minify setting from tag manager
	bool bMinify = TagManager.IsValid() ? TagManager->IsMinifyJsonEnabled() : false;

	// Perform batch JSON export
	FN2CBatchJsonExportResult Result;
	UN2CBatchTranslationOrchestrator::Get().BatchExportJson(SelectedGraphs, Result, bMinify);
}

void SN2CMainWindow::HandleRemoveSelectedRequested()
{
	TArray<FN2CTagInfo> SelectedGraphs = GetSelectedGraphs();
	if (SelectedGraphs.Num() == 0)
	{
		return;
	}

	// Remove tags from all selected graphs
	UN2CGraphStateManager& StateManager = UN2CGraphStateManager::Get();
	for (const FN2CTagInfo& Graph : SelectedGraphs)
	{
		FGuid GraphGuid;
		if (FGuid::Parse(Graph.GraphGuid, GraphGuid))
		{
			StateManager.RemoveTag(GraphGuid, Graph.Tag, Graph.Category);
		}
	}

	// Refresh the UI
	RefreshData();
}

// ==================== Backend Integration ====================

void SN2CMainWindow::TranslateSingleGraph(const FN2CTagInfo& Graph)
{
	if (bIsSingleTranslationInProgress)
	{
		return; // Already translating
	}

	// Load the blueprint
	FSoftObjectPath BlueprintPath(Graph.BlueprintPath);
	UBlueprint* Blueprint = Cast<UBlueprint>(BlueprintPath.TryLoad());
	if (!Blueprint)
	{
		return;
	}

	// Find the graph
	FGuid GraphGuid;
	if (!FGuid::Parse(Graph.GraphGuid, GraphGuid))
	{
		return;
	}

	UEdGraph* EdGraph = FindGraphByGuid(Blueprint, GraphGuid);
	if (!EdGraph)
	{
		return;
	}

	// Collect and translate nodes
	TArray<UK2Node*> CollectedNodes;
	FN2CEditorIntegration::Get().CollectNodesFromGraph(EdGraph, CollectedNodes);

	FN2CBlueprint N2CBlueprint;
	FN2CEditorIntegration::Get().TranslateNodesToN2CBlueprint(CollectedNodes, N2CBlueprint);

	FString JsonContent = FN2CEditorIntegration::Get().SerializeN2CBlueprintToJson(N2CBlueprint, true);
	if (JsonContent.IsEmpty())
	{
		return;
	}

	// Store pending graph info for callback
	PendingSingleTranslateGraph = Graph;
	bIsSingleTranslationInProgress = true;

	// Send to LLM - bind to the module's response delegate
	UN2CLLMModule* LLMModule = UN2CLLMModule::Get();
	if (LLMModule)
	{
		// Store weak reference for safe lambda capture
		TWeakPtr<SN2CMainWindow> WeakSelf = SharedThis(this);

		// Process JSON with callback that captures raw response
		LLMModule->ProcessN2CJson(JsonContent, FOnLLMResponseReceived::CreateLambda(
			[WeakSelf](const FString& RawResponse)
			{
				// Response parsing is handled by LLM module - we get notified via OnTranslationResponseReceived
				// The actual handling is done through that delegate if we need it
			}
		));
	}
}

void SN2CMainWindow::ExportSingleGraphToJson(const FN2CTagInfo& Graph)
{
	// Load the blueprint
	FSoftObjectPath BlueprintPath(Graph.BlueprintPath);
	UBlueprint* Blueprint = Cast<UBlueprint>(BlueprintPath.TryLoad());
	if (!Blueprint)
	{
		return;
	}

	// Find the graph
	FGuid GraphGuid;
	if (!FGuid::Parse(Graph.GraphGuid, GraphGuid))
	{
		return;
	}

	UEdGraph* EdGraph = FindGraphByGuid(Blueprint, GraphGuid);
	if (!EdGraph)
	{
		return;
	}

	// Collect and translate nodes
	TArray<UK2Node*> CollectedNodes;
	FN2CEditorIntegration::Get().CollectNodesFromGraph(EdGraph, CollectedNodes);

	FN2CBlueprint N2CBlueprint;
	FN2CEditorIntegration::Get().TranslateNodesToN2CBlueprint(CollectedNodes, N2CBlueprint);

	bool bMinify = TagManager.IsValid() ? TagManager->IsMinifyJsonEnabled() : false;
	FString JsonContent = FN2CEditorIntegration::Get().SerializeN2CBlueprintToJson(N2CBlueprint, !bMinify);

	// Show in translation viewer (JSON tab)
	if (TranslationViewer.IsValid())
	{
		TranslationViewer->SetJsonContent(JsonContent, Graph.GraphName);
		bShowTranslationViewer = true;
	}
}

void SN2CMainWindow::NavigateToGraph(const FN2CTagInfo& Graph)
{
	// Load the blueprint
	FSoftObjectPath BlueprintPath(Graph.BlueprintPath);
	UBlueprint* Blueprint = Cast<UBlueprint>(BlueprintPath.TryLoad());
	if (!Blueprint)
	{
		return;
	}

	// Find the graph
	FGuid GraphGuid;
	if (!FGuid::Parse(Graph.GraphGuid, GraphGuid))
	{
		return;
	}

	UEdGraph* EdGraph = FindGraphByGuid(Blueprint, GraphGuid);
	if (!EdGraph)
	{
		return;
	}

	// Open the blueprint editor
	GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Blueprint);

	// Open the specific graph tab in the Blueprint editor
	// Use FKismetEditorUtilities to open and focus the graph
	FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(EdGraph);
}

UEdGraph* SN2CMainWindow::FindGraphByGuid(UBlueprint* Blueprint, const FGuid& GraphGuid)
{
	if (!Blueprint)
	{
		return nullptr;
	}

	// Search UbergraphPages
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (Graph && Graph->GraphGuid == GraphGuid)
		{
			return Graph;
		}
	}

	// Search FunctionGraphs
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GraphGuid == GraphGuid)
		{
			return Graph;
		}
	}

	// Search MacroGraphs
	for (UEdGraph* Graph : Blueprint->MacroGraphs)
	{
		if (Graph && Graph->GraphGuid == GraphGuid)
		{
			return Graph;
		}
	}

	// Search DelegateSignatureGraphs
	for (UEdGraph* Graph : Blueprint->DelegateSignatureGraphs)
	{
		if (Graph && Graph->GraphGuid == GraphGuid)
		{
			return Graph;
		}
	}

	return nullptr;
}

// ==================== Batch Translation Callbacks ====================

void SN2CMainWindow::OnBatchItemComplete(const FN2CTagInfo& TagInfo, const FN2CTranslationResponse& Response,
	bool bSuccess, int32 ItemIndex, int32 TotalCount)
{
	if (BatchProgressModal.IsValid())
	{
		BatchProgressModal->MarkItemComplete(TagInfo.GraphName, bSuccess);
	}
}

void SN2CMainWindow::OnBatchComplete(const FN2CBatchTranslationResult& Result)
{
	if (BatchProgressModal.IsValid())
	{
		BatchProgressModal->SetResult(Result);
	}

	// Fire external delegate
	OnBatchCompleteDelegate.ExecuteIfBound(Result);
}

void SN2CMainWindow::OnBatchProgress(int32 CurrentIndex, int32 TotalCount, const FString& GraphName)
{
	if (BatchProgressModal.IsValid())
	{
		BatchProgressModal->SetProgress(CurrentIndex, TotalCount, GraphName);
	}
}

// ==================== Single Translation Callbacks ====================

void SN2CMainWindow::OnSingleTranslationComplete(const FN2CTranslationResponse& Response, bool bSuccess)
{
	bIsSingleTranslationInProgress = false;

	if (bSuccess && Response.Graphs.Num() > 0)
	{
		// Show translation in viewer
		ShowTranslationViewer(Response.Graphs[0], PendingSingleTranslateGraph.GraphName);

		// Update translation state in state manager
		FGuid GraphGuid;
		if (FGuid::Parse(PendingSingleTranslateGraph.GraphGuid, GraphGuid))
		{
			// Create translation summary from the response
			FN2CTranslationSummary Summary;
			const FN2CGraphTranslation& Translation = Response.Graphs[0];
			Summary.DeclarationPreview = Translation.Code.GraphDeclaration.Left(100);
			TArray<FString> Lines;
			Translation.Code.GraphImplementation.ParseIntoArrayLines(Lines, false);
			Summary.ImplementationLines = Lines.Num();
			Summary.bHasNotes = !Translation.Code.ImplementationNotes.IsEmpty();

			// Get output path from LLM module
			FString OutputPath;
			if (UN2CLLMModule* LLMModule = UN2CLLMModule::Get())
			{
				OutputPath = LLMModule->GetLatestTranslationPath();
			}

			// Get provider and model info from settings
			FString Provider = TEXT("Unknown");
			FString Model = TEXT("Unknown");
			FString Language = TEXT("CPP");

			UN2CGraphStateManager::Get().SetTranslationState(
				GraphGuid,
				PendingSingleTranslateGraph.GraphName,
				FSoftObjectPath(PendingSingleTranslateGraph.BlueprintPath),
				OutputPath,
				Provider,
				Model,
				Language,
				Summary
			);
		}
	}

	// Fire external delegate
	OnTranslationCompleteDelegate.ExecuteIfBound(Response, bSuccess);

	// Clear pending graph
	PendingSingleTranslateGraph = FN2CTagInfo();
}

// ==================== Overlay Visibility ====================

EVisibility SN2CMainWindow::GetTranslationOverlayVisibility() const
{
	return bShowTranslationViewer ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SN2CMainWindow::GetBatchProgressVisibility() const
{
	return bShowBatchProgress ? EVisibility::Visible : EVisibility::Collapsed;
}

FReply SN2CMainWindow::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	// Handle Escape key to close overlays
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		if (bShowTranslationViewer)
		{
			HideTranslationViewer();
			return FReply::Handled();
		}
		if (bShowBatchProgress)
		{
			// Only allow closing batch progress if complete
			if (BatchProgressModal.IsValid() && !UN2CBatchTranslationOrchestrator::Get().IsBatchInProgress())
			{
				HideBatchProgress();
				return FReply::Handled();
			}
		}
	}

	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

#undef LOCTEXT_NAMESPACE
