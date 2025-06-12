#include "N2CMcpSearchBlueprintNodesTool.h"
#include "MCP/Tools/N2CMcpToolRegistry.h"
#include "Core/N2CEditorIntegration.h"
#include "Utils/N2CLogger.h"
#include "BlueprintActionFilter.h"
#include "BlueprintActionMenuBuilder.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Core/N2CNodeTranslator.h"
#include "Core/N2CSerializer.h"
#include "Utils/N2CNodeTypeRegistry.h"
#include "K2Node.h"
#include "EdGraphSchema_K2.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraph.h"

// Register the tool
REGISTER_MCP_TOOL(FN2CMcpSearchBlueprintNodesTool)

FN2CMcpSearchBlueprintNodesTool::FN2CMcpSearchBlueprintNodesTool()
{
}

FMcpToolDefinition FN2CMcpSearchBlueprintNodesTool::GetDefinition() const
{
    FMcpToolDefinition Definition;
    Definition.Name = TEXT("search-blueprint-nodes");
    Definition.Description = TEXT("Searches for Blueprint nodes/actions relevant to a given query. Can perform a context-sensitive search based on the current Blueprint or a global search.");
    
    // Build input schema
    TSharedPtr<FJsonObject> Schema = MakeShareable(new FJsonObject);
    Schema->SetStringField(TEXT("type"), TEXT("object"));
    
    TSharedPtr<FJsonObject> Properties = MakeShareable(new FJsonObject);
    
    // searchTerm property
    TSharedPtr<FJsonObject> SearchTermProp = MakeShareable(new FJsonObject);
    SearchTermProp->SetStringField(TEXT("type"), TEXT("string"));
    SearchTermProp->SetStringField(TEXT("description"), TEXT("The text query to search for"));
    Properties->SetObjectField(TEXT("searchTerm"), SearchTermProp);
    
    // contextSensitive property
    TSharedPtr<FJsonObject> ContextSensitiveProp = MakeShareable(new FJsonObject);
    ContextSensitiveProp->SetStringField(TEXT("type"), TEXT("boolean"));
    ContextSensitiveProp->SetStringField(TEXT("description"), TEXT("If true, performs a context-sensitive search using blueprintContext. If false, performs a global search ignoring blueprintContext."));
    ContextSensitiveProp->SetBoolField(TEXT("default"), true);
    Properties->SetObjectField(TEXT("contextSensitive"), ContextSensitiveProp);
    
    // maxResults property
    TSharedPtr<FJsonObject> MaxResultsProp = MakeShareable(new FJsonObject);
    MaxResultsProp->SetStringField(TEXT("type"), TEXT("integer"));
    MaxResultsProp->SetStringField(TEXT("description"), TEXT("Maximum number of results to return"));
    MaxResultsProp->SetNumberField(TEXT("default"), 20);
    MaxResultsProp->SetNumberField(TEXT("minimum"), 1);
    MaxResultsProp->SetNumberField(TEXT("maximum"), 100);
    Properties->SetObjectField(TEXT("maxResults"), MaxResultsProp);
    
    // blueprintContext property
    TSharedPtr<FJsonObject> ContextProp = MakeShareable(new FJsonObject);
    ContextProp->SetStringField(TEXT("type"), TEXT("object"));
    ContextProp->SetStringField(TEXT("description"), TEXT("Information to make the search context-sensitive when contextSensitive is true"));
    
    TSharedPtr<FJsonObject> ContextProps = MakeShareable(new FJsonObject);
    
    TSharedPtr<FJsonObject> GraphPathProp = MakeShareable(new FJsonObject);
    GraphPathProp->SetStringField(TEXT("type"), TEXT("string"));
    GraphPathProp->SetStringField(TEXT("description"), TEXT("The asset path of the UEdGraph currently being viewed"));
    ContextProps->SetObjectField(TEXT("graphPath"), GraphPathProp);
    
    TSharedPtr<FJsonObject> BlueprintPathProp = MakeShareable(new FJsonObject);
    BlueprintPathProp->SetStringField(TEXT("type"), TEXT("string"));
    BlueprintPathProp->SetStringField(TEXT("description"), TEXT("The asset path of the UBlueprint asset itself"));
    ContextProps->SetObjectField(TEXT("owningBlueprintPath"), BlueprintPathProp);
    
    ContextProp->SetObjectField(TEXT("properties"), ContextProps);
    Properties->SetObjectField(TEXT("blueprintContext"), ContextProp);
    
    Schema->SetObjectField(TEXT("properties"), Properties);
    
    // Required fields
    TArray<TSharedPtr<FJsonValue>> RequiredArray;
    RequiredArray.Add(MakeShareable(new FJsonValueString(TEXT("searchTerm"))));
    Schema->SetArrayField(TEXT("required"), RequiredArray);
    
    Definition.InputSchema = Schema;
    
    // Annotations
    // Annotations
    Definition.Annotations = MakeShareable(new FJsonObject);
    Definition.Annotations->SetBoolField(TEXT("readOnlyHint"), true);
    
    return Definition;
}

