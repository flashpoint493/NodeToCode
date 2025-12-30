// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#include "N2CMcpAddBlueprintNodeTool.h"
#include "MCP/Tools/N2CMcpToolRegistry.h"
#include "Core/N2CEditorIntegration.h"
#include "MCP/Utils/N2CMcpBlueprintUtils.h"
#include "MCP/Utils/N2CMcpArgumentParser.h"
#include "Core/N2CNodeTranslator.h"
#include "Core/N2CSerializer.h"
#include "Utils/N2CLogger.h"
#include "BlueprintActionFilter.h"
#include "BlueprintActionMenuBuilder.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "K2Node.h"
#include "EdGraphSchema_K2.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraph.h"
#include "BlueprintEditor.h"
#include "BlueprintEditorModule.h"
#include "Kismet2/KismetEditorUtilities.h"

// Register the tool
REGISTER_MCP_TOOL(FN2CMcpAddBlueprintNodeTool)

FN2CMcpAddBlueprintNodeTool::FN2CMcpAddBlueprintNodeTool()
{
}

FMcpToolDefinition FN2CMcpAddBlueprintNodeTool::GetDefinition() const
{
    FMcpToolDefinition Definition(
        TEXT("add-bp-node-to-active-graph"),
        TEXT("Adds a Blueprint node to the currently active graph. IMPORTANT: The search-blueprint-nodes tool MUST have been used before this tool in order to find the node and get its actionIdentifier from the spawnMetadata alongside its Name. The node's Name and actionIdentifier are required to spawn the exact correct variant of the node."),
        TEXT("Blueprint Graph Editing")
    );
    
    // Build input schema
    TSharedPtr<FJsonObject> Schema = MakeShareable(new FJsonObject);
    Schema->SetStringField(TEXT("type"), TEXT("object"));
    
    TSharedPtr<FJsonObject> Properties = MakeShareable(new FJsonObject);
    
    // nodeName property (required)
    TSharedPtr<FJsonObject> NodeNameProp = MakeShareable(new FJsonObject);
    NodeNameProp->SetStringField(TEXT("type"), TEXT("string"));
    NodeNameProp->SetStringField(TEXT("description"), TEXT("The name of the node to add (e.g., 'Spawn Actor from Class')"));
    Properties->SetObjectField(TEXT("nodeName"), NodeNameProp);
    
    // actionIdentifier property (required)
    TSharedPtr<FJsonObject> ActionIdProp = MakeShareable(new FJsonObject);
    ActionIdProp->SetStringField(TEXT("type"), TEXT("string"));
    ActionIdProp->SetStringField(TEXT("description"), TEXT("The unique action identifier obtained from the spawnMetadata.actionIdentifier field in search-blueprint-nodes results. This MUST be the exact value from the search results."));
    Properties->SetObjectField(TEXT("actionIdentifier"), ActionIdProp);
    
    // location property (optional)
    TSharedPtr<FJsonObject> LocationProp = MakeShareable(new FJsonObject);
    LocationProp->SetStringField(TEXT("type"), TEXT("object"));
    LocationProp->SetStringField(TEXT("description"), TEXT("The location to spawn the node at"));
    
    TSharedPtr<FJsonObject> LocationProps = MakeShareable(new FJsonObject);
    
    TSharedPtr<FJsonObject> XProp = MakeShareable(new FJsonObject);
    XProp->SetStringField(TEXT("type"), TEXT("number"));
    XProp->SetNumberField(TEXT("default"), 0.0);
    LocationProps->SetObjectField(TEXT("x"), XProp);
    
    TSharedPtr<FJsonObject> YProp = MakeShareable(new FJsonObject);
    YProp->SetStringField(TEXT("type"), TEXT("number"));
    YProp->SetNumberField(TEXT("default"), 0.0);
    LocationProps->SetObjectField(TEXT("y"), YProp);
    
    LocationProp->SetObjectField(TEXT("properties"), LocationProps);
    Properties->SetObjectField(TEXT("location"), LocationProp);
    
    Schema->SetObjectField(TEXT("properties"), Properties);
    
    // Required fields
    TArray<TSharedPtr<FJsonValue>> RequiredArray;
    RequiredArray.Add(MakeShareable(new FJsonValueString(TEXT("nodeName"))));
    RequiredArray.Add(MakeShareable(new FJsonValueString(TEXT("actionIdentifier"))));
    Schema->SetArrayField(TEXT("required"), RequiredArray);
    
    Definition.InputSchema = Schema;
    
    return Definition;
}

