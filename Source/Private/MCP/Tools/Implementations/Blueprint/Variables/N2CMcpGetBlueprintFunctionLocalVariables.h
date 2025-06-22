// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/Tools/N2CMcpToolBase.h"

/**
 * MCP tool for retrieving local variables defined in a Blueprint function.
 * Local variables are stored in the FunctionEntry node and are scoped to that specific function.
 */
class FN2CMcpGetBlueprintFunctionLocalVariables : public FN2CMcpToolBase
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
};