// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/Tools/N2CMcpToolBase.h"

/**
 * @class FN2CMcpRemoveTagFromGraphTool
 * @brief MCP tool for removing a tag from a Blueprint graph
 * 
 * This tool removes a specific tag from a Blueprint graph by its GUID and tag name.
 * If multiple tags with the same name exist in different categories, all will be removed.
 * The operation is idempotent - removing a non-existent tag is not an error.
 */
class FN2CMcpRemoveTagFromGraphTool : public FN2CMcpToolBase
{
public:
	virtual FMcpToolDefinition GetDefinition() const override;
	virtual FMcpToolCallResult Execute(const TSharedPtr<FJsonObject>& Arguments) override;
	virtual bool RequiresGameThread() const override { return true; }
};