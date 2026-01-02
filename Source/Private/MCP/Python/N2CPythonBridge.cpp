// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#include "N2CPythonBridge.h"
#include "Core/N2CEditorIntegration.h"
#include "Core/N2CSettings.h"
#include "Core/N2CTagManager.h"
#include "MCP/Utils/N2CMcpBlueprintUtils.h"
#include "Models/N2CBlueprint.h"
#include "Models/N2CTaggedBlueprintGraph.h"
#include "Utils/N2CLogger.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Editor.h"
#include "EditorScriptingUtilities/Public/EditorAssetLibrary.h"

FString UN2CPythonBridge::GetFocusedBlueprintJson()
{
	// Get the focused graph
	UBlueprint* OwningBlueprint = nullptr;
	UEdGraph* FocusedGraph = nullptr;
	FString GraphError;

	if (!FN2CMcpBlueprintUtils::GetFocusedEditorGraph(OwningBlueprint, FocusedGraph, GraphError))
	{
		FN2CLogger::Get().LogWarning(FString::Printf(TEXT("GetFocusedBlueprintJson failed: %s"), *GraphError));
		return MakeErrorJson(GraphError);
	}

	// Collect nodes
	TArray<UK2Node*> CollectedNodes;
	if (!FN2CEditorIntegration::Get().CollectNodesFromGraph(FocusedGraph, CollectedNodes) || CollectedNodes.IsEmpty())
	{
		return MakeErrorJson(TEXT("Failed to collect nodes or no nodes found in the focused graph."));
	}

	// Translate nodes
	FN2CBlueprint N2CBlueprintData;
	TMap<FGuid, FString> NodeIDMapCopy;
	TMap<FGuid, FString> PinIDMapCopy;

	if (!FN2CEditorIntegration::Get().TranslateNodesToN2CBlueprintWithMaps(CollectedNodes, N2CBlueprintData, NodeIDMapCopy, PinIDMapCopy))
	{
		return MakeErrorJson(TEXT("Failed to translate collected nodes into N2CBlueprint structure."));
	}

	// Serialize to JSON
	FString JsonOutput = FN2CEditorIntegration::Get().SerializeN2CBlueprintToJson(N2CBlueprintData, false);
	if (JsonOutput.IsEmpty())
	{
		return MakeErrorJson(TEXT("Failed to serialize N2CBlueprint to JSON."));
	}

	// Build metadata response
	FString BlueprintName = OwningBlueprint ? OwningBlueprint->GetName() : TEXT("Unknown");
	FString BlueprintPath = OwningBlueprint ? OwningBlueprint->GetPathName() : TEXT("");
	FString GraphName = FocusedGraph ? FocusedGraph->GetName() : TEXT("Unknown");

	// Create a simplified metadata response along with the full N2CJSON
	FString DataJson = FString::Printf(TEXT(
		"{"
		"\"name\": \"%s\","
		"\"path\": \"%s\","
		"\"graph_name\": \"%s\","
		"\"node_count\": %d,"
		"\"n2c_json\": %s"
		"}"
	),
		*BlueprintName.ReplaceCharWithEscapedChar(),
		*BlueprintPath.ReplaceCharWithEscapedChar(),
		*GraphName.ReplaceCharWithEscapedChar(),
		CollectedNodes.Num(),
		*JsonOutput
	);

	FN2CLogger::Get().Log(FString::Printf(TEXT("GetFocusedBlueprintJson: Retrieved %s with %d nodes"), *BlueprintName, CollectedNodes.Num()), EN2CLogSeverity::Info);
	return MakeSuccessJson(DataJson);
}

