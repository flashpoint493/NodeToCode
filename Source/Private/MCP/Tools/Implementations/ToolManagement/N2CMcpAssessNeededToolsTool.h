// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "MCP/Tools/N2CMcpToolBase.h"

/**
 * @class FN2CMcpAssessNeededToolsTool
 * @brief MCP tool for dynamically managing the available toolset based on categories.
 */
class FN2CMcpAssessNeededToolsTool : public FN2CMcpToolBase
{
public:
    virtual FMcpToolDefinition GetDefinition() const override;
    virtual FMcpToolCallResult Execute(const TSharedPtr<FJsonObject>& Arguments) override;
    virtual bool RequiresGameThread() const override { return true; }

private:
    /**
     * Gets the descriptions for all tool categories.
     * @return A map of category names to their descriptions.
     */
    static TMap<FString, FString> GetCategoryDescriptions();
};