// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/Tools/N2CMcpToolBase.h"

/**
 * @class FN2CMcpRenameAssetTool
 * @brief MCP tool for renaming assets in the content browser
 * 
 * This tool allows renaming an asset to a new name within the same directory
 * or moving it to a different location (rename with path change).
 * 
 * @example Rename asset within same directory:
 * {
 *   "sourcePath": "/Game/Blueprints/OldName",
 *   "newName": "NewName"
 * }
 * 
 * @example Move and rename asset:
 * {
 *   "sourcePath": "/Game/Blueprints/OldName",
 *   "destinationPath": "/Game/NewFolder/NewName"
 * }
 */
class FN2CMcpRenameAssetTool : public FN2CMcpToolBase
{
public:
    virtual FMcpToolDefinition GetDefinition() const override;
    virtual FMcpToolCallResult Execute(const TSharedPtr<FJsonObject>& Arguments) override;
    virtual bool RequiresGameThread() const override { return true; }

private:
    /**
     * Validates the source asset path and checks if it exists
     * @param AssetPath The asset path to validate
     * @param OutNormalizedPath The normalized path
     * @param OutErrorMessage Error message if validation fails
     * @return True if validation succeeds
     */
    bool ValidateSourceAsset(const FString& AssetPath, FString& OutNormalizedPath, FString& OutErrorMessage) const;

    /**
     * Validates the destination path or new name
     * @param SourcePath The normalized source path
     * @param NewNameOrPath The new name or full destination path
     * @param OutDestinationPath The full destination path
     * @param OutErrorMessage Error message if validation fails
     * @return True if validation succeeds
     */
    bool ValidateDestination(const FString& SourcePath, const FString& NewNameOrPath, 
        FString& OutDestinationPath, FString& OutErrorMessage) const;

    /**
     * Converts an asset path to package path format
     * @param AssetPath The asset path to convert
     * @return The package path
     */
    FString ConvertToPackagePath(const FString& AssetPath) const;
};