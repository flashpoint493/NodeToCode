// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#include "LLM/N2CBatchTranslationOrchestrator.h"
#include "LLM/N2CLLMModule.h"
#include "Core/N2CNodeCollector.h"
#include "Core/N2CNodeTranslator.h"
#include "Core/N2CSerializer.h"
#include "Core/N2CSettings.h"
#include "Utils/N2CLogger.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "K2Node.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

UN2CBatchTranslationOrchestrator& UN2CBatchTranslationOrchestrator::Get()
{
	static UN2CBatchTranslationOrchestrator* Instance = nullptr;
	if (!Instance)
	{
		Instance = NewObject<UN2CBatchTranslationOrchestrator>();
		Instance->AddToRoot(); // Prevent garbage collection
	}
	return *Instance;
}

bool UN2CBatchTranslationOrchestrator::StartBatchTranslation(const TArray<FN2CTagInfo>& TagInfos)
{
	// Validate input
	if (TagInfos.IsEmpty())
	{
		FN2CLogger::Get().LogError(TEXT("Cannot start batch translation with empty TagInfos array"), TEXT("BatchOrchestrator"));
		return false;
	}

	// Check if already in progress
	if (bBatchInProgress)
	{
		FN2CLogger::Get().LogWarning(TEXT("Batch translation already in progress"), TEXT("BatchOrchestrator"));
		return false;
	}

	// Check if LLM module is already processing
	UN2CLLMModule* LLMModule = UN2CLLMModule::Get();
	if (LLMModule && LLMModule->GetSystemStatus() == EN2CSystemStatus::Processing)
	{
		FN2CLogger::Get().LogWarning(TEXT("LLM module is already processing a translation"), TEXT("BatchOrchestrator"));
		return false;
	}

	// Initialize LLM module if needed
	if (LLMModule && !LLMModule->IsInitialized())
	{
		if (!LLMModule->Initialize())
		{
			FN2CLogger::Get().LogError(TEXT("Failed to initialize LLM module for batch translation"), TEXT("BatchOrchestrator"));
			return false;
		}
	}

	// Initialize batch state
	bBatchInProgress = true;
	bCancellationRequested = false;
	CurrentItemIndex = -1;
	BatchStartTime = FPlatformTime::Seconds();
	BatchItems.Empty();
	BlueprintCache.Empty();
	CurrentResult = FN2CBatchTranslationResult();

	// Convert TagInfos to BatchItems
	for (const FN2CTagInfo& TagInfo : TagInfos)
	{
		FN2CBatchTranslationItem Item;
		Item.TagInfo = TagInfo;
		Item.Status = EN2CBatchItemStatus::Pending;
		BatchItems.Add(Item);
	}
	CurrentResult.TotalCount = BatchItems.Num();

	FN2CLogger::Get().Log(
		FString::Printf(TEXT("Starting batch translation with %d items"), BatchItems.Num()),
		EN2CLogSeverity::Info, TEXT("BatchOrchestrator"));

	// Create output directory
	CreateBatchOutputDirectory();

	// Resolve all blueprints and graphs upfront
	if (!ResolveBlueprintsAndGraphs())
	{
		FN2CLogger::Get().LogWarning(TEXT("Some items could not be resolved, continuing with valid items"), TEXT("BatchOrchestrator"));
	}

	// Check if we have any valid items to process
	bool bHasValidItems = false;
	for (const FN2CBatchTranslationItem& Item : BatchItems)
	{
		if (Item.Status == EN2CBatchItemStatus::Pending)
		{
			bHasValidItems = true;
			break;
		}
	}

	if (!bHasValidItems)
	{
		FN2CLogger::Get().LogError(TEXT("No valid items to process in batch"), TEXT("BatchOrchestrator"));
		CleanupBatch();
		return false;
	}

	// Bind to LLM module's response delegate
	if (LLMModule && !bIsBoundToLLMModule)
	{
		LLMModule->OnTranslationResponseReceived.AddDynamic(
			this, &UN2CBatchTranslationOrchestrator::HandleTranslationResponse);
		bIsBoundToLLMModule = true;
	}

	// Start processing first item
	ProcessNextItem();
	return true;
}

