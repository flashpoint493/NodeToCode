// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/Tools/N2CMcpToolBase.h"

/**
 * @class FN2CMcpListBlueprintTagsTool
 * @brief MCP tool for listing Blueprint graph tags
 * 
 * This tool allows users to query and list tags that have been applied to Blueprint graphs.
 * It supports filtering by graph GUID, tag name, or category.
 */
class FN2CMcpListBlueprintTagsTool : public FN2CMcpToolBase
{
public:
	virtual FMcpToolDefinition GetDefinition() const override;
	virtual FMcpToolCallResult Execute(const TSharedPtr<FJsonObject>& Arguments) override;
	virtual bool RequiresGameThread() const override { return true; }
};