FMcpToolCallResult FN2CMcpAddBlueprintNodeTool::Execute(const TSharedPtr<FJsonObject>& Arguments)
{
    FMcpToolCallResult Result;
    
    // Parse arguments
    FString NodeName;
    FString ActionIdentifier;
    FVector2D Location(0, 0);
    FString ParseError;
    
    if (!ParseArguments(Arguments, NodeName, ActionIdentifier, Location, ParseError))
    {
        return FMcpToolCallResult::CreateErrorResult(ParseError);
    }
    
    FN2CLogger::Get().Log(FString::Printf(TEXT("Adding Blueprint node: '%s' with identifier: '%s' at location (%.2f, %.2f)"),
        *NodeName, *ActionIdentifier, Location.X, Location.Y), EN2CLogSeverity::Info);
    
    // Get the active graph context
    FString ContextError;
    UBlueprint* ActiveBlueprint = nullptr;
    UEdGraph* ActiveGraph = nullptr;
    if (!FN2CMcpBlueprintUtils::GetFocusedEditorGraph(ActiveBlueprint, ActiveGraph, ContextError))
    {
        return FMcpToolCallResult::CreateErrorResult(ContextError);
    }
    
    // Find and spawn the node
    FString SpawnedNodeId;
    FString SpawnError;
    
    if (!FindAndSpawnNode(NodeName, ActionIdentifier, ActiveBlueprint, ActiveGraph, Location, SpawnedNodeId, SpawnError))
    {
        return FMcpToolCallResult::CreateErrorResult(SpawnError);
    }
    
    FN2CLogger::Get().Log(FString::Printf(TEXT("Successfully added node '%s' to graph '%s'"), 
        *NodeName, *ActiveGraph->GetName()), EN2CLogSeverity::Info);
    
    // Return success with the spawned node ID
    TSharedPtr<FJsonObject> ResultObject = MakeShareable(new FJsonObject);
    ResultObject->SetBoolField(TEXT("success"), true);
    ResultObject->SetStringField(TEXT("nodeId"), SpawnedNodeId);
    ResultObject->SetStringField(TEXT("graphName"), ActiveGraph->GetName());
    ResultObject->SetStringField(TEXT("blueprintName"), ActiveBlueprint->GetName());
    
    FString ResultJson;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultJson);
    FJsonSerializer::Serialize(ResultObject.ToSharedRef(), Writer);
    
    // Result = FMcpToolCallResult::CreateTextResult(ResultJson); // This was assigning to the outer scope Result

    // Schedule deferred refresh of BlueprintActionDatabase
    FN2CMcpBlueprintUtils::DeferredRefreshBlueprintActionDatabase();
    
    return FMcpToolCallResult::CreateTextResult(ResultJson); // Return the result from the lambda
}

bool FN2CMcpAddBlueprintNodeTool::ParseArguments(
    const TSharedPtr<FJsonObject>& Arguments,
    FString& OutNodeName,
    FString& OutActionIdentifier,
    FVector2D& OutLocation,
    FString& OutError)
{
    if (!Arguments.IsValid())
    {
        OutError = TEXT("Invalid arguments object");
        return false;
    }
    
    FN2CMcpArgumentParser ArgParser(Arguments);
    
    // Required: nodeName
    if (!ArgParser.TryGetRequiredString(TEXT("nodeName"), OutNodeName, OutError, false))
    {
        return false;
    }
    
    // Required: actionIdentifier
    if (!ArgParser.TryGetRequiredString(TEXT("actionIdentifier"), OutActionIdentifier, OutError, false))
    {
        return false;
    }
    
    // Optional: location
    TSharedPtr<FJsonObject> LocationObject = ArgParser.GetOptionalObject(TEXT("location"));
    if (LocationObject.IsValid())
    {
        FN2CMcpArgumentParser LocationParser(LocationObject);
        OutLocation.X = LocationParser.GetOptionalNumber(TEXT("x"), 0.0);
        OutLocation.Y = LocationParser.GetOptionalNumber(TEXT("y"), 0.0);
    }
    
    return true;
}