void UN2CBatchTranslationOrchestrator::CancelBatch()
{
	if (!bBatchInProgress)
	{
		return;
	}

	FN2CLogger::Get().Log(TEXT("Batch translation cancellation requested"), EN2CLogSeverity::Info, TEXT("BatchOrchestrator"));
	bCancellationRequested = true;
}

float UN2CBatchTranslationOrchestrator::GetBatchProgress() const
{
	if (BatchItems.IsEmpty())
	{
		return 0.0f;
	}

	int32 ProcessedCount = 0;
	for (const FN2CBatchTranslationItem& Item : BatchItems)
	{
		if (Item.Status != EN2CBatchItemStatus::Pending && Item.Status != EN2CBatchItemStatus::Processing)
		{
			ProcessedCount++;
		}
	}

	return static_cast<float>(ProcessedCount) / static_cast<float>(BatchItems.Num());
}

void UN2CBatchTranslationOrchestrator::ProcessNextItem()
{
	// Find next pending item
	CurrentItemIndex++;
	while (CurrentItemIndex < BatchItems.Num())
	{
		if (BatchItems[CurrentItemIndex].Status == EN2CBatchItemStatus::Pending)
		{
			break;
		}
		CurrentItemIndex++;
	}

	// Check if done
	if (CurrentItemIndex >= BatchItems.Num())
	{
		FinalizeBatch();
		return;
	}

	// Check cancellation
	if (bCancellationRequested)
	{
		for (int32 i = CurrentItemIndex; i < BatchItems.Num(); i++)
		{
			if (BatchItems[i].Status == EN2CBatchItemStatus::Pending)
			{
				BatchItems[i].Status = EN2CBatchItemStatus::Skipped;
				CurrentResult.SkippedCount++;
			}
		}
		FinalizeBatch();
		return;
	}

	FN2CBatchTranslationItem& Item = BatchItems[CurrentItemIndex];
	Item.Status = EN2CBatchItemStatus::Processing;

	// Broadcast progress
	OnProgress.Broadcast(CurrentItemIndex, BatchItems.Num(), Item.TagInfo.GraphName);

	FN2CLogger::Get().Log(
		FString::Printf(TEXT("Processing item %d/%d: %s"), CurrentItemIndex + 1, BatchItems.Num(), *Item.TagInfo.GraphName),
		EN2CLogSeverity::Info, TEXT("BatchOrchestrator"));

	// Collect and serialize the graph
	FString JsonPayload = CollectAndSerializeGraph(Item.CachedGraph.Get(), Item.CachedBlueprint.Get());

	if (JsonPayload.IsEmpty())
	{
		Item.Status = EN2CBatchItemStatus::Failed;
		Item.ErrorMessage = TEXT("Failed to collect/serialize graph");
		CurrentResult.FailureCount++;
		CurrentResult.FailedGraphNames.Add(Item.TagInfo.GraphName);

		FN2CLogger::Get().LogError(
			FString::Printf(TEXT("Failed to collect/serialize graph: %s"), *Item.TagInfo.GraphName),
			TEXT("BatchOrchestrator"));

		// Broadcast item completion with failure
		OnItemComplete.Broadcast(Item.TagInfo, Item.TranslationResponse, false, CurrentItemIndex, BatchItems.Num());

		// Continue to next item
		ProcessNextItem();
		return;
	}

	// Send to LLM module
	UN2CLLMModule* LLMModule = UN2CLLMModule::Get();
	if (LLMModule)
	{
		// Use the simpler ProcessN2CJson that triggers OnTranslationResponseReceived
		LLMModule->ProcessN2CJson(JsonPayload, FOnLLMResponseReceived());
	}
	else
	{
		Item.Status = EN2CBatchItemStatus::Failed;
		Item.ErrorMessage = TEXT("LLM module not available");
		CurrentResult.FailureCount++;
		CurrentResult.FailedGraphNames.Add(Item.TagInfo.GraphName);

		OnItemComplete.Broadcast(Item.TagInfo, Item.TranslationResponse, false, CurrentItemIndex, BatchItems.Num());
		ProcessNextItem();
	}
}

