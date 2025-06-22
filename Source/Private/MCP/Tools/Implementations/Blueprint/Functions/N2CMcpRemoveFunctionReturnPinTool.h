// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/Tools/N2CMcpToolBase.h"

/**
 * MCP tool that removes a return value from a Blueprint function.
 * 
 * Tool name: remove-function-return-pin
 * 
 * Input schema:
 * - pinName (string, required): The name of the return value to remove
 * 
 * This tool removes return values from Blueprint functions by deleting 
 * the corresponding input pin on the UK2Node_FunctionResult node.
 */
class FN2CMcpRemoveFunctionReturnPinTool : public FN2CMcpToolBase
{
public:
	virtual FMcpToolDefinition GetDefinition() const override;
	virtual FMcpToolCallResult Execute(const TSharedPtr<FJsonObject>& Arguments) override;
	virtual bool RequiresGameThread() const override { return true; }
};