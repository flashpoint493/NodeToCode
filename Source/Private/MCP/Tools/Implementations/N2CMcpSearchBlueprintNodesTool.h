#pragma once

#include "CoreMinimal.h"
#include "MCP/Tools/N2CMcpToolBase.h"
#include "Engine/Blueprint.h"
// Forward declarations

class UEdGraph;
struct FBlueprintActionMenuBuilder;

/**
 * MCP Tool for searching Blueprint nodes/actions
 */
class NODETOCODE_API FN2CMcpSearchBlueprintNodesTool : public FN2CMcpToolBase
{
public:
    FN2CMcpSearchBlueprintNodesTool();
    
    // IN2CMcpTool interface
    virtual FMcpToolDefinition GetDefinition() const override;
    virtual FMcpToolCallResult Execute(const TSharedPtr<FJsonObject>& Arguments) override;
    virtual bool RequiresGameThread() const override { return true; }
    
private:
    // Helper methods
    bool ParseArguments(const TSharedPtr<FJsonObject>& Arguments,
                       FString& OutSearchTerm,
                       bool& OutContextSensitive,
                       int32& OutMaxResults,
                       TSharedPtr<FJsonObject>& OutBlueprintContext,
                       FString& OutError);
    
    bool GetContextFromPaths(const TSharedPtr<FJsonObject>& BlueprintContext,
                            UBlueprint*& OutBlueprint,
                            UEdGraph*& OutGraph,
                            FString& OutError);
    
    TSharedPtr<FJsonObject> ConvertActionToJson(
        const FGraphActionListBuilderBase::ActionGroup& Action,
        bool bIsContextSensitive);
    
    FString ExtractInternalName(const FGraphActionListBuilderBase::ActionGroup& Action);
    TArray<FString> ExtractCategoryPath(const FGraphActionListBuilderBase::ActionGroup& Action);
};