void UN2CBatchTranslationOrchestrator::HandleTranslationResponse(const FN2CTranslationResponse& Response, bool bSuccess)
{
	if (!bBatchInProgress || CurrentItemIndex < 0 || CurrentItemIndex >= BatchItems.Num())
	{
		return;
	}

	FN2CBatchTranslationItem& Item = BatchItems[CurrentItemIndex];

	// Only process if this item is currently being processed
	if (Item.Status != EN2CBatchItemStatus::Processing)
	{
		return;
	}

	Item.TranslationResponse = Response;

	if (bSuccess)
	{
		Item.Status = EN2CBatchItemStatus::Completed;
		CurrentResult.SuccessCount++;
		CurrentResult.TotalInputTokens += Response.Usage.InputTokens;
		CurrentResult.TotalOutputTokens += Response.Usage.OutputTokens;

		// Save individual translation files
		SaveItemTranslation(Item);

		FN2CLogger::Get().Log(
			FString::Printf(TEXT("Successfully translated: %s (tokens: %d in, %d out)"),
				*Item.TagInfo.GraphName, Response.Usage.InputTokens, Response.Usage.OutputTokens),
			EN2CLogSeverity::Info, TEXT("BatchOrchestrator"));
	}
	else
	{
		Item.Status = EN2CBatchItemStatus::Failed;
		Item.ErrorMessage = TEXT("Translation failed");
		CurrentResult.FailureCount++;
		CurrentResult.FailedGraphNames.Add(Item.TagInfo.GraphName);

		FN2CLogger::Get().LogError(
			FString::Printf(TEXT("Translation failed for: %s"), *Item.TagInfo.GraphName),
			TEXT("BatchOrchestrator"));
	}

	// Broadcast per-item completion
	OnItemComplete.Broadcast(Item.TagInfo, Response, bSuccess, CurrentItemIndex, BatchItems.Num());

	// Continue to next item
	ProcessNextItem();
}

void UN2CBatchTranslationOrchestrator::FinalizeBatch()
{
	// Calculate final timing
	CurrentResult.TotalTimeSeconds = static_cast<float>(FPlatformTime::Seconds() - BatchStartTime);
	CurrentResult.BatchOutputPath = BatchOutputPath;

	// Generate batch summary
	GenerateBatchSummary();

	FN2CLogger::Get().Log(
		FString::Printf(TEXT("Batch translation complete. Success: %d, Failed: %d, Skipped: %d, Time: %.2fs"),
			CurrentResult.SuccessCount, CurrentResult.FailureCount, CurrentResult.SkippedCount, CurrentResult.TotalTimeSeconds),
		EN2CLogSeverity::Info, TEXT("BatchOrchestrator"));

	// Broadcast batch completion
	OnBatchComplete.Broadcast(CurrentResult);

	// Clean up
	CleanupBatch();
}

void UN2CBatchTranslationOrchestrator::CleanupBatch()
{
	// Unbind from LLM module delegate
	UN2CLLMModule* LLMModule = UN2CLLMModule::Get();
	if (LLMModule && bIsBoundToLLMModule)
	{
		LLMModule->OnTranslationResponseReceived.RemoveDynamic(
			this, &UN2CBatchTranslationOrchestrator::HandleTranslationResponse);
		bIsBoundToLLMModule = false;
	}

	// Clear blueprint cache
	BlueprintCache.Empty();

	// Reset state
	bBatchInProgress = false;
	bCancellationRequested = false;
	CurrentItemIndex = -1;
}

