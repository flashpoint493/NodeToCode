// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/Tools/N2CMcpToolBase.h"

/**
 * MCP tool that removes a pin from a Blueprint function entry node.
 * 
 * Tool name: remove-function-entry-pin
 * 
 * Input schema:
 * - pinName (string, required): The name of the pin to remove from the entry node
 * 
 * This tool removes parameter pins (output pins) from the UK2Node_FunctionEntry node.
 */
class FN2CMcpRemoveFunctionEntryPinTool : public FN2CMcpToolBase
{
public:
	virtual FMcpToolDefinition GetDefinition() const override;
	virtual FMcpToolCallResult Execute(const TSharedPtr<FJsonObject>& Arguments) override;
	virtual bool RequiresGameThread() const override { return true; }
};