// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/Tools/N2CMcpToolBase.h"

/**
 * MCP tool for listing files and directories within the Unreal Engine project directory
 * Enforces path security to prevent directory traversal attacks outside the project
 */
class FN2CMcpReadPathTool : public FN2CMcpToolBase
{
public:
    virtual FMcpToolDefinition GetDefinition() const override;
    virtual FMcpToolCallResult Execute(const TSharedPtr<FJsonObject>& Arguments) override;
    
    // This tool needs to access file system operations
    virtual bool RequiresGameThread() const override { return true; }
};