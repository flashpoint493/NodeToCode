// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/Tools/N2CMcpToolBase.h"

/**
 * MCP tool for reading file contents within the Unreal Engine project directory
 * Enforces path security to prevent directory traversal attacks outside the project
 * Supports text files with automatic content type detection based on file extension
 */
class FN2CMcpReadFileTool : public FN2CMcpToolBase
{
public:
    virtual FMcpToolDefinition GetDefinition() const override;
    virtual FMcpToolCallResult Execute(const TSharedPtr<FJsonObject>& Arguments) override;
    
    // This tool needs to access file system operations
    virtual bool RequiresGameThread() const override { return true; }
    
private:
    /** Maximum file size allowed to read (500KB) */
    static constexpr int64 MaxFileSize = 500 * 1024;
};