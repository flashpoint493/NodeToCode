#pragma once

#include "CoreMinimal.h"
#include "MCP/Tools/N2CMcpToolBase.h"
#include "Engine/Blueprint.h"

// Forward declarations
class UEdGraph;
struct FBlueprintActionMenuBuilder;

/**
 * MCP Tool for adding Blueprint nodes to the active graph
 * Uses node name and actionIdentifier to find and spawn specific nodes
 */
class NODETOCODE_API FN2CMcpAddBlueprintNodeTool : public FN2CMcpToolBase
{
public:
    FN2CMcpAddBlueprintNodeTool();
    
    // IN2CMcpTool interface
    virtual FMcpToolDefinition GetDefinition() const override;
    virtual FMcpToolCallResult Execute(const TSharedPtr<FJsonObject>& Arguments) override;
    virtual bool RequiresGameThread() const override { return true; }
    
private:
    // Helper methods
    bool ParseArguments(const TSharedPtr<FJsonObject>& Arguments,
                       FString& OutNodeName,
                       FString& OutActionIdentifier,
                       FVector2D& OutLocation,
                       FString& OutError);
    
    bool GetActiveGraphContext(UBlueprint*& OutBlueprint,
                             UEdGraph*& OutGraph,
                             FString& OutError);
    
    bool FindAndSpawnNode(const FString& NodeName,
                         const FString& ActionIdentifier,
                         UBlueprint* Blueprint,
                         UEdGraph* Graph,
                         const FVector2D& Location,
                         FString& OutNodeId,
                         FString& OutError);
    
    // Convert spawned node to N2CJSON format
    TSharedPtr<FJsonObject> ConvertSpawnedNodeToJson(class UK2Node* Node);
};