bool UN2CBatchTranslationOrchestrator::ResolveBlueprintsAndGraphs()
{
	bool bAnyResolved = false;

	for (FN2CBatchTranslationItem& Item : BatchItems)
	{
		// Load blueprint (using cache if already loaded)
		UBlueprint* Blueprint = nullptr;
		if (TWeakObjectPtr<UBlueprint>* CachedBP = BlueprintCache.Find(Item.TagInfo.BlueprintPath))
		{
			Blueprint = CachedBP->Get();
		}
		else
		{
			Blueprint = LoadBlueprintFromPath(Item.TagInfo.BlueprintPath);
			if (Blueprint)
			{
				BlueprintCache.Add(Item.TagInfo.BlueprintPath, Blueprint);
			}
		}

		if (!Blueprint)
		{
			Item.Status = EN2CBatchItemStatus::Failed;
			Item.ErrorMessage = FString::Printf(TEXT("Failed to load blueprint: %s"), *Item.TagInfo.BlueprintPath);
			CurrentResult.FailureCount++;
			CurrentResult.FailedGraphNames.Add(Item.TagInfo.GraphName);
			FN2CLogger::Get().LogError(Item.ErrorMessage, TEXT("BatchOrchestrator"));
			continue;
		}

		Item.CachedBlueprint = Blueprint;

		// Parse and find graph by GUID
		FGuid GraphGuid;
		if (!FGuid::Parse(Item.TagInfo.GraphGuid, GraphGuid) || !GraphGuid.IsValid())
		{
			Item.Status = EN2CBatchItemStatus::Failed;
			Item.ErrorMessage = FString::Printf(TEXT("Invalid graph GUID: %s"), *Item.TagInfo.GraphGuid);
			CurrentResult.FailureCount++;
			CurrentResult.FailedGraphNames.Add(Item.TagInfo.GraphName);
			FN2CLogger::Get().LogError(Item.ErrorMessage, TEXT("BatchOrchestrator"));
			continue;
		}

		UEdGraph* Graph = FindGraphByGuid(Blueprint, GraphGuid);
		if (!Graph)
		{
			Item.Status = EN2CBatchItemStatus::Failed;
			Item.ErrorMessage = FString::Printf(TEXT("Graph not found with GUID: %s"), *Item.TagInfo.GraphGuid);
			CurrentResult.FailureCount++;
			CurrentResult.FailedGraphNames.Add(Item.TagInfo.GraphName);
			FN2CLogger::Get().LogError(Item.ErrorMessage, TEXT("BatchOrchestrator"));
			continue;
		}

		Item.CachedGraph = Graph;
		bAnyResolved = true;

		FN2CLogger::Get().Log(
			FString::Printf(TEXT("Resolved graph '%s' in blueprint '%s'"), *Item.TagInfo.GraphName, *Item.TagInfo.BlueprintPath),
			EN2CLogSeverity::Info, TEXT("BatchOrchestrator"));
	}

	return bAnyResolved;
}

UBlueprint* UN2CBatchTranslationOrchestrator::LoadBlueprintFromPath(const FString& BlueprintPath)
{
	FSoftObjectPath SoftPath(BlueprintPath);
	UObject* LoadedObject = SoftPath.TryLoad();

	if (!LoadedObject)
	{
		FN2CLogger::Get().LogError(
			FString::Printf(TEXT("Failed to load object at path: %s"), *BlueprintPath),
			TEXT("BatchOrchestrator"));
		return nullptr;
	}

	UBlueprint* Blueprint = Cast<UBlueprint>(LoadedObject);
	if (!Blueprint)
	{
		FN2CLogger::Get().LogError(
			FString::Printf(TEXT("Object at path is not a Blueprint: %s"), *BlueprintPath),
			TEXT("BatchOrchestrator"));
		return nullptr;
	}

	return Blueprint;
}