FString UN2CPythonBridge::CompileFocusedBlueprint()
{
	// Get the focused Blueprint
	UBlueprint* OwningBlueprint = nullptr;
	UEdGraph* FocusedGraph = nullptr;
	FString GraphError;

	if (!FN2CMcpBlueprintUtils::GetFocusedEditorGraph(OwningBlueprint, FocusedGraph, GraphError))
	{
		return MakeErrorJson(GraphError);
	}

	if (!OwningBlueprint)
	{
		return MakeErrorJson(TEXT("No Blueprint found to compile."));
	}

	FN2CLogger::Get().Log(FString::Printf(TEXT("CompileFocusedBlueprint: Compiling %s"), *OwningBlueprint->GetName()), EN2CLogSeverity::Info);

	// Compile the Blueprint
	FKismetEditorUtilities::CompileBlueprint(OwningBlueprint);

	// Get status after compilation
	FString StatusStr;
	bool bHadErrors = false;
	bool bHadWarnings = false;

	switch (OwningBlueprint->Status)
	{
	case BS_Unknown:
		StatusStr = TEXT("Unknown");
		break;
	case BS_Dirty:
		StatusStr = TEXT("Dirty");
		break;
	case BS_Error:
		StatusStr = TEXT("Error");
		bHadErrors = true;
		break;
	case BS_UpToDate:
		StatusStr = TEXT("UpToDate");
		break;
	case BS_BeingCreated:
		StatusStr = TEXT("BeingCreated");
		break;
	case BS_UpToDateWithWarnings:
		StatusStr = TEXT("UpToDateWithWarnings");
		bHadWarnings = true;
		break;
	default:
		StatusStr = TEXT("Unknown");
		break;
	}

	FString DataJson = FString::Printf(TEXT(
		"{"
		"\"blueprint_name\": \"%s\","
		"\"status\": \"%s\","
		"\"had_errors\": %s,"
		"\"had_warnings\": %s"
		"}"
	),
		*OwningBlueprint->GetName().ReplaceCharWithEscapedChar(),
		*StatusStr,
		bHadErrors ? TEXT("true") : TEXT("false"),
		bHadWarnings ? TEXT("true") : TEXT("false")
	);

	if (bHadErrors)
	{
		return FString::Printf(TEXT("{\"success\": false, \"data\": %s, \"error\": \"Compilation failed with errors\"}"), *DataJson);
	}

	return MakeSuccessJson(DataJson);
}

FString UN2CPythonBridge::SaveFocusedBlueprint(bool bOnlyIfDirty)
{
	// Get the focused Blueprint
	UBlueprint* OwningBlueprint = nullptr;
	UEdGraph* FocusedGraph = nullptr;
	FString GraphError;

	if (!FN2CMcpBlueprintUtils::GetFocusedEditorGraph(OwningBlueprint, FocusedGraph, GraphError))
	{
		return MakeErrorJson(GraphError);
	}

	if (!OwningBlueprint)
	{
		return MakeErrorJson(TEXT("No Blueprint found to save."));
	}

	// Check if dirty
	UPackage* Package = OwningBlueprint->GetOutermost();
	bool bWasDirty = Package ? Package->IsDirty() : false;

	// Skip if not dirty and only_if_dirty is true
	if (bOnlyIfDirty && !bWasDirty)
	{
		FString DataJson = FString::Printf(TEXT(
			"{"
			"\"blueprint_name\": \"%s\","
			"\"was_dirty\": false,"
			"\"was_saved\": false"
			"}"
		), *OwningBlueprint->GetName().ReplaceCharWithEscapedChar());

		return MakeSuccessJson(DataJson);
	}

	FN2CLogger::Get().Log(FString::Printf(TEXT("SaveFocusedBlueprint: Saving %s"), *OwningBlueprint->GetName()), EN2CLogSeverity::Info);

	// Save the asset
	FString AssetPath = OwningBlueprint->GetPathName();
	bool bSaved = UEditorAssetLibrary::SaveAsset(AssetPath, bOnlyIfDirty);

	FString DataJson = FString::Printf(TEXT(
		"{"
		"\"blueprint_name\": \"%s\","
		"\"was_dirty\": %s,"
		"\"was_saved\": %s"
		"}"
	),
		*OwningBlueprint->GetName().ReplaceCharWithEscapedChar(),
		bWasDirty ? TEXT("true") : TEXT("false"),
		bSaved ? TEXT("true") : TEXT("false")
	);

	if (!bSaved && bWasDirty)
	{
		return FString::Printf(TEXT("{\"success\": false, \"data\": %s, \"error\": \"Failed to save Blueprint\"}"), *DataJson);
	}

	return MakeSuccessJson(DataJson);
}

// ========== Tagging System ==========

