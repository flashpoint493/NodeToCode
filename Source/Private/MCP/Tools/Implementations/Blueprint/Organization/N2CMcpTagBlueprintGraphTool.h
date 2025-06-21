// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/Tools/N2CMcpToolBase.h"

/**
 * @class FN2CMcpTagBlueprintGraphTool
 * @brief MCP tool for tagging Blueprint graphs
 * 
 * This tool allows users to apply tags to the currently focused Blueprint graph
 * with optional category and description information.
 */
class FN2CMcpTagBlueprintGraphTool : public FN2CMcpToolBase
{
public:
	virtual FMcpToolDefinition GetDefinition() const override;
	virtual FMcpToolCallResult Execute(const TSharedPtr<FJsonObject>& Arguments) override;
	virtual bool RequiresGameThread() const override { return true; }
};