UEdGraph* UN2CBatchTranslationOrchestrator::FindGraphByGuid(UBlueprint* Blueprint, const FGuid& GraphGuid)
{
	if (!Blueprint || !GraphGuid.IsValid())
	{
		return nullptr;
	}

	// Search in UbergraphPages (event graphs)
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (Graph && Graph->GraphGuid == GraphGuid)
		{
			return Graph;
		}
	}

	// Search in FunctionGraphs
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GraphGuid == GraphGuid)
		{
			return Graph;
		}
	}

	// Search in MacroGraphs
	for (UEdGraph* Graph : Blueprint->MacroGraphs)
	{
		if (Graph && Graph->GraphGuid == GraphGuid)
		{
			return Graph;
		}
	}

	// Search in DelegateSignatureGraphs
	for (UEdGraph* Graph : Blueprint->DelegateSignatureGraphs)
	{
		if (Graph && Graph->GraphGuid == GraphGuid)
		{
			return Graph;
		}
	}

	return nullptr;
}

FString UN2CBatchTranslationOrchestrator::CollectAndSerializeGraph(UEdGraph* Graph, UBlueprint* Blueprint)
{
	if (!Graph || !Blueprint)
	{
		return FString();
	}

	// Collect nodes from the graph
	TArray<UK2Node*> CollectedNodes;
	if (!FN2CNodeCollector::Get().CollectNodesFromGraph(Graph, CollectedNodes))
	{
		FN2CLogger::Get().LogError(TEXT("Failed to collect nodes from graph"), TEXT("BatchOrchestrator"));
		return FString();
	}

	if (CollectedNodes.IsEmpty())
	{
		FN2CLogger::Get().LogWarning(TEXT("No nodes collected from graph"), TEXT("BatchOrchestrator"));
		return FString();
	}

	// Translate to N2CBlueprint structure
	FN2CNodeTranslator& Translator = FN2CNodeTranslator::Get();
	if (!Translator.GenerateN2CStruct(CollectedNodes))
	{
		FN2CLogger::Get().LogError(TEXT("Failed to generate N2CStruct"), TEXT("BatchOrchestrator"));
		return FString();
	}

	// Get the result and serialize to JSON
	const FN2CBlueprint& N2CBlueprintData = Translator.GetN2CBlueprint();

	// Serialize to JSON (minified for LLM)
	FN2CSerializer::SetPrettyPrint(false);
	FString JsonOutput = FN2CSerializer::ToJson(N2CBlueprintData);

	return JsonOutput;
}

void UN2CBatchTranslationOrchestrator::CreateBatchOutputDirectory()
{
	// Get current timestamp
	FDateTime Now = FDateTime::Now();
	FString Timestamp = Now.ToString(TEXT("%Y-%m-%d-%H.%M.%S"));

	// Create folder name
	FString FolderName = FString::Printf(TEXT("BatchTranslation_%s"), *Timestamp);

	// Get base path
	FString BasePath = GetTranslationBasePath();

	BatchOutputPath = FPaths::Combine(BasePath, FolderName);

	if (!EnsureDirectoryExists(BatchOutputPath))
	{
		FN2CLogger::Get().LogError(
			FString::Printf(TEXT("Failed to create batch output directory: %s"), *BatchOutputPath),
			TEXT("BatchOrchestrator"));
	}
	else
	{
		FN2CLogger::Get().Log(
			FString::Printf(TEXT("Created batch output directory: %s"), *BatchOutputPath),
			EN2CLogSeverity::Info, TEXT("BatchOrchestrator"));
	}
}

