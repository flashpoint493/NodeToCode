// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FAssetData;
struct FContentBrowserItem;
class FJsonObject;

/**
 * @class FN2CMcpContentBrowserUtils
 * @brief Utility class for content browser operations in MCP tools
 * 
 * Provides common functionality for navigating, creating, and managing
 * content browser paths and assets.
 */
class FN2CMcpContentBrowserUtils
{
public:
	/**
	 * Validates a content browser path format
	 * @param Path The path to validate (e.g., "/Game/Blueprints")
	 * @param OutErrorMsg Error message if validation fails
	 * @return True if the path is valid
	 */
	static bool ValidateContentPath(const FString& Path, FString& OutErrorMsg);
	
	/**
	 * Checks if a content browser path exists
	 * @param Path The path to check
	 * @return True if the path exists
	 */
	static bool DoesPathExist(const FString& Path);
	
	/**
	 * Creates a folder at the specified path
	 * @param Path The path where the folder should be created
	 * @param OutErrorMsg Error message if creation fails
	 * @return True if the folder was created successfully
	 */
	static bool CreateContentFolder(const FString& Path, FString& OutErrorMsg);
	
	/**
	 * Navigates the primary content browser to the specified path
	 * @param Path The path to navigate to
	 * @return True if navigation was initiated (doesn't guarantee UI update)
	 */
	static bool NavigateToPath(const FString& Path);
	
	/**
	 * Selects an asset in the content browser
	 * @param AssetPath The full path to the asset (e.g., "/Game/Blueprints/MyAsset")
	 * @return True if the asset was found and selected
	 */
	static bool SelectAssetAtPath(const FString& AssetPath);
	
	/**
	 * Gets the currently selected paths in the content browser
	 * @param OutSelectedPaths Array to fill with selected paths
	 * @return True if any paths are selected
	 */
	static bool GetSelectedPaths(TArray<FString>& OutSelectedPaths);
	
	/**
	 * Normalizes a content browser path (removes trailing slashes, ensures proper format)
	 * @param Path The path to normalize
	 * @return The normalized path
	 */
	static FString NormalizeContentPath(const FString& Path);
	
	/**
	 * Checks if a path is within the allowed content directories
	 * @param Path The path to check
	 * @return True if the path is in an allowed directory
	 */
	static bool IsPathAllowed(const FString& Path);
	
	/**
	 * Enumerates items at a content browser path
	 * @param Path The path to enumerate
	 * @param bIncludeFolders Whether to include folders in results
	 * @param bIncludeFiles Whether to include files/assets in results
	 * @param OutItems Array to fill with content browser items
	 * @return True if enumeration succeeded
	 */
	static bool EnumerateItemsAtPath(const FString& Path, bool bIncludeFolders, bool bIncludeFiles, TArray<struct FContentBrowserItem>& OutItems);
	
	/**
	 * Filters content browser items by type
	 * @param Items Input array of items to filter
	 * @param FilterType Type filter (All, Blueprint, Material, Texture, StaticMesh, Folder)
	 * @param OutFilteredItems Output array of filtered items
	 */
	static void FilterItemsByType(const TArray<struct FContentBrowserItem>& Items, const FString& FilterType, TArray<struct FContentBrowserItem>& OutFilteredItems);
	
	/**
	 * Filters content browser items by name
	 * @param Items Input array of items to filter
	 * @param NameFilter Case-insensitive substring to match
	 * @param OutFilteredItems Output array of filtered items
	 */
	static void FilterItemsByName(const TArray<struct FContentBrowserItem>& Items, const FString& NameFilter, TArray<struct FContentBrowserItem>& OutFilteredItems);
	
	/**
	 * Converts a content browser item to JSON representation
	 * @param Item The item to convert
	 * @return JSON object representing the item
	 */
	static TSharedPtr<FJsonObject> ConvertItemToJson(const struct FContentBrowserItem& Item);
	
	/**
	 * Applies pagination to an array
	 * @param TotalItems Total number of items
	 * @param Page Current page (1-based)
	 * @param PageSize Items per page
	 * @param OutStartIndex Start index for pagination
	 * @param OutEndIndex End index for pagination
	 * @param OutHasMore Whether more pages exist
	 * @return True if pagination parameters are valid
	 */
	static bool CalculatePagination(int32 TotalItems, int32 Page, int32 PageSize, int32& OutStartIndex, int32& OutEndIndex, bool& OutHasMore);

private:
	// Prevent instantiation
	FN2CMcpContentBrowserUtils() = delete;
};