bool FN2CMcpAddBlueprintNodeTool::FindAndSpawnNode(
    const FString& NodeName,
    const FString& ActionIdentifier,
    UBlueprint* Blueprint,
    UEdGraph* Graph,
    const FVector2D& Location,
    FString& OutNodeId,
    FString& OutError)
{
    // Build the action filter for the current context
    FBlueprintActionFilter Filter;
    Filter.Context.Blueprints.Add(Blueprint);
    Filter.Context.Graphs.Add(Graph);
    
    // Build the action list
    FBlueprintActionMenuBuilder MenuBuilder;
    MenuBuilder.AddMenuSection(Filter, FText::GetEmpty(), 0);
    MenuBuilder.RebuildActionList();
    
    // Tokenize node name for searching (same logic as search-blueprint-nodes)
    TArray<FString> FilterTerms;
    NodeName.ParseIntoArray(FilterTerms, TEXT(" "), true);
    
    // Also create case-insensitive versions
    TArray<FString> LowerFilterTerms;
    for (const FString& Term : FilterTerms)
    {
        LowerFilterTerms.Add(Term.ToLower());
    }
    
    // Search for actions matching the node name
    TArray<int32> MatchingActionIndices;
    
    for (int32 i = 0; i < MenuBuilder.GetNumActions(); ++i)
    {
        FGraphActionListBuilderBase::ActionGroup& Action = MenuBuilder.GetAction(i);
        const FString& ActionSearchText = Action.GetSearchTextForFirstAction();
        FString LowerSearchText = ActionSearchText.ToLower();
        
        // Check if all search terms match (case-insensitive) - same as search-blueprint-nodes
        bool bMatchesAllTerms = true;
        for (const FString& Term : LowerFilterTerms)
        {
            if (!LowerSearchText.Contains(Term))
            {
                bMatchesAllTerms = false;
                break;
            }
        }
        
        if (bMatchesAllTerms)
        {
            MatchingActionIndices.Add(i);
        }
    }
    
    if (MatchingActionIndices.Num() == 0)
    {
        OutError = FString::Printf(TEXT("No nodes found matching name: %s"), *NodeName);
        return false;
    }
    
    // Now find the exact match using actionIdentifier
    int32 ExactMatchIndex = -1;
    
    // Convert the actionIdentifier back to the original format
    // Simply replace > with \n since we use > as our delimiter
    FString SearchActionId = ActionIdentifier.Replace(TEXT(">"), TEXT("\n"));
    
    for (int32 Index : MatchingActionIndices)
    {
        FGraphActionListBuilderBase::ActionGroup& Action = MenuBuilder.GetAction(Index);
        FString CurrentActionId = Action.GetSearchTextForFirstAction();
        
        // Compare using the converted identifier
        if (CurrentActionId == SearchActionId)
        {
            ExactMatchIndex = Index;
            break;
        }
    }
    
    if (ExactMatchIndex == -1)
    {
        OutError = FString::Printf(TEXT("Found %d nodes matching name '%s', but none with the exact actionIdentifier"), 
            MatchingActionIndices.Num(), *NodeName);
        return false;
    }
    
    // Get the matching action
    FGraphActionListBuilderBase::ActionGroup& MatchedAction = MenuBuilder.GetAction(ExactMatchIndex);
    
    if (MatchedAction.Actions.Num() == 0)
    {
        OutError = TEXT("Matched action has no executable actions");
        return false;
    }
    
    // Execute the action to spawn the node
    TSharedPtr<FEdGraphSchemaAction> SchemaAction = MatchedAction.Actions[0];
    if (!SchemaAction.IsValid())
    {
        OutError = TEXT("Invalid schema action");
        return false;
    }
    
    // Store the current node count to identify the new node
    int32 PreSpawnNodeCount = Graph->Nodes.Num();
    
    // Perform the action to spawn the node
    UEdGraphNode* SpawnedNode = SchemaAction->PerformAction(Graph, nullptr, Location);
    
    if (!SpawnedNode)
    {
        // Sometimes the action doesn't return the node directly, check if a new node was added
        if (Graph->Nodes.Num() > PreSpawnNodeCount)
        {
            SpawnedNode = Graph->Nodes.Last();
        }
        else
        {
            OutError = TEXT("Failed to spawn node - action did not create a new node");
            return false;
        }
    }
    
    // Generate a unique ID for the spawned node
    static int32 SpawnedNodeCounter = 0;
    OutNodeId = FString::Printf(TEXT("SpawnedNode_%d"), ++SpawnedNodeCounter);
    
    // Compile Blueprint synchronously to ensure preview actors are properly updated
    FN2CMcpBlueprintUtils::MarkBlueprintAsModifiedAndCompile(Blueprint);
    
    // If it's a K2Node, we could return additional information
    if (UK2Node* K2Node = Cast<UK2Node>(SpawnedNode))
    {
        FN2CLogger::Get().Log(FString::Printf(TEXT("Spawned K2Node: %s at (%.2f, %.2f)"), 
            *K2Node->GetNodeTitle(ENodeTitleType::ListView).ToString(),
            SpawnedNode->NodePosX, SpawnedNode->NodePosY), EN2CLogSeverity::Debug);
    }
    
    return true;
}

TSharedPtr<FJsonObject> FN2CMcpAddBlueprintNodeTool::ConvertSpawnedNodeToJson(UK2Node* Node)
{
    if (!Node)
    {
        return nullptr;
    }
    
    // Create a node definition
    FN2CNodeDefinition NodeDef;
    
    // Use the NodeTranslator to process the node
    if (!FN2CNodeTranslator::Get().ProcessSingleNode(Node, NodeDef))
    {
        return nullptr;
    }
    
    // Convert to JSON
    return FN2CSerializer::NodeToJsonObject(NodeDef);
}