void UN2CBatchTranslationOrchestrator::SaveItemTranslation(const FN2CBatchTranslationItem& Item)
{
	if (BatchOutputPath.IsEmpty())
	{
		return;
	}

	const UN2CSettings* Settings = GetDefault<UN2CSettings>();
	EN2CCodeLanguage TargetLanguage = Settings ? Settings->TargetLanguage : EN2CCodeLanguage::Cpp;

	// Save each graph's files
	for (const FN2CGraphTranslation& Graph : Item.TranslationResponse.Graphs)
	{
		// Use TagInfo graph name for the folder (more reliable)
		FString GraphName = Item.TagInfo.GraphName;
		if (GraphName.IsEmpty())
		{
			GraphName = Graph.GraphName;
		}
		if (GraphName.IsEmpty())
		{
			continue;
		}

		// Create directory for this graph
		FString GraphDir = FPaths::Combine(BatchOutputPath, GraphName);
		if (!EnsureDirectoryExists(GraphDir))
		{
			FN2CLogger::Get().LogWarning(
				FString::Printf(TEXT("Failed to create graph directory: %s"), *GraphDir),
				TEXT("BatchOrchestrator"));
			continue;
		}

		// Save declaration file (C++ only)
		if (TargetLanguage == EN2CCodeLanguage::Cpp && !Graph.Code.GraphDeclaration.IsEmpty())
		{
			FString HeaderPath = FPaths::Combine(GraphDir, GraphName + TEXT(".h"));
			if (!FFileHelper::SaveStringToFile(Graph.Code.GraphDeclaration, *HeaderPath))
			{
				FN2CLogger::Get().LogWarning(
					FString::Printf(TEXT("Failed to save header file: %s"), *HeaderPath),
					TEXT("BatchOrchestrator"));
			}
		}

		// Save implementation file with appropriate extension
		if (!Graph.Code.GraphImplementation.IsEmpty())
		{
			FString Extension = GetFileExtensionForLanguage();
			FString ImplPath = FPaths::Combine(GraphDir, GraphName + Extension);
			if (!FFileHelper::SaveStringToFile(Graph.Code.GraphImplementation, *ImplPath))
			{
				FN2CLogger::Get().LogWarning(
					FString::Printf(TEXT("Failed to save implementation file: %s"), *ImplPath),
					TEXT("BatchOrchestrator"));
			}
		}

		// Save implementation notes
		if (!Graph.Code.ImplementationNotes.IsEmpty())
		{
			FString NotesPath = FPaths::Combine(GraphDir, GraphName + TEXT("_Notes.txt"));
			if (!FFileHelper::SaveStringToFile(Graph.Code.ImplementationNotes, *NotesPath))
			{
				FN2CLogger::Get().LogWarning(
					FString::Printf(TEXT("Failed to save notes file: %s"), *NotesPath),
					TEXT("BatchOrchestrator"));
			}
		}
	}
}

