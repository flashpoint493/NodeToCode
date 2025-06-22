// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/Tools/N2CMcpToolBase.h"

/**
 * MCP tool for copying assets to new locations in the content browser.
 * Supports both package paths (/Game/Folder/Asset) and object paths (/Game/Folder/Asset.Asset).
 */
class FN2CMcpCopyAssetTool : public FN2CMcpToolBase
{
public:
    virtual FMcpToolDefinition GetDefinition() const override;
    virtual FMcpToolCallResult Execute(const TSharedPtr<FJsonObject>& Arguments) override;
    virtual bool RequiresGameThread() const override { return true; }

private:
    bool ValidateSourceAsset(const FString& AssetPath, FString& OutNormalizedPath, FString& OutErrorMessage) const;
    bool ValidateDestinationPath(const FString& DestinationPath, FString& OutNormalizedPath, FString& OutErrorMessage) const;
    FString ConvertToPackagePath(const FString& AssetPath) const;
};