FString UN2CPythonBridge::TagFocusedGraph(const FString& Tag, const FString& Category, const FString& Description)
{
	if (Tag.IsEmpty())
	{
		return MakeErrorJson(TEXT("Tag name cannot be empty"));
	}

	// Get the focused graph
	UBlueprint* OwningBlueprint = nullptr;
	UEdGraph* FocusedGraph = nullptr;
	FString GraphError;

	if (!FN2CMcpBlueprintUtils::GetFocusedEditorGraph(OwningBlueprint, FocusedGraph, GraphError))
	{
		return MakeErrorJson(GraphError);
	}

	if (!FocusedGraph || !OwningBlueprint)
	{
		return MakeErrorJson(TEXT("No focused graph or Blueprint found"));
	}

	// Create the tag info
	FN2CTaggedBlueprintGraph TaggedGraph(
		Tag,
		Category.IsEmpty() ? TEXT("Default") : Category,
		Description,
		FocusedGraph->GraphGuid,
		FocusedGraph->GetName(),
		FSoftObjectPath(OwningBlueprint)
	);

	// Add the tag
	if (!UN2CTagManager::Get().AddTag(TaggedGraph))
	{
		return MakeErrorJson(TEXT("Failed to add tag - tag may already exist on this graph"));
	}

	// Save tags to persist
	UN2CTagManager::Get().SaveTags();

	FN2CLogger::Get().Log(FString::Printf(TEXT("TagFocusedGraph: Added tag '%s' to %s"), *Tag, *FocusedGraph->GetName()), EN2CLogSeverity::Info);

	FString DataJson = FString::Printf(TEXT(
		"{"
		"\"tag\": \"%s\","
		"\"category\": \"%s\","
		"\"description\": \"%s\","
		"\"graph_guid\": \"%s\","
		"\"graph_name\": \"%s\","
		"\"blueprint_name\": \"%s\""
		"}"
	),
		*Tag.ReplaceCharWithEscapedChar(),
		*TaggedGraph.Category.ReplaceCharWithEscapedChar(),
		*Description.ReplaceCharWithEscapedChar(),
		*FocusedGraph->GraphGuid.ToString(),
		*FocusedGraph->GetName().ReplaceCharWithEscapedChar(),
		*OwningBlueprint->GetName().ReplaceCharWithEscapedChar()
	);

	return MakeSuccessJson(DataJson);
}

FString UN2CPythonBridge::ListTags(const FString& Category, const FString& Tag)
{
	TArray<FN2CTaggedBlueprintGraph> Tags;

	// Get filtered or all tags
	if (!Category.IsEmpty() && !Tag.IsEmpty())
	{
		// Filter by both
		Tags = UN2CTagManager::Get().GetGraphsWithTag(Tag, Category);
	}
	else if (!Category.IsEmpty())
	{
		// Filter by category only
		Tags = UN2CTagManager::Get().GetTagsInCategory(Category);
	}
	else if (!Tag.IsEmpty())
	{
		// Filter by tag only (all categories)
		Tags = UN2CTagManager::Get().GetGraphsWithTag(Tag, TEXT(""));
	}
	else
	{
		// Get all tags
		Tags = UN2CTagManager::Get().GetAllTags();
	}

	// Build JSON array
	FString TagsArrayJson = TEXT("[");
	for (int32 i = 0; i < Tags.Num(); ++i)
	{
		const FN2CTaggedBlueprintGraph& TagInfo = Tags[i];

		if (i > 0)
		{
			TagsArrayJson += TEXT(",");
		}

		TagsArrayJson += FString::Printf(TEXT(
			"{"
			"\"tag\": \"%s\","
			"\"category\": \"%s\","
			"\"description\": \"%s\","
			"\"graph_guid\": \"%s\","
			"\"graph_name\": \"%s\","
			"\"blueprint_path\": \"%s\","
			"\"timestamp\": \"%s\""
			"}"
		),
			*TagInfo.Tag.ReplaceCharWithEscapedChar(),
			*TagInfo.Category.ReplaceCharWithEscapedChar(),
			*TagInfo.Description.ReplaceCharWithEscapedChar(),
			*TagInfo.GraphGuid.ToString(),
			*TagInfo.GraphName.ReplaceCharWithEscapedChar(),
			*TagInfo.OwningBlueprint.ToString().ReplaceCharWithEscapedChar(),
			*TagInfo.Timestamp.ToString()
		);
	}
	TagsArrayJson += TEXT("]");

	// Get summary info
	TArray<FString> AllCategories = UN2CTagManager::Get().GetAllCategories();
	TArray<FString> AllTagNames = UN2CTagManager::Get().GetAllTagNames();

	FString DataJson = FString::Printf(TEXT(
		"{"
		"\"tags\": %s,"
		"\"count\": %d,"
		"\"total_categories\": %d,"
		"\"total_unique_tags\": %d"
		"}"
	),
		*TagsArrayJson,
		Tags.Num(),
		AllCategories.Num(),
		AllTagNames.Num()
	);

	return MakeSuccessJson(DataJson);
}

FString UN2CPythonBridge::RemoveTag(const FString& GraphGuid, const FString& Tag)
{
	if (GraphGuid.IsEmpty() || Tag.IsEmpty())
	{
		return MakeErrorJson(TEXT("GraphGuid and Tag cannot be empty"));
	}

	// Parse GUID
	FGuid ParsedGuid;
	if (!FGuid::Parse(GraphGuid, ParsedGuid))
	{
		return MakeErrorJson(FString::Printf(TEXT("Invalid GUID format: %s"), *GraphGuid));
	}

	// Try to remove the tag
	FN2CTaggedBlueprintGraph RemovedTag;
	int32 RemovedCount = UN2CTagManager::Get().RemoveTagByName(ParsedGuid, Tag, RemovedTag);

	if (RemovedCount == 0)
	{
		return MakeErrorJson(FString::Printf(TEXT("Tag '%s' not found on graph %s"), *Tag, *GraphGuid));
	}

	// Save tags to persist
	UN2CTagManager::Get().SaveTags();

	// Get remaining tags for this graph
	TArray<FN2CTaggedBlueprintGraph> RemainingTags = UN2CTagManager::Get().GetTagsForGraph(ParsedGuid);

	FN2CLogger::Get().Log(FString::Printf(TEXT("RemoveTag: Removed %d tag(s) '%s' from graph %s"), RemovedCount, *Tag, *GraphGuid), EN2CLogSeverity::Info);

	FString DataJson = FString::Printf(TEXT(
		"{"
		"\"removed\": true,"
		"\"removed_count\": %d,"
		"\"tag\": \"%s\","
		"\"graph_guid\": \"%s\","
		"\"remaining_tags\": %d"
		"}"
	),
		RemovedCount,
		*Tag.ReplaceCharWithEscapedChar(),
		*GraphGuid.ReplaceCharWithEscapedChar(),
		RemainingTags.Num()
	);

	return MakeSuccessJson(DataJson);
}

// ========== LLM Provider Info ==========

FString UN2CPythonBridge::GetLLMProviders()
{
	const UN2CSettings* Settings = GetDefault<UN2CSettings>();
	if (!Settings)
	{
		return MakeErrorJson(TEXT("Failed to get NodeToCode settings"));
	}

	// Build providers array
	FString ProvidersJson = TEXT("[");

	// Define provider info (matching EN2CLLMProvider enum)
	struct FProviderInfo
	{
		FString Name;
		FString DisplayName;
		bool bIsLocal;
	};

	TArray<FProviderInfo> Providers = {
		{TEXT("OpenAI"), TEXT("OpenAI"), false},
		{TEXT("Anthropic"), TEXT("Anthropic"), false},
		{TEXT("Gemini"), TEXT("Google Gemini"), false},
		{TEXT("Ollama"), TEXT("Ollama (Local)"), true},
		{TEXT("DeepSeek"), TEXT("DeepSeek"), false},
		{TEXT("LMStudio"), TEXT("LM Studio (Local)"), true}
	};

	EN2CLLMProvider CurrentProvider = Settings->Provider;

	for (int32 i = 0; i < Providers.Num(); ++i)
	{
		if (i > 0)
		{
			ProvidersJson += TEXT(",");
		}

		bool bIsCurrent = (i == static_cast<int32>(CurrentProvider));

		ProvidersJson += FString::Printf(TEXT(
			"{"
			"\"name\": \"%s\","
			"\"display_name\": \"%s\","
			"\"is_local\": %s,"
			"\"is_current\": %s"
			"}"
		),
			*Providers[i].Name,
			*Providers[i].DisplayName,
			Providers[i].bIsLocal ? TEXT("true") : TEXT("false"),
			bIsCurrent ? TEXT("true") : TEXT("false")
		);
	}
	ProvidersJson += TEXT("]");

	// Get current provider name
	FString CurrentProviderName;
	switch (CurrentProvider)
	{
	case EN2CLLMProvider::OpenAI: CurrentProviderName = TEXT("OpenAI"); break;
	case EN2CLLMProvider::Anthropic: CurrentProviderName = TEXT("Anthropic"); break;
	case EN2CLLMProvider::Gemini: CurrentProviderName = TEXT("Gemini"); break;
	case EN2CLLMProvider::Ollama: CurrentProviderName = TEXT("Ollama"); break;
	case EN2CLLMProvider::DeepSeek: CurrentProviderName = TEXT("DeepSeek"); break;
	case EN2CLLMProvider::LMStudio: CurrentProviderName = TEXT("LMStudio"); break;
	default: CurrentProviderName = TEXT("Unknown"); break;
	}

	FString DataJson = FString::Printf(TEXT(
		"{"
		"\"providers\": %s,"
		"\"current_provider\": \"%s\","
		"\"provider_count\": %d"
		"}"
	),
		*ProvidersJson,
		*CurrentProviderName,
		Providers.Num()
	);

	return MakeSuccessJson(DataJson);
}

FString UN2CPythonBridge::GetActiveProvider()
{
	const UN2CSettings* Settings = GetDefault<UN2CSettings>();
	if (!Settings)
	{
		return MakeErrorJson(TEXT("Failed to get NodeToCode settings"));
	}

	EN2CLLMProvider CurrentProvider = Settings->Provider;

	FString ProviderName;
	FString DisplayName;
	FString Model;
	FString Endpoint;
	bool bIsLocal = false;

	switch (CurrentProvider)
	{
	case EN2CLLMProvider::OpenAI:
		ProviderName = TEXT("OpenAI");
		DisplayName = TEXT("OpenAI");
		Model = Settings->GetActiveModel();
		Endpoint = TEXT("https://api.openai.com/v1/chat/completions");
		break;

	case EN2CLLMProvider::Anthropic:
		ProviderName = TEXT("Anthropic");
		DisplayName = TEXT("Anthropic");
		Model = Settings->GetActiveModel();
		Endpoint = TEXT("https://api.anthropic.com/v1/messages");
		break;

	case EN2CLLMProvider::Gemini:
		ProviderName = TEXT("Gemini");
		DisplayName = TEXT("Google Gemini");
		Model = Settings->GetActiveModel();
		Endpoint = TEXT("https://generativelanguage.googleapis.com/v1beta/models");
		break;

	case EN2CLLMProvider::Ollama:
		ProviderName = TEXT("Ollama");
		DisplayName = TEXT("Ollama (Local)");
		Model = Settings->OllamaModel;
		Endpoint = Settings->OllamaConfig.OllamaEndpoint.IsEmpty() ? TEXT("http://localhost:11434/api/chat") : Settings->OllamaConfig.OllamaEndpoint;
		bIsLocal = true;
		break;

	case EN2CLLMProvider::DeepSeek:
		ProviderName = TEXT("DeepSeek");
		DisplayName = TEXT("DeepSeek");
		Model = Settings->GetActiveModel();
		Endpoint = TEXT("https://api.deepseek.com/v1/chat/completions");
		break;

	case EN2CLLMProvider::LMStudio:
		ProviderName = TEXT("LMStudio");
		DisplayName = TEXT("LM Studio (Local)");
		Model = Settings->LMStudioModel;
		Endpoint = Settings->LMStudioEndpoint.IsEmpty() ? TEXT("http://localhost:1234/v1/chat/completions") : Settings->LMStudioEndpoint;
		bIsLocal = true;
		break;

	default:
		ProviderName = TEXT("Unknown");
		DisplayName = TEXT("Unknown");
		break;
	}

	FString DataJson = FString::Printf(TEXT(
		"{"
		"\"name\": \"%s\","
		"\"display_name\": \"%s\","
		"\"model\": \"%s\","
		"\"endpoint\": \"%s\","
		"\"is_local\": %s"
		"}"
	),
		*ProviderName.ReplaceCharWithEscapedChar(),
		*DisplayName.ReplaceCharWithEscapedChar(),
		*Model.ReplaceCharWithEscapedChar(),
		*Endpoint.ReplaceCharWithEscapedChar(),
		bIsLocal ? TEXT("true") : TEXT("false")
	);

	return MakeSuccessJson(DataJson);
}

FString UN2CPythonBridge::MakeSuccessJson(const FString& DataJson)
{
	return FString::Printf(TEXT("{\"success\": true, \"data\": %s, \"error\": null}"), *DataJson);
}

FString UN2CPythonBridge::MakeErrorJson(const FString& ErrorMessage)
{
	return FString::Printf(TEXT("{\"success\": false, \"data\": null, \"error\": \"%s\"}"),
		*ErrorMessage.ReplaceCharWithEscapedChar());
}
