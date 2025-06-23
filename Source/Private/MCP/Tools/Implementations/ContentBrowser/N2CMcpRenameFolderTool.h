// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/Tools/N2CMcpToolBase.h"

/**
 * @class FN2CMcpRenameFolderTool
 * @brief MCP tool for renaming folders in the content browser
 * 
 * This tool allows renaming a folder and automatically updates all assets within it.
 * It effectively moves all contents to a new folder with the new name.
 * 
 * @example
 * {
 *   "sourcePath": "/Game/OldFolderName",
 *   "newName": "NewFolderName"
 * }
 * 
 * @note The folder will be renamed in place within its parent directory
 */
class FN2CMcpRenameFolderTool : public FN2CMcpToolBase
{
public:
    virtual FMcpToolDefinition GetDefinition() const override;
    virtual FMcpToolCallResult Execute(const TSharedPtr<FJsonObject>& Arguments) override;
    virtual bool RequiresGameThread() const override { return true; }

private:
    /**
     * Validates the source folder path
     * @param Path The folder path to validate
     * @param OutNormalizedPath The normalized path
     * @param OutErrorMessage Error message if validation fails
     * @return True if validation succeeds
     */
    bool ValidateFolderPath(const FString& Path, FString& OutNormalizedPath, FString& OutErrorMessage) const;

    /**
     * Validates the new folder name
     * @param NewName The new folder name
     * @param OutErrorMessage Error message if validation fails
     * @return True if validation succeeds
     */
    bool ValidateNewName(const FString& NewName, FString& OutErrorMessage) const;

    /**
     * Moves all folder contents to the new location
     * @param SourcePath The source folder path
     * @param DestinationPath The destination folder path
     * @param OutErrorMessage Error message if operation fails
     * @return True if all assets were moved successfully
     */
    bool MoveFolderContents(const FString& SourcePath, const FString& DestinationPath, FString& OutErrorMessage) const;

    /**
     * Counts the number of assets in a folder
     * @param FolderPath The folder path to count assets in
     * @return The number of assets
     */
    int32 CountAssetsInFolder(const FString& FolderPath) const;

    /**
     * Collects all assets in a folder recursively
     * @param FolderPath The folder path
     * @param OutAssets Array to fill with asset data
     */
    void CollectAllAssetsInFolder(const FString& FolderPath, TArray<struct FAssetData>& OutAssets) const;
};