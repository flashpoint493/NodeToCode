// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/Tools/N2CMcpToolBase.h"

/**
 * MCP tool for moving/renaming assets within the content browser.
 * Supports both single asset moves and batch operations.
 */
class FN2CMcpMoveAssetTool : public FN2CMcpToolBase
{
public:
	virtual FMcpToolDefinition GetDefinition() const override;
	virtual FMcpToolCallResult Execute(const TSharedPtr<FJsonObject>& Arguments) override;
	virtual bool RequiresGameThread() const override { return true; }

private:
	/**
	 * Validates the source asset path
	 */
	bool ValidateSourceAsset(const FString& AssetPath, FString& OutErrorMessage) const;

	/**
	 * Validates the destination path and prepares asset name
	 */
	bool ValidateDestinationPath(const FString& DestinationPath, const FString& NewName, 
		FString& OutFullPath, FString& OutErrorMessage) const;

	/**
	 * Performs the actual asset move/rename operation
	 */
	bool MoveAsset(const FString& SourcePath, const FString& DestinationPath, 
		FString& OutNewPath, FString& OutErrorMessage) const;

	/**
	 * Builds the success response with move details
	 */
	TSharedPtr<FJsonObject> BuildSuccessResponse(const FString& OldPath, const FString& NewPath, 
		bool bLeftRedirector) const;

	/**
	 * Extracts the asset name from a full asset path
	 */
	FString GetAssetNameFromPath(const FString& AssetPath) const;

	/**
	 * Checks if asset is checked out by another user
	 */
	bool IsAssetCheckedOutByAnother(const FString& AssetPath) const;
};