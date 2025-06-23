// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/Tools/N2CMcpToolBase.h"

/**
 * MCP tool for creating a Get node for a local function variable in a Blueprint graph.
 * This creates a K2Node_VariableGet node that reads the value of a local variable at runtime.
 * 
 * The tool requires the currently focused graph to be a function graph (not an event graph
 * or construction script). The created node will have an output pin matching the variable's type.
 * 
 * Example usage:
 * {
 *   "variableName": "TempCounter",
 *   "x": 200,
 *   "y": 150
 * }
 * 
 * The returned nodeId can be used with the connect-pins tool to wire up the node.
 */
class FN2CMcpCreateGetLocalFunctionVariableNode : public FN2CMcpToolBase
{
public:
    virtual FMcpToolDefinition GetDefinition() const override;
    virtual FMcpToolCallResult Execute(const TSharedPtr<FJsonObject>& Arguments) override;
    virtual bool RequiresGameThread() const override { return true; }
    
private:
    /**
     * Finds the function entry node in a graph.
     * @param Graph The graph to search
     * @return The function entry node, or nullptr if not found
     */
    class UK2Node_FunctionEntry* FindFunctionEntryNode(class UEdGraph* Graph) const;
    
    /**
     * Finds a local variable by name in the function entry node.
     * @param FunctionEntry The function entry node
     * @param VariableName The name of the local variable
     * @return The variable description, or nullptr if not found
     */
    struct FBPVariableDescription* FindLocalVariable(class UK2Node_FunctionEntry* FunctionEntry, const FString& VariableName) const;
};