FMcpToolCallResult FN2CMcpSearchBlueprintNodesTool::Execute(const TSharedPtr<FJsonObject>& Arguments)
{
    FMcpToolCallResult Result;
    
    // Parse arguments
    FString SearchTerm;
    bool bContextSensitive;
    int32 MaxResults;
    TSharedPtr<FJsonObject> BlueprintContext;
    FString ParseError;
    
    if (!ParseArguments(Arguments, SearchTerm, bContextSensitive, MaxResults, BlueprintContext, ParseError))
    {
        return FMcpToolCallResult::CreateErrorResult(ParseError);
    }
    
    FN2CLogger::Get().Log(FString::Printf(TEXT("Searching for Blueprint nodes: '%s' (ContextSensitive: %s, MaxResults: %d)"),
        *SearchTerm, bContextSensitive ? TEXT("true") : TEXT("false"), MaxResults), EN2CLogSeverity::Info);
    
    // Get context if needed
    UBlueprint* ContextBlueprint = nullptr;
    UEdGraph* ContextGraph = nullptr;
    
    if (bContextSensitive && BlueprintContext.IsValid())
    {
        FString ContextError;
        if (!GetContextFromPaths(BlueprintContext, ContextBlueprint, ContextGraph, ContextError))
        {
            // Try to fall back to active editor
            ContextGraph = FN2CEditorIntegration::Get().GetFocusedGraphFromActiveEditor();
            if (ContextGraph)
            {
                ContextBlueprint = FBlueprintEditorUtils::FindBlueprintForGraph(ContextGraph);
            }
            
            if (!ContextGraph || !ContextBlueprint)
            {
                return FMcpToolCallResult::CreateErrorResult(
                    FString::Printf(TEXT("Context-sensitive search requested but no valid context available: %s"), *ContextError));
            }
        }
    }
    else if (bContextSensitive)
    {
        // Try to get context from active editor
        ContextGraph = FN2CEditorIntegration::Get().GetFocusedGraphFromActiveEditor();
        if (ContextGraph)
        {
            ContextBlueprint = FBlueprintEditorUtils::FindBlueprintForGraph(ContextGraph);
        }
        
        if (!ContextGraph || !ContextBlueprint)
        {
            return FMcpToolCallResult::CreateErrorResult(TEXT("Context-sensitive search requested but no Blueprint editor is active"));
        }
    }
    
    // Set up the action filter
    FBlueprintActionFilter Filter;
    if (bContextSensitive && ContextBlueprint && ContextGraph)
    {
        Filter.Context.Blueprints.Add(ContextBlueprint);
        Filter.Context.Graphs.Add(ContextGraph);
    }
    // For global search, leave Context empty
    
    // Build the action list
    FBlueprintActionMenuBuilder MenuBuilder;
    MenuBuilder.AddMenuSection(Filter, FText::GetEmpty(), 0);
    
    // Rebuild to populate from database
    MenuBuilder.RebuildActionList();
    
    // Tokenize search terms
    TArray<FString> FilterTerms;
    SearchTerm.ParseIntoArray(FilterTerms, TEXT(" "), true);
    
    // Also create case-insensitive versions
    TArray<FString> LowerFilterTerms;
    for (const FString& Term : FilterTerms)
    {
        LowerFilterTerms.Add(Term.ToLower());
    }
    
    // Search through actions
    TArray<TSharedPtr<FJsonValue>> ResultNodes;
    int32 ResultCount = 0;
    
    for (int32 i = 0; i < MenuBuilder.GetNumActions() && ResultCount < MaxResults; ++i)
    {
        FGraphActionListBuilderBase::ActionGroup& Action = MenuBuilder.GetAction(i);
        const FString& ActionSearchText = Action.GetSearchTextForFirstAction();
        FString LowerSearchText = ActionSearchText.ToLower();
        
        // Check if all search terms match (case-insensitive)
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
            TSharedPtr<FJsonObject> NodeJson = ConvertActionToJson(Action, bContextSensitive, ContextBlueprint, ContextGraph);
            if (NodeJson.IsValid())
            {
                ResultNodes.Add(MakeShareable(new FJsonValueObject(NodeJson)));
                ResultCount++;
            }
        }
    }
    
    // Build result JSON
    TSharedPtr<FJsonObject> ResultObject = MakeShareable(new FJsonObject);
    ResultObject->SetArrayField(TEXT("nodes"), ResultNodes);
    
    FString ResultJson;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultJson);
    FJsonSerializer::Serialize(ResultObject.ToSharedRef(), Writer);
    
    Result = FMcpToolCallResult::CreateTextResult(ResultJson);
    
    FN2CLogger::Get().Log(FString::Printf(TEXT("Blueprint node search completed. Found %d results"), ResultCount), EN2CLogSeverity::Info);
    
    return Result;
}

bool FN2CMcpSearchBlueprintNodesTool::ParseArguments(
    const TSharedPtr<FJsonObject>& Arguments,
    FString& OutSearchTerm,
    bool& OutContextSensitive,
    int32& OutMaxResults,
    TSharedPtr<FJsonObject>& OutBlueprintContext,
    FString& OutError)
{
    if (!Arguments.IsValid())
    {
        OutError = TEXT("Invalid arguments object");
        return false;
    }
    
    // Required: searchTerm
    if (!Arguments->TryGetStringField(TEXT("searchTerm"), OutSearchTerm))
    {
        OutError = TEXT("Missing required field: searchTerm");
        return false;
    }
    
    if (OutSearchTerm.IsEmpty())
    {
        OutError = TEXT("searchTerm cannot be empty");
        return false;
    }
    
    // Optional: contextSensitive (default: true)
    OutContextSensitive = true;
    Arguments->TryGetBoolField(TEXT("contextSensitive"), OutContextSensitive);
    
    // Optional: maxResults (default: 20)
    OutMaxResults = 20;
    if (Arguments->HasField(TEXT("maxResults")))
    {
        Arguments->TryGetNumberField(TEXT("maxResults"), OutMaxResults);
        OutMaxResults = FMath::Clamp(OutMaxResults, 1, 100);
    }
    
    // Optional: blueprintContext
    const TSharedPtr<FJsonObject>* ContextObject;
    if (Arguments->TryGetObjectField(TEXT("blueprintContext"), ContextObject))
    {
        OutBlueprintContext = *ContextObject;
    }
    
    return true;
}

bool FN2CMcpSearchBlueprintNodesTool::GetContextFromPaths(
    const TSharedPtr<FJsonObject>& BlueprintContext,
    UBlueprint*& OutBlueprint,
    UEdGraph*& OutGraph,
    FString& OutError)
{
    if (!BlueprintContext.IsValid())
    {
        OutError = TEXT("Invalid blueprintContext object");
        return false;
    }
    
    FString BlueprintPath;
    if (BlueprintContext->TryGetStringField(TEXT("owningBlueprintPath"), BlueprintPath))
    {
        // Load the blueprint
        FSoftObjectPath SoftPath(BlueprintPath);
        OutBlueprint = Cast<UBlueprint>(SoftPath.TryLoad());
        
        if (!OutBlueprint)
        {
            OutError = FString::Printf(TEXT("Failed to load Blueprint from path: %s"), *BlueprintPath);
            return false;
        }
    }
    
    FString GraphPath;
    if (BlueprintContext->TryGetStringField(TEXT("graphPath"), GraphPath))
    {
        // Parse graph path (format: /Path/To/Blueprint.Blueprint:GraphName)
        FString PackagePath;
        FString GraphName;
        if (GraphPath.Split(TEXT(":"), &PackagePath, &GraphName))
        {
            // If we don't have the blueprint yet, try to load it from the graph path
            if (!OutBlueprint)
            {
                FSoftObjectPath SoftPath(PackagePath);
                OutBlueprint = Cast<UBlueprint>(SoftPath.TryLoad());
            }
            
            if (OutBlueprint)
            {
                // Find the graph by name
                for (UEdGraph* Graph : OutBlueprint->UbergraphPages)
                {
                    if (Graph && Graph->GetFName().ToString() == GraphName)
                    {
                        OutGraph = Graph;
                        break;
                    }
                }
                
                // Also check function graphs
                if (!OutGraph)
                {
                    for (UEdGraph* Graph : OutBlueprint->FunctionGraphs)
                    {
                        if (Graph && Graph->GetFName().ToString() == GraphName)
                        {
                            OutGraph = Graph;
                            break;
                        }
                    }
                }
            }
        }
    }
    
    return OutBlueprint != nullptr;
}

TSharedPtr<FJsonObject> FN2CMcpSearchBlueprintNodesTool::ConvertActionToJson(
    const FGraphActionListBuilderBase::ActionGroup& Action,
    bool bIsContextSensitive,
    UBlueprint* ContextBlueprint,
    UEdGraph* ContextGraph)
{
    // Generate a unique node ID for this search result
    static int32 SearchNodeCounter = 0;
    FString NodeId = FString::Printf(TEXT("SearchNode_%d"), ++SearchNodeCounter);
    
    TSharedPtr<FJsonObject> NodeJson;
    
    // Try to spawn the actual node if we have a context
    if (ContextGraph && Action.Actions.Num() > 0)
    {
        TSharedPtr<FEdGraphSchemaAction> SchemaAction = Action.Actions[0];
        if (SchemaAction.IsValid())
        {
            // Perform the action to spawn the node in the context graph
            FVector2D SpawnLocation(0, 0);
            UEdGraphNode* SpawnedNode = SchemaAction->PerformAction(ContextGraph, nullptr, SpawnLocation);
            
            // Try to cast to K2Node
            UK2Node* K2Node = Cast<UK2Node>(SpawnedNode);
            if (K2Node)
            {
                // Convert the actual spawned node to N2CJSON
                NodeJson = ConvertNodeToN2CJson(K2Node, NodeId);
                
                // Remove the node from the graph immediately
                ContextGraph->RemoveNode(SpawnedNode);
            }
            else if (SpawnedNode)
            {
                // Clean up non-K2 nodes
                ContextGraph->RemoveNode(SpawnedNode);
            }
        }
    }
    
    // Fallback if we couldn't spawn the node
    if (!NodeJson.IsValid())
    {
        // Create a template node definition from the action metadata
        FN2CNodeDefinition NodeDef = CreateNodeDefinitionFromAction(Action, NodeId);
        
        // Convert to JSON using the standard N2C serializer
        NodeJson = FN2CSerializer::NodeToJsonObject(NodeDef);
    }
    
    // Add minimal spawning metadata for future node creation
    if (NodeJson.IsValid() && Action.Actions.Num() > 0)
    {
        TSharedPtr<FJsonObject> SpawnMetadata = MakeShareable(new FJsonObject);
        
        // Use the raw search text as the unique action identifier
        // This is the most precise way to identify a specific action
        FString ActionIdentifier = Action.GetSearchTextForFirstAction();
        
        // Replace newlines with a delimiter that's easier for LLMs to handle
        // This makes the identifier a single line that's easier to copy/paste
        // Use > as delimiter since it's unique and won't be confused with existing content
        ActionIdentifier = ActionIdentifier.Replace(TEXT("\n"), TEXT(">"));
        
        SpawnMetadata->SetStringField(TEXT("actionIdentifier"), ActionIdentifier);
        
        // Store whether this action requires specific context
        SpawnMetadata->SetBoolField(TEXT("isContextSensitive"), bIsContextSensitive);
        
        NodeJson->SetObjectField(TEXT("spawnMetadata"), SpawnMetadata);
    }
    
    return NodeJson;
}

FString FN2CMcpSearchBlueprintNodesTool::ExtractInternalName(const FGraphActionListBuilderBase::ActionGroup& Action)
{
    // For now, we'll return an empty string as accessing internal action structure is complex
    return FString();
}

TArray<FString> FN2CMcpSearchBlueprintNodesTool::ExtractCategoryPath(const FGraphActionListBuilderBase::ActionGroup& Action)
{
    TArray<FString> CategoryPath;
    
    // Extract categories from the search text if possible
    FString SearchText = Action.GetSearchTextForFirstAction();
    
    // Simple heuristic: split by common separators that might appear in search text
    // This is a simplified approach since we can't directly access the category structure
    if (!SearchText.IsEmpty())
    {
        // Look for category-like patterns in the search text
        // For example "Development > String > Print String"
        SearchText.ParseIntoArray(CategoryPath, TEXT(">"), true);
        
        // Clean up the entries
        for (FString& Category : CategoryPath)
        {
            Category = Category.TrimStartAndEnd();
        }
    }
    
    return CategoryPath;
}

TSharedPtr<FJsonObject> FN2CMcpSearchBlueprintNodesTool::ConvertNodeToN2CJson(UK2Node* Node, const FString& NodeId)
{
    if (!Node)
    {
        return nullptr;
    }
    
    // Create a node definition with the specified ID
    FN2CNodeDefinition NodeDef;
    NodeDef.ID = NodeId;
    
    // Use the NodeTranslator to properly process the node
    // This reuses all the existing node processing logic including:
    // - Node type determination
    // - Node processor selection
    // - Pin processing
    // - Property extraction
    if (!FN2CNodeTranslator::Get().ProcessSingleNode(Node, NodeDef))
    {
        FN2CLogger::Get().LogWarning(FString::Printf(TEXT("Failed to process node %s for search results"), 
            *Node->GetNodeTitle(ENodeTitleType::ListView).ToString()));
        return nullptr;
    }
    
    // Convert to JSON using the serializer
    return FN2CSerializer::NodeToJsonObject(NodeDef);
}

FN2CNodeDefinition FN2CMcpSearchBlueprintNodesTool::CreateNodeDefinitionFromAction(const FGraphActionListBuilderBase::ActionGroup& Action, const FString& NodeId)
{
    FN2CNodeDefinition NodeDef;
    NodeDef.ID = NodeId;
    
    // Get search text as the node name
    FString SearchText = Action.GetSearchTextForFirstAction();
    NodeDef.Name = SearchText;
    
    // Try to determine node type from the action
    // This is a simplified approach since we don't have the actual node yet
    NodeDef.NodeType = EN2CNodeType::CallFunction; // Default to function call
    
    // Extract category information for potential node type hints
    TArray<FString> Categories = ExtractCategoryPath(Action);
    if (Categories.Num() > 0)
    {
        const FString& FirstCategory = Categories[0].ToLower();
        
        // Try to guess node type from category
        if (FirstCategory.Contains(TEXT("variable")))
        {
            NodeDef.NodeType = EN2CNodeType::VariableGet;
        }
        else if (FirstCategory.Contains(TEXT("event")))
        {
            NodeDef.NodeType = EN2CNodeType::Event;
        }
        else if (FirstCategory.Contains(TEXT("flow")))
        {
            NodeDef.NodeType = EN2CNodeType::Branch;
        }
        else if (FirstCategory.Contains(TEXT("struct")))
        {
            NodeDef.NodeType = EN2CNodeType::MakeStruct;
        }
    }
    
    // Set default flags
    NodeDef.bPure = false;
    NodeDef.bLatent = false;
    
    // Note: We can't populate pins without the actual node instance
    // This would require spawning the node which is more complex
    
    return NodeDef;
}