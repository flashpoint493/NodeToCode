// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/Tools/N2CMcpToolBase.h"

/**
 * MCP tool for setting the default value of a local function variable.
 * This modifies the FBPVariableDescription stored in the function entry node,
 * similar to editing the default value in the Blueprint editor's details panel.
 */
class FN2CMcpSetLocalFunctionVariableDefaultValue : public FN2CMcpToolBase
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