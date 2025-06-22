// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/Tools/N2CMcpToolBase.h"

class UK2Node_VariableSet;
class UEdGraph;
class UBlueprint;

/**
 * MCP tool for creating a Set node for a member variable in the focused blueprint graph.
 * 
 * This tool creates a K2Node_VariableSet node that can be used to assign a value to
 * a member variable in a Blueprint. The created node will have:
 * - An execution input pin (when called from an execution context)
 * - An execution output pin (when called from an execution context)
 * - An input pin for the value to set (matching the variable's type)
 * - An output pin for the variable's value (same as the input)
 * 
 * Example usage:
 * {
 *   "variableName": "Health",
 *   "defaultValue": "100.0",
 *   "location": { "x": 400, "y": 200 }
 * }
 * 
 * The returned nodeId can be used with the connect-pins tool to wire up the node.
 */
class FN2CMcpCreateSetMemberVariableNode : public FN2CMcpToolBase
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
        FString& OutDefaultValue,
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
     * Create the Set node for the variable
     */
    UK2Node_VariableSet* CreateSetNode(
        UBlueprint* Blueprint,
        UEdGraph* Graph,
        const FBPVariableDescription& Variable,
        const FVector2D& Location
    );

    /**
     * Set the default value on the input pin if provided
     */
    bool SetNodeDefaultValue(
        UK2Node_VariableSet* SetNode,
        const FBPVariableDescription& Variable,
        const FString& DefaultValue,
        FString& OutError
    );

    /**
     * Build the success result JSON
     */
    TSharedPtr<FJsonObject> BuildSuccessResult(
        UK2Node_VariableSet* SetNode,
        const FBPVariableDescription& Variable,
        UBlueprint* Blueprint,
        UEdGraph* Graph
    );

    /**
     * Get information about a node's pins for the result
     */
    TArray<TSharedPtr<FJsonValue>> GetNodePins(UK2Node_VariableSet* SetNode);
};