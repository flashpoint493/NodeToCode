// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/Tools/N2CMcpToolBase.h"

struct FAssetData;

/**
 * MCP tool for searching the content browser across all mounted paths
 */
class FN2CMcpSearchContentBrowserTool : public FN2CMcpToolBase
{
public:
	virtual FMcpToolDefinition GetDefinition() const override;
	virtual FMcpToolCallResult Execute(const TSharedPtr<FJsonObject>& Arguments) override;
	virtual bool RequiresGameThread() const override { return true; }

private:
	/**
	 * Searches for assets matching the given criteria
	 * @param SearchQuery The search query (name pattern)
	 * @param AssetType The type of assets to search for (empty for all)
	 * @param bIncludeEngineContent Whether to include engine content
	 * @param bIncludePluginContent Whether to include plugin content
	 * @param OutAssets The found assets
	 * @return True if the search succeeded
	 */
	bool SearchAssets(const FString& SearchQuery, const FString& AssetType, 
		bool bIncludeEngineContent, bool bIncludePluginContent, TArray<FAssetData>& OutAssets) const;

	/**
	 * Filters assets by type
	 * @param Assets The assets to filter
	 * @param AssetType The type to filter by
	 * @param OutFilteredAssets The filtered assets
	 */
	void FilterAssetsByType(const TArray<FAssetData>& Assets, const FString& AssetType, 
		TArray<FAssetData>& OutFilteredAssets) const;

	/**
	 * Scores an asset based on how well it matches the search query
	 * @param AssetData The asset to score
	 * @param SearchQuery The search query
	 * @return Score from 0.0 to 1.0
	 */
	float ScoreAssetMatch(const FAssetData& AssetData, const FString& SearchQuery) const;

	/**
	 * Converts an asset to a JSON representation
	 * @param AssetData The asset data
	 * @param Score The relevance score
	 * @return JSON object representing the asset
	 */
	TSharedPtr<FJsonObject> ConvertAssetToJson(const FAssetData& AssetData, float Score) const;

	/**
	 * Gets the display type for an asset
	 * @param AssetData The asset data
	 * @return Human-readable type string
	 */
	FString GetAssetDisplayType(const FAssetData& AssetData) const;

	/**
	 * Checks if a path should be included based on search settings
	 * @param PackagePath The package path to check
	 * @param bIncludeEngineContent Whether to include engine content
	 * @param bIncludePluginContent Whether to include plugin content
	 * @return True if the path should be included
	 */
	bool ShouldIncludePath(const FString& PackagePath, bool bIncludeEngineContent, bool bIncludePluginContent) const;
};