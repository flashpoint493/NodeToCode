// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#include "Core/Widgets/SN2CTagManager.h"
#include "Core/Widgets/SN2CTagCategoryTree.h"
#include "Core/Widgets/SN2CTaggedGraphsList.h"
#include "Core/N2CTagManager.h"
#include "Core/N2CSettings.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Images/SImage.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateIconFinder.h"
#include "DesktopPlatformModule.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "SN2CTagManager"

void SN2CTagManager::Construct(const FArguments& InArgs)
{
	OnTagSelectedDelegate = InArgs._OnTagSelected;
	OnCategorySelectedDelegate = InArgs._OnCategorySelected;
	OnSelectionChangedDelegate = InArgs._OnSelectionChanged;
	OnGraphDoubleClickedDelegate = InArgs._OnGraphDoubleClicked;
	OnBatchTranslateRequestedDelegate = InArgs._OnBatchTranslateRequested;
	OnExportJsonRequestedDelegate = InArgs._OnExportJsonRequested;
	OnRemoveSelectedRequestedDelegate = InArgs._OnRemoveSelectedRequested;
	OnSingleTranslateRequestedDelegate = InArgs._OnSingleTranslateRequested;
	OnSingleJsonExportRequestedDelegate = InArgs._OnSingleJsonExportRequested;
	OnViewTranslationRequestedDelegate = InArgs._OnViewTranslationRequested;

	bMinifyJson = InArgs._MinifyJsonByDefault;

	// Initialize output path from settings
	const UN2CSettings* Settings = GetDefault<UN2CSettings>();
	OutputPath = Settings->CustomTranslationOutputDirectory.Path;
	if (OutputPath.IsEmpty())
	{
		OutputPath = FPaths::ProjectSavedDir() / TEXT("NodeToCode") / TEXT("Translations");
	}

	ChildSlot
	[
		SNew(SVerticalBox)
		// Search bar (optional)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f)
		[
			SNew(SBox)
			.Visibility(InArgs._ShowSearchBar ? EVisibility::Visible : EVisibility::Collapsed)
			[
				SNew(SHorizontalBox)
				// Search input
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SAssignNew(SearchBox, SEditableTextBox)
					.HintText(LOCTEXT("SearchHint", "Search tags or graphs..."))
					.OnTextChanged(this, &SN2CTagManager::HandleSearchTextChanged)
				]
			]
		]
		// Main split panel
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(4.0f, 0.0f)
		[
			SNew(SSplitter)
			.Orientation(Orient_Horizontal)
			// Left panel - Categories tree
			+ SSplitter::Slot()
			.Value(0.3f)
			[
				SAssignNew(CategoryTree, SN2CTagCategoryTree)
				.OnSelectionChanged(FSimpleDelegate::CreateSP(this, &SN2CTagManager::HandleTreeSelectionChanged))
			]
			// Right panel - Graphs list
			+ SSplitter::Slot()
			.Value(0.7f)
			[
				SAssignNew(GraphsList, SN2CTaggedGraphsList)
				.OnSelectionChanged(FSimpleDelegate::CreateSP(this, &SN2CTagManager::HandleListSelectionChanged))
				.OnGraphDoubleClicked(FSimpleDelegate::CreateSP(this, &SN2CTagManager::HandleGraphDoubleClicked))
				.OnSingleTranslateRequested(FSimpleDelegate::CreateSP(this, &SN2CTagManager::HandleSingleTranslateRequested))
				.OnSingleJsonExportRequested(FSimpleDelegate::CreateSP(this, &SN2CTagManager::HandleSingleJsonExportRequested))
				.OnViewTranslationRequested(FSimpleDelegate::CreateSP(this, &SN2CTagManager::HandleViewTranslationRequested))
			]
		]
		// Actions bar (optional)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f)
		[
			SNew(SBox)
			.Visibility(InArgs._ShowActionBar ? EVisibility::Visible : EVisibility::Collapsed)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.Padding(8.0f)
				[
					SNew(SVerticalBox)
					// Actions header
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 0.0f, 0.0f, 4.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ActionsHeader", "ACTIONS"))
						.Font(FAppStyle::GetFontStyle("SmallFontBold"))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					]
					// Selection info
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 0.0f, 0.0f, 8.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("SelectionLabel", "Selection: "))
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SAssignNew(SelectionCountText, STextBlock)
							.Text(LOCTEXT("NoSelection", "0 graphs selected"))
							.ColorAndOpacity(FLinearColor(0.83f, 0.63f, 0.29f, 1.0f)) // Orange
						]
					]
					// Action buttons
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 0.0f, 0.0f, 8.0f)
					[
						SNew(SHorizontalBox)
						// Batch Translate button
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.0f, 0.0f, 8.0f, 0.0f)
						[
							SNew(SButton)
							.ButtonStyle(FAppStyle::Get(), "Button")
							.OnClicked(this, &SN2CTagManager::HandleBatchTranslateClicked)
							.ContentPadding(FMargin(8.0f, 4.0f))
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(0.0f, 0.0f, 4.0f, 0.0f)
								[
									SNew(SImage)
									.Image(FSlateIconFinder::FindIconBrushForClass(nullptr, FName("Icons.Convert")))
									.DesiredSizeOverride(FVector2D(16.0f, 16.0f))
								]
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								[
									SNew(STextBlock)
									.Text(LOCTEXT("BatchTranslateButton", "Batch Translate"))
								]
							]
						]
						// Export JSON button
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.0f, 0.0f, 8.0f, 0.0f)
						[
							SNew(SButton)
							.ButtonStyle(FAppStyle::Get(), "Button")
							.OnClicked(this, &SN2CTagManager::HandleExportJsonClicked)
							.ContentPadding(FMargin(8.0f, 4.0f))
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(0.0f, 0.0f, 4.0f, 0.0f)
								[
									SNew(SImage)
									.Image(FAppStyle::GetBrush("MainFrame.RefreshSourceCodeEditor"))
									.DesiredSizeOverride(FVector2D(16.0f, 16.0f))
								]
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								[
									SNew(STextBlock)
									.Text(LOCTEXT("ExportJsonButton", "Export JSON"))
								]
							]
						]
						// Remove Selected button
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SButton)
							.ButtonStyle(FAppStyle::Get(), "Button")
							.OnClicked(this, &SN2CTagManager::HandleRemoveSelectedClicked)
							.ContentPadding(FMargin(8.0f, 4.0f))
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(0.0f, 0.0f, 4.0f, 0.0f)
								[
									SNew(SImage)
									.Image(FAppStyle::GetBrush("GenericCommands.Delete"))
									.DesiredSizeOverride(FVector2D(16.0f, 16.0f))
								]
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								[
									SNew(STextBlock)
									.Text(LOCTEXT("RemoveSelectedButton", "Remove Selected"))
								]
							]
						]
					]
					// Export options row
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)
						// Minify checkbox
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(0.0f, 0.0f, 16.0f, 0.0f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								SAssignNew(MinifyCheckbox, SCheckBox)
								.IsChecked(bMinifyJson ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
								.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)
								{
									bMinifyJson = (NewState == ECheckBoxState::Checked);
								})
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(4.0f, 0.0f, 0.0f, 0.0f)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("MinifyJsonLabel", "Minify JSON"))
							]
						]
						// Output path
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(0.0f, 0.0f, 8.0f, 0.0f)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("OutputLabel", "Output:"))
						]
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.VAlign(VAlign_Center)
						.Padding(0.0f, 0.0f, 4.0f, 0.0f)
						[
							SAssignNew(OutputPathBox, SEditableTextBox)
							.Text(FText::FromString(OutputPath))
							.IsReadOnly(true)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SButton)
							.Text(LOCTEXT("BrowseButton", "Browse..."))
							.OnClicked(this, &SN2CTagManager::HandleBrowseClicked)
						]
					]
				]
			]
		]
	];

	// Subscribe to tag manager events for live updates
	OnTagAddedHandle = UN2CTagManager::Get().OnBlueprintTagAdded.AddRaw(this, &SN2CTagManager::HandleTagAdded);
	OnTagRemovedHandle = UN2CTagManager::Get().OnBlueprintTagRemoved.AddRaw(this, &SN2CTagManager::HandleTagRemoved);

	// Initial data load
	RefreshData();
}

