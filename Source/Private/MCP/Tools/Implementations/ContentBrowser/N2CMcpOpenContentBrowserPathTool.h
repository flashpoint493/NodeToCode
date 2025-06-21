// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/Tools/N2CMcpToolBase.h"

/**
 * @class FN2CMcpOpenContentBrowserPathTool
 * @brief MCP tool for navigating to a specified path in the content browser
 * 
 * This tool allows MCP clients to programmatically navigate the Unreal Editor's
 * content browser to specific paths, optionally creating folders and selecting assets.
 */
class FN2CMcpOpenContentBrowserPathTool : public FN2CMcpToolBase
{
public:
	virtual FMcpToolDefinition GetDefinition() const override;
	virtual FMcpToolCallResult Execute(const TSharedPtr<FJsonObject>& Arguments) override;
	virtual bool RequiresGameThread() const override { return true; }
};