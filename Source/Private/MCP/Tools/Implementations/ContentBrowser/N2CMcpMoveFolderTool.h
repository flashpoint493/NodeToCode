// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/Tools/N2CMcpToolBase.h"

/**
 * @class FN2CMcpMoveFolderTool
 * @brief MCP tool for moving folders and their contents in the content browser
 * 
 * This tool provides functionality to move entire folders including all their
 * assets and subfolders to a new location in the Unreal Engine content browser.
 */
class FN2CMcpMoveFolderTool : public FN2CMcpToolBase
{
public:
    virtual FMcpToolDefinition GetDefinition() const override;
    virtual FMcpToolCallResult Execute(const TSharedPtr<FJsonObject>& Arguments) override;
    virtual bool RequiresGameThread() const override { return true; }

private:
    bool ValidateFolderPath(const FString& Path, FString& OutNormalizedPath, FString& OutErrorMessage) const;
    bool ValidateDestinationPath(const FString& DestinationPath, const FString& FolderName, FString& OutFullPath, FString& OutErrorMessage) const;
    bool MoveFolderContents(const FString& SourcePath, const FString& DestinationPath, FString& OutErrorMessage) const;
    int32 CountAssetsInFolder(const FString& FolderPath) const;
    void CollectAllAssetsInFolder(const FString& FolderPath, TArray<FAssetData>& OutAssets) const;
};