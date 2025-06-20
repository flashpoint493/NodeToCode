// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/Tools/N2CMcpToolBase.h"

/**
 * MCP tool that returns the custom translation output directory configuration.
 * This tool queries the NodeToCode settings to get the custom output directory
 * path where translations are saved, or indicates if the default path is being used.
 */
class FN2CMcpGetCustomTranslationOutputDirectoryTool : public FN2CMcpToolBase
{
public:
    // IN2CMcpTool interface
    virtual FMcpToolDefinition GetDefinition() const override;
    virtual FMcpToolCallResult Execute(const TSharedPtr<FJsonObject>& Arguments) override;
    
    // This tool requires Game Thread to access UObject settings
    virtual bool RequiresGameThread() const override { return true; }
};