SN2CTagManager::~SN2CTagManager()
{
	// Unsubscribe from tag manager events
	if (OnTagAddedHandle.IsValid())
	{
		UN2CTagManager::Get().OnBlueprintTagAdded.Remove(OnTagAddedHandle);
	}
	if (OnTagRemovedHandle.IsValid())
	{
		UN2CTagManager::Get().OnBlueprintTagRemoved.Remove(OnTagRemovedHandle);
	}
}

void SN2CTagManager::HandleTagAdded(const FN2CTaggedBlueprintGraph& TagInfo)
{
	RefreshData();
}

void SN2CTagManager::HandleTagRemoved(const FGuid& GraphGuid, const FString& RemovedTag)
{
	RefreshData();
}

void SN2CTagManager::RefreshData()
{
	if (CategoryTree.IsValid())
	{
		CategoryTree->RefreshData();
	}
	UpdateGraphsList();
}

FString SN2CTagManager::GetSelectedTag() const
{
	if (CategoryTree.IsValid())
	{
		return CategoryTree->GetSelectedTag();
	}
	return FString();
}

FString SN2CTagManager::GetSelectedCategory() const
{
	if (CategoryTree.IsValid())
	{
		return CategoryTree->GetSelectedCategory();
	}
	return FString();
}

bool SN2CTagManager::IsSelectedCategory() const
{
	if (CategoryTree.IsValid())
	{
		return CategoryTree->IsSelectedCategory();
	}
	return false;
}

