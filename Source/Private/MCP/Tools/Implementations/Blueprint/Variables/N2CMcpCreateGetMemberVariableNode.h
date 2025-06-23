// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/Tools/N2CMcpToolBase.h"

class UK2Node_VariableGet;
class UEdGraph;
class UBlueprint;

/**
 * MCP tool for creating a Get node for a member variable in the focused blueprint graph.
 * 
 * This tool creates a K2Node_VariableGet node that can be used to read a value from
 * a member variable in a Blueprint. The created node will have:
 * - An output pin for the variable's value (matching the variable's type)
 * 
 * Unlike Set nodes, Get nodes do not have execution pins unless they are used
 * in a pure function context where they might affect execution flow.
 * 
 * Example usage:
 * {
 *   "variableName": "Health",
 *   "location": { "x": 400, "y": 200 }
 * }
 * 
 * The returned nodeId can be used with the connect-pins tool to wire up the node.
 */
class FN2CMcpCreateGetMemberVariableNode : public FN2CMcpToolBase
{
public:
    virtual FMcpToolDefinition GetDefinition() const override;
    virtual FMcpToolCallResult Execute(const TSharedPtr<FJsonObject>& Arguments) override;
    virtual bool RequiresGameThread() const override { return true; }

private:
    /**
     * Parse and validate the input arguments
     */
    bool ParseArguments(
        const TSharedPtr<FJsonObject>& Arguments,
        FString& OutVariableName,
        FVector2D& OutLocation,
        FString& OutError
    );

    /**
     * Find the member variable in the blueprint
     */
    const FBPVariableDescription* FindMemberVariable(
        UBlueprint* Blueprint,
        const FString& VariableName,
        FString& OutError
    );

    /**
     * Create the Get node for the variable
     */
    UK2Node_VariableGet* CreateGetNode(
        UBlueprint* Blueprint,
        UEdGraph* Graph,
        const FBPVariableDescription& Variable,
        const FVector2D& Location
    );

    /**
     * Build the success result JSON
     */
    TSharedPtr<FJsonObject> BuildSuccessResult(
        UK2Node_VariableGet* GetNode,
        const FBPVariableDescription& Variable,
        UBlueprint* Blueprint,
        UEdGraph* Graph
    );

    /**
     * Get information about a node's pins for the result
     */
    TArray<TSharedPtr<FJsonValue>> GetNodePins(UK2Node_VariableGet* GetNode);
};