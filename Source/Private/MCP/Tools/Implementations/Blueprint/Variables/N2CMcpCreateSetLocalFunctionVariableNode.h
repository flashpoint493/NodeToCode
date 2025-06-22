// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/Tools/N2CMcpToolBase.h"

/**
 * MCP tool for creating a Set node for a local function variable in a Blueprint graph.
 * This creates a K2Node_VariableSet node that sets the value of a local variable at runtime.
 * Optionally sets the input pin's value on the created node.
 */
class FN2CMcpCreateSetLocalFunctionVariableNode : public FN2CMcpToolBase
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