TArray<FN2CTagInfo> SN2CTagManager::GetSelectedGraphs() const
{
	if (GraphsList.IsValid())
	{
		return GraphsList->GetSelectedGraphs();
	}
	return TArray<FN2CTagInfo>();
}

int32 SN2CTagManager::GetSelectedCount() const
{
	if (GraphsList.IsValid())
	{
		return GraphsList->GetSelectedCount();
	}
	return 0;
}

bool SN2CTagManager::IsMinifyJsonEnabled() const
{
	return bMinifyJson;
}

FString SN2CTagManager::GetOutputPath() const
{
	return OutputPath;
}

FN2CTagInfo SN2CTagManager::GetDoubleClickedGraph() const
{
	if (GraphsList.IsValid())
	{
		return GraphsList->GetDoubleClickedGraph();
	}
	return FN2CTagInfo();
}

FN2CTagInfo SN2CTagManager::GetTranslateRequestedGraph() const
{
	if (GraphsList.IsValid())
	{
		return GraphsList->GetTranslateRequestedGraph();
	}
	return FN2CTagInfo();
}

FN2CTagInfo SN2CTagManager::GetJsonExportRequestedGraph() const
{
	if (GraphsList.IsValid())
	{
		return GraphsList->GetJsonExportRequestedGraph();
	}
	return FN2CTagInfo();
}

FN2CTagInfo SN2CTagManager::GetViewTranslationRequestedGraph() const
{
	if (GraphsList.IsValid())
	{
		return GraphsList->GetViewTranslationRequestedGraph();
	}
	return FN2CTagInfo();
}

void SN2CTagManager::SelectTag(const FString& InTag, const FString& InCategory)
{
	if (CategoryTree.IsValid())
	{
		CategoryTree->SelectTag(InTag, InCategory);
		UpdateGraphsList();
	}
}

void SN2CTagManager::SelectCategory(const FString& Category)
{
	if (CategoryTree.IsValid())
	{
		CategoryTree->SelectCategory(Category);
		UpdateGraphsList();
	}
}

void SN2CTagManager::SetSearchFilter(const FString& SearchText)
{
	if (SearchBox.IsValid())
	{
		SearchBox->SetText(FText::FromString(SearchText));
	}
	HandleSearchTextChanged(FText::FromString(SearchText));
}

void SN2CTagManager::HandleTreeSelectionChanged()
{
	UpdateGraphsList();

	if (CategoryTree.IsValid())
	{
		if (CategoryTree->IsSelectedTag())
		{
			OnTagSelectedDelegate.ExecuteIfBound();
		}
		else if (CategoryTree->IsSelectedCategory())
		{
			OnCategorySelectedDelegate.ExecuteIfBound();
		}
	}
}

void SN2CTagManager::HandleListSelectionChanged()
{
	UpdateSelectionDisplay();
	OnSelectionChangedDelegate.ExecuteIfBound();
}

void SN2CTagManager::HandleGraphDoubleClicked()
{
	OnGraphDoubleClickedDelegate.ExecuteIfBound();
}

void SN2CTagManager::HandleSearchTextChanged(const FText& NewText)
{
	FString SearchText = NewText.ToString();
	CurrentSearchFilter = SearchText;

	if (CategoryTree.IsValid())
	{
		CategoryTree->SetSearchFilter(SearchText);
	}

	// When search is active, load ALL graphs so search can find matches across all tags
	// When search is cleared, go back to showing graphs for selected category/tag
	if (!SearchText.IsEmpty())
	{
		// Load all graphs from all tags
		if (GraphsList.IsValid())
		{
			UN2CTagManager& TagManager = UN2CTagManager::Get();
			const TArray<FN2CTaggedBlueprintGraph>& AllTaggedGraphs = TagManager.GetAllTags();

			TArray<FN2CTagInfo> AllTagInfos;
			for (const FN2CTaggedBlueprintGraph& Graph : AllTaggedGraphs)
			{
				FN2CTagInfo Info = FN2CTagInfo::FromTaggedGraph(Graph);
				AllTagInfos.Add(Info);
			}

			GraphsList->SetGraphs(AllTagInfos);
			GraphsList->SetHeaderPath(TEXT("Search Results"), TEXT(""));
			GraphsList->SetSearchFilter(SearchText);
		}
	}
	else
	{
		// Search cleared - go back to selection-based view
		UpdateGraphsList();
		if (GraphsList.IsValid())
		{
			GraphsList->SetSearchFilter(SearchText);
		}
	}
}