void UN2CBatchTranslationOrchestrator::GenerateBatchSummary()
{
	if (BatchOutputPath.IsEmpty())
	{
		return;
	}

	// Get settings for metadata
	const UN2CSettings* Settings = GetDefault<UN2CSettings>();

	// Build summary JSON
	TSharedPtr<FJsonObject> SummaryJson = MakeShared<FJsonObject>();

	SummaryJson->SetStringField(TEXT("version"), TEXT("1.0"));
	SummaryJson->SetStringField(TEXT("timestamp"), FDateTime::Now().ToIso8601());

	// Statistics
	TSharedPtr<FJsonObject> StatsJson = MakeShared<FJsonObject>();
	StatsJson->SetNumberField(TEXT("total"), CurrentResult.TotalCount);
	StatsJson->SetNumberField(TEXT("successful"), CurrentResult.SuccessCount);
	StatsJson->SetNumberField(TEXT("failed"), CurrentResult.FailureCount);
	StatsJson->SetNumberField(TEXT("skipped"), CurrentResult.SkippedCount);
	StatsJson->SetNumberField(TEXT("durationSeconds"), CurrentResult.TotalTimeSeconds);
	StatsJson->SetNumberField(TEXT("totalInputTokens"), CurrentResult.TotalInputTokens);
	StatsJson->SetNumberField(TEXT("totalOutputTokens"), CurrentResult.TotalOutputTokens);
	SummaryJson->SetObjectField(TEXT("statistics"), StatsJson);

	// Settings
	TSharedPtr<FJsonObject> SettingsJson = MakeShared<FJsonObject>();
	if (Settings)
	{
		SettingsJson->SetStringField(TEXT("provider"), UEnum::GetValueAsString(Settings->Provider));
		SettingsJson->SetStringField(TEXT("model"), Settings->GetActiveModel());
		SettingsJson->SetStringField(TEXT("language"), UEnum::GetValueAsString(Settings->TargetLanguage));
	}
	SummaryJson->SetObjectField(TEXT("settings"), SettingsJson);

	// Items
	TArray<TSharedPtr<FJsonValue>> ItemsArray;
	for (const FN2CBatchTranslationItem& Item : BatchItems)
	{
		TSharedPtr<FJsonObject> ItemJson = MakeShared<FJsonObject>();
		ItemJson->SetStringField(TEXT("graphName"), Item.TagInfo.GraphName);
		ItemJson->SetStringField(TEXT("blueprintPath"), Item.TagInfo.BlueprintPath);
		ItemJson->SetStringField(TEXT("tag"), Item.TagInfo.Tag);
		ItemJson->SetStringField(TEXT("category"), Item.TagInfo.Category);
		ItemJson->SetStringField(TEXT("status"), UEnum::GetValueAsString(Item.Status));

		if (Item.Status == EN2CBatchItemStatus::Completed)
		{
			TSharedPtr<FJsonObject> TokensJson = MakeShared<FJsonObject>();
			TokensJson->SetNumberField(TEXT("input"), Item.TranslationResponse.Usage.InputTokens);
			TokensJson->SetNumberField(TEXT("output"), Item.TranslationResponse.Usage.OutputTokens);
			ItemJson->SetObjectField(TEXT("tokensUsed"), TokensJson);
		}

		if (!Item.ErrorMessage.IsEmpty())
		{
			ItemJson->SetStringField(TEXT("error"), Item.ErrorMessage);
		}

		ItemsArray.Add(MakeShared<FJsonValueObject>(ItemJson));
	}
	SummaryJson->SetArrayField(TEXT("items"), ItemsArray);

	// Serialize and save
	FString SummaryContent;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&SummaryContent);
	FJsonSerializer::Serialize(SummaryJson.ToSharedRef(), Writer);

	FString SummaryPath = FPaths::Combine(BatchOutputPath, TEXT("BatchSummary.json"));
	if (!FFileHelper::SaveStringToFile(SummaryContent, *SummaryPath))
	{
		FN2CLogger::Get().LogWarning(
			FString::Printf(TEXT("Failed to save batch summary: %s"), *SummaryPath),
			TEXT("BatchOrchestrator"));
	}
}

FString UN2CBatchTranslationOrchestrator::GetTranslationBasePath() const
{
	const UN2CSettings* Settings = GetDefault<UN2CSettings>();
	FString BasePath;

	if (Settings && !Settings->CustomTranslationOutputDirectory.Path.IsEmpty())
	{
		BasePath = Settings->CustomTranslationOutputDirectory.Path;
	}
	else
	{
		BasePath = FPaths::ProjectSavedDir() / TEXT("NodeToCode") / TEXT("Translations");
	}

	return BasePath;
}

FString UN2CBatchTranslationOrchestrator::GetFileExtensionForLanguage() const
{
	const UN2CSettings* Settings = GetDefault<UN2CSettings>();
	EN2CCodeLanguage Language = Settings ? Settings->TargetLanguage : EN2CCodeLanguage::Cpp;

	switch (Language)
	{
		case EN2CCodeLanguage::Cpp:
			return TEXT(".cpp");
		case EN2CCodeLanguage::Python:
			return TEXT(".py");
		case EN2CCodeLanguage::JavaScript:
			return TEXT(".js");
		case EN2CCodeLanguage::CSharp:
			return TEXT(".cs");
		case EN2CCodeLanguage::Swift:
			return TEXT(".swift");
		case EN2CCodeLanguage::Pseudocode:
			return TEXT(".md");
		default:
			return TEXT(".txt");
	}
}

bool UN2CBatchTranslationOrchestrator::EnsureDirectoryExists(const FString& DirectoryPath) const
{
	if (!FPaths::DirectoryExists(DirectoryPath))
	{
		return FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*DirectoryPath);
	}
	return true;
}
