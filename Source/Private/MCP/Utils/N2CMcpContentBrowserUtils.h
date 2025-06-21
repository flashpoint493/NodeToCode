// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FAssetData;

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

private:
	// Prevent instantiation
	FN2CMcpContentBrowserUtils() = delete;
};