FReply SN2CTagManager::HandleBatchTranslateClicked()
{
	OnBatchTranslateRequestedDelegate.ExecuteIfBound();
	return FReply::Handled();
}

FReply SN2CTagManager::HandleExportJsonClicked()
{
	OnExportJsonRequestedDelegate.ExecuteIfBound();
	return FReply::Handled();
}

FReply SN2CTagManager::HandleRemoveSelectedClicked()
{
	OnRemoveSelectedRequestedDelegate.ExecuteIfBound();
	return FReply::Handled();
}

void SN2CTagManager::HandleSingleTranslateRequested()
{
	OnSingleTranslateRequestedDelegate.ExecuteIfBound();
}

void SN2CTagManager::HandleSingleJsonExportRequested()
{
	OnSingleJsonExportRequestedDelegate.ExecuteIfBound();
}

void SN2CTagManager::HandleViewTranslationRequested()
{
	OnViewTranslationRequestedDelegate.ExecuteIfBound();
}

FReply SN2CTagManager::HandleBrowseClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		FString SelectedPath;
		const bool bOpened = DesktopPlatform->OpenDirectoryDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			LOCTEXT("SelectOutputFolder", "Select Output Folder").ToString(),
			OutputPath,
			SelectedPath
		);

		if (bOpened && !SelectedPath.IsEmpty())
		{
			OutputPath = SelectedPath;
			if (OutputPathBox.IsValid())
			{
				OutputPathBox->SetText(FText::FromString(OutputPath));
			}
		}
	}

	return FReply::Handled();
}

void SN2CTagManager::UpdateGraphsList()
{
	if (!CategoryTree.IsValid() || !GraphsList.IsValid())
	{
		return;
	}

	TSharedPtr<FN2CTreeItem> SelectedItem = CategoryTree->GetSelectedItem();
	if (!SelectedItem.IsValid())
	{
		GraphsList->SetGraphs(TArray<FN2CTagInfo>());
		GraphsList->SetHeaderPath(TEXT(""), TEXT(""));
		return;
	}

	UN2CTagManager& TagManager = UN2CTagManager::Get();
	TArray<FN2CTagInfo> TagInfos;

	if (SelectedItem->IsCategory())
	{
		// Get all graphs in category
		TArray<FN2CTaggedBlueprintGraph> TaggedGraphs = TagManager.GetTagsInCategory(SelectedItem->Name);
		for (const FN2CTaggedBlueprintGraph& Graph : TaggedGraphs)
		{
			FN2CTagInfo Info = FN2CTagInfo::FromTaggedGraph(Graph);
			TagInfos.Add(Info);
		}
		GraphsList->SetHeaderPath(SelectedItem->Name, TEXT(""));
	}
	else
	{
		// Get all graphs with this tag
		TArray<FN2CTaggedBlueprintGraph> TaggedGraphs = TagManager.GetGraphsWithTag(SelectedItem->Name, SelectedItem->Category);
		for (const FN2CTaggedBlueprintGraph& Graph : TaggedGraphs)
		{
			FN2CTagInfo Info = FN2CTagInfo::FromTaggedGraph(Graph);
			TagInfos.Add(Info);
		}
		GraphsList->SetHeaderPath(SelectedItem->Category, SelectedItem->Name);
	}

	GraphsList->SetGraphs(TagInfos);
	UpdateSelectionDisplay();
}

void SN2CTagManager::UpdateSelectionDisplay()
{
	if (SelectionCountText.IsValid())
	{
		int32 Count = GetSelectedCount();
		FText DisplayText = FText::Format(
			LOCTEXT("SelectionCountFormat", "{0} {0}|plural(one=graph,other=graphs) selected"),
			FText::AsNumber(Count)
		);
		SelectionCountText->SetText(DisplayText);
	}
}

#undef LOCTEXT_NAMESPACE
