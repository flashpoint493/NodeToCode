// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/Tools/N2CMcpToolBase.h"

/**
 * @class FN2CMcpCreateFolderTool
 * @brief MCP tool for creating folders in the content browser
 * 
 * This tool provides functionality to create new folders at specified paths
 * in the Unreal Engine content browser.
 */
class FN2CMcpCreateFolderTool : public FN2CMcpToolBase
{
public:
    virtual FMcpToolDefinition GetDefinition() const override;
    virtual FMcpToolCallResult Execute(const TSharedPtr<FJsonObject>& Arguments) override;
    virtual bool RequiresGameThread() const override { return true; }

private:
    bool ValidateFolderPath(const FString& Path, FString& OutNormalizedPath, FString& OutErrorMessage) const;
};