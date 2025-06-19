// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "MCP/Tools/N2CMcpToolBase.h"

class FN2CMcpTranslateBlueprintTool : public FN2CMcpToolBase
{
public:
    virtual FMcpToolDefinition GetDefinition() const override;
    virtual FMcpToolCallResult Execute(const TSharedPtr<FJsonObject>& Arguments) override;
    // The tool itself doesn't require game thread, the async task manages GT needs.
    virtual bool RequiresGameThread() const override { return false; } 
};
