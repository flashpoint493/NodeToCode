// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#include "N2CMcpTagBlueprintGraphTool.h"
#include "MCP/Tools/N2CMcpToolRegistry.h"
#include "MCP/Utils/N2CMcpBlueprintUtils.h"
#include "Core/N2CTagManager.h"
#include "Core/N2CEditorIntegration.h"
#include "Utils/N2CLogger.h"
#include "Models/N2CTaggedBlueprintGraph.h"
#include "Engine/Blueprint.h"
#include "BlueprintEditorModule.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// Auto-register this tool
REGISTER_MCP_TOOL(FN2CMcpTagBlueprintGraphTool)

FMcpToolDefinition FN2CMcpTagBlueprintGraphTool::GetDefinition() const
{
	FMcpToolDefinition Definition(
		TEXT("tag-blueprint-graph"),
		TEXT("Tags the currently focused Blueprint graph with a name and category for organization and tracking")
	);
	
	// Define input schema
	TMap<FString, FString> Properties;
	Properties.Add(TEXT("tag"), TEXT("string"));
	Properties.Add(TEXT("category"), TEXT("string"));
	Properties.Add(TEXT("description"), TEXT("string"));
	
	TArray<FString> Required;
	Required.Add(TEXT("tag")); // Only tag is required
	
	Definition.InputSchema = BuildInputSchema(Properties, Required);
	
	return Definition;
}

FMcpToolCallResult FN2CMcpTagBlueprintGraphTool::Execute(const TSharedPtr<FJsonObject>& Arguments)
{
	// Execute on Game Thread since we need to access editor state
	return ExecuteOnGameThread([this, Arguments]() -> FMcpToolCallResult
	{
		// Extract parameters
		FString Tag;
		if (!Arguments->TryGetStringField(TEXT("tag"), Tag) || Tag.IsEmpty())
		{
			return FMcpToolCallResult::CreateErrorResult(TEXT("Missing required parameter: tag"));
		}
		
		FString Category = TEXT("Default");
		Arguments->TryGetStringField(TEXT("category"), Category);
		
		FString Description;
		Arguments->TryGetStringField(TEXT("description"), Description);
		
		// Get the focused graph and owning Blueprint
		UBlueprint* OwningBlueprint = nullptr;
		UEdGraph* FocusedGraph = nullptr;
		FString GraphError;
		
		if (!FN2CMcpBlueprintUtils::GetFocusedEditorGraph(OwningBlueprint, FocusedGraph, GraphError))
		{
			FN2CLogger::Get().LogWarning(FString::Printf(TEXT("tag-blueprint-graph tool failed: %s"), *GraphError));
			return FMcpToolCallResult::CreateErrorResult(GraphError);
		}
		
		// Validate graph has a valid GUID
		if (!FocusedGraph->GraphGuid.IsValid())
		{
			FN2CLogger::Get().LogWarning(TEXT("tag-blueprint-graph tool failed: Current graph does not have a valid GUID"));
			return FMcpToolCallResult::CreateErrorResult(TEXT("Current graph is not valid for tagging"));
		}
		
		// Create the tagged graph struct
		FSoftObjectPath BlueprintPath(OwningBlueprint);
		FN2CTaggedBlueprintGraph TaggedGraph(
			Tag,
			Category,
			Description,
			FocusedGraph->GraphGuid,
			FocusedGraph->GetFName().ToString(),
			BlueprintPath
		);
		
		// Add the tag using the tag manager
		bool bSuccess = UN2CTagManager::Get().AddTag(TaggedGraph);
		
		if (!bSuccess)
		{
			FN2CLogger::Get().LogError(TEXT("Failed to add tag to tag manager"));
			return FMcpToolCallResult::CreateErrorResult(TEXT("Failed to persist tag"));
		}
		
		// Create success response
		TSharedPtr<FJsonObject> ResultObject = MakeShareable(new FJsonObject);
		ResultObject->SetBoolField(TEXT("success"), true);
		
		// Create tagged graph object
		TSharedPtr<FJsonObject> TaggedGraphObject = MakeShareable(new FJsonObject);
		TaggedGraphObject->SetStringField(TEXT("tag"), Tag);
		TaggedGraphObject->SetStringField(TEXT("category"), Category);
		TaggedGraphObject->SetStringField(TEXT("graphGuid"), FocusedGraph->GraphGuid.ToString(EGuidFormats::DigitsWithHyphens));
		TaggedGraphObject->SetStringField(TEXT("graphName"), FocusedGraph->GetFName().ToString());
		TaggedGraphObject->SetStringField(TEXT("blueprintPath"), BlueprintPath.ToString());
		TaggedGraphObject->SetStringField(TEXT("timestamp"), TaggedGraph.Timestamp.ToIso8601());
		
		ResultObject->SetObjectField(TEXT("taggedGraph"), TaggedGraphObject);
		
		FString Message = FString::Printf(TEXT("Successfully tagged %s with '%s'"), 
			*FocusedGraph->GetFName().ToString(), *Tag);
		ResultObject->SetStringField(TEXT("message"), Message);
		
		// Convert to JSON string
		FString JsonString;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
		FJsonSerializer::Serialize(ResultObject.ToSharedRef(), Writer);
		
		FN2CLogger::Get().Log(FString::Printf(TEXT("tag-blueprint-graph tool: %s"), *Message), 
			EN2CLogSeverity::Info);
		
		return FMcpToolCallResult::CreateTextResult(JsonString);
	});
}