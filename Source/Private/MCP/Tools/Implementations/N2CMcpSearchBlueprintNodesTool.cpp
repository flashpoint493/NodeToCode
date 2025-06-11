#include "N2CMcpSearchBlueprintNodesTool.h"
#include "MCP/Tools/N2CMcpToolRegistry.h"
#include "Core/N2CEditorIntegration.h"
#include "Utils/N2CLogger.h"
#include "BlueprintActionFilter.h"
#include "BlueprintActionMenuBuilder.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet2/BlueprintEditorUtils.h"

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
            TSharedPtr<FJsonObject> NodeJson = ConvertActionToJson(Action, bContextSensitive);
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
    bool bIsContextSensitive)
{
    TSharedPtr<FJsonObject> NodeJson = MakeShareable(new FJsonObject);
    
    // Get search text which contains most of the info we need
    FString SearchText = Action.GetSearchTextForFirstAction();
    
    // For display name, we'll use the search text
    NodeJson->SetStringField(TEXT("displayName"), SearchText);
    
    // Keywords (from search text)
    if (!SearchText.IsEmpty())
    {
        NodeJson->SetStringField(TEXT("keywords"), SearchText);
    }
    
    // Extract category path if possible
    TArray<FString> CategoryPath = ExtractCategoryPath(Action);
    TArray<TSharedPtr<FJsonValue>> CategoryArray;
    for (const FString& Category : CategoryPath)
    {
        CategoryArray.Add(MakeShareable(new FJsonValueString(Category)));
    }
    NodeJson->SetArrayField(TEXT("categoryPath"), CategoryArray);
    
    // Internal name
    FString InternalName = ExtractInternalName(Action);
    if (!InternalName.IsEmpty())
    {
        NodeJson->SetStringField(TEXT("internalName"), InternalName);
    }
    
    // Context sensitivity flag
    NodeJson->SetBoolField(TEXT("isContextSensitiveMatch"), bIsContextSensitive);
    
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