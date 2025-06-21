// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#include "N2CMcpContentBrowserUtils.h"
#include "ContentBrowserModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor/ContentBrowser/Public/IContentBrowserSingleton.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "Editor.h"
#include "AssetRegistry/AssetData.h"
#include "Utils/N2CLogger.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "ContentBrowserDataSubsystem.h"
#include "ContentBrowserDataFilter.h"
#include "ContentBrowserItem.h"
#include "Dom/JsonObject.h"

bool FN2CMcpContentBrowserUtils::ValidateContentPath(const FString& Path, FString& OutErrorMsg)
{
	// Check if empty
	if (Path.IsEmpty())
	{
		OutErrorMsg = TEXT("Path cannot be empty");
		return false;
	}
	
	// Normalize the path first
	FString NormalizedPath = NormalizeContentPath(Path);
	
	// Check if it starts with /
	if (!NormalizedPath.StartsWith(TEXT("/")))
	{
		OutErrorMsg = TEXT("Path must start with '/'");
		return false;
	}
	
	// Check if it's in an allowed directory
	if (!IsPathAllowed(NormalizedPath))
	{
		OutErrorMsg = FString::Printf(TEXT("Path '%s' is not in an allowed content directory"), *NormalizedPath);
		return false;
	}
	
	// Validate package name format
	FText FailureReason;
	if (!FPackageName::IsValidLongPackageName(NormalizedPath, false, &FailureReason))
	{
		OutErrorMsg = FString::Printf(TEXT("Invalid package path: %s"), *FailureReason.ToString());
		return false;
	}
	
	return true;
}

bool FN2CMcpContentBrowserUtils::DoesPathExist(const FString& Path)
{
	// Get the editor asset subsystem
	UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	if (!EditorAssetSubsystem)
	{
		return false;
	}
	
	// Check if the directory exists
	return EditorAssetSubsystem->DoesDirectoryExist(Path);
}

bool FN2CMcpContentBrowserUtils::CreateContentFolder(const FString& Path, FString& OutErrorMsg)
{
	// Validate the path first
	if (!ValidateContentPath(Path, OutErrorMsg))
	{
		return false;
	}
	
	// Get the editor asset subsystem
	UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	if (!EditorAssetSubsystem)
	{
		OutErrorMsg = TEXT("Failed to get EditorAssetSubsystem");
		return false;
	}
	
	// Check if it already exists
	if (DoesPathExist(Path))
	{
		OutErrorMsg = FString::Printf(TEXT("Path already exists: %s"), *Path);
		return false;
	}
	
	// Make the directory
	if (!EditorAssetSubsystem->MakeDirectory(Path))
	{
		OutErrorMsg = FString::Printf(TEXT("Failed to create directory: %s"), *Path);
		return false;
	}
	
	FN2CLogger::Get().Log(FString::Printf(TEXT("Created content folder: %s"), *Path), EN2CLogSeverity::Info);
	return true;
}

bool FN2CMcpContentBrowserUtils::NavigateToPath(const FString& Path)
{
	FContentBrowserModule& ContentBrowserModule = 
		FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	
	IContentBrowserSingleton& ContentBrowser = ContentBrowserModule.Get();
	
	// Create array of folders to sync to
	TArray<FString> FoldersToSync;
	FoldersToSync.Add(Path);
	
	// Sync the primary content browser to the folder
	ContentBrowser.SyncBrowserToFolders(FoldersToSync);
	
	FN2CLogger::Get().Log(FString::Printf(TEXT("Navigated content browser to: %s"), *Path), EN2CLogSeverity::Info);
	
	// SyncBrowserToFolders doesn't return success/failure, so we assume success
	return true;
}

bool FN2CMcpContentBrowserUtils::SelectAssetAtPath(const FString& AssetPath)
{
	// Get the asset registry
	FAssetRegistryModule& AssetRegistryModule = 
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	
	// Convert path to object path if needed
	FString ObjectPath = AssetPath;
	if (!ObjectPath.Contains(TEXT(".")))
	{
		// If no class is specified, try to find the asset
		FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(AssetPath));
		if (AssetData.IsValid())
		{
			ObjectPath = AssetData.GetSoftObjectPath().ToString();
		}
		else
		{
			FN2CLogger::Get().LogWarning(FString::Printf(TEXT("Asset not found at path: %s"), *AssetPath));
			return false;
		}
	}
	
	// Get the asset data
	FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(ObjectPath));
	if (!AssetData.IsValid())
	{
		FN2CLogger::Get().LogWarning(FString::Printf(TEXT("Invalid asset at path: %s"), *ObjectPath));
		return false;
	}
	
	// Sync to the asset
	TArray<FAssetData> AssetsToSync;
	AssetsToSync.Add(AssetData);
	
	FContentBrowserModule& ContentBrowserModule = 
		FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	ContentBrowserModule.Get().SyncBrowserToAssets(AssetsToSync);
	
	FN2CLogger::Get().Log(FString::Printf(TEXT("Selected asset: %s"), *AssetPath), EN2CLogSeverity::Info);
	return true;
}

bool FN2CMcpContentBrowserUtils::GetSelectedPaths(TArray<FString>& OutSelectedPaths)
{
	FContentBrowserModule& ContentBrowserModule = 
		FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	
	// Get the selected paths from the content browser
	ContentBrowserModule.Get().GetSelectedPathViewFolders(OutSelectedPaths);
	
	return OutSelectedPaths.Num() > 0;
}

FString FN2CMcpContentBrowserUtils::NormalizeContentPath(const FString& Path)
{
	FString NormalizedPath = Path;
	
	// Replace backslashes with forward slashes
	NormalizedPath.ReplaceInline(TEXT("\\"), TEXT("/"));
	
	// Remove trailing slash if present (unless it's just "/")
	if (NormalizedPath.Len() > 1 && NormalizedPath.EndsWith(TEXT("/")))
	{
		NormalizedPath = NormalizedPath.Left(NormalizedPath.Len() - 1);
	}
	
	// Ensure it starts with /
	if (!NormalizedPath.StartsWith(TEXT("/")))
	{
		NormalizedPath = TEXT("/") + NormalizedPath;
	}
	
	return NormalizedPath;
}

bool FN2CMcpContentBrowserUtils::IsPathAllowed(const FString& Path)
{
	// List of allowed root directories
	static const TArray<FString> AllowedRoots = {
		TEXT("/Game"),
		TEXT("/Engine"),
		TEXT("/EnginePresets"),
		TEXT("/Paper2D"),
		// Add plugin content paths
		TEXT("/NodeToCode"),
		// Common plugin paths
		TEXT("/Plugins")
	};
	
	// Check if the path starts with any allowed root
	for (const FString& AllowedRoot : AllowedRoots)
	{
		if (Path.StartsWith(AllowedRoot))
		{
			return true;
		}
	}
	
	// Also check if it's a plugin content path (format: /PluginName)
	if (Path.StartsWith(TEXT("/")) && !Path.Contains(TEXT("../")) && !Path.Contains(TEXT("..\\")))
	{
		// Basic security check - no directory traversal
		return true;
	}
	
	return false;
}

bool FN2CMcpContentBrowserUtils::EnumerateItemsAtPath(const FString& Path, bool bIncludeFolders, bool bIncludeFiles, TArray<FContentBrowserItem>& OutItems)
{
	// Get the content browser data subsystem
	UContentBrowserDataSubsystem* ContentBrowserData = GEditor->GetEditorSubsystem<UContentBrowserDataSubsystem>();
	if (!ContentBrowserData)
	{
		FN2CLogger::Get().LogError(TEXT("Content browser data subsystem not available"));
		return false;
	}
	
	// Set up filter
	FContentBrowserDataFilter Filter;
	
	// Configure what types of items to include
	EContentBrowserItemTypeFilter TypeFilter = EContentBrowserItemTypeFilter::IncludeNone;
	if (bIncludeFolders)
	{
		TypeFilter |= EContentBrowserItemTypeFilter::IncludeFolders;
	}
	if (bIncludeFiles)
	{
		TypeFilter |= EContentBrowserItemTypeFilter::IncludeFiles;
	}
	
	Filter.ItemTypeFilter = TypeFilter;
	
	// Get items at the specified path
	TArray<FContentBrowserItem> AllItems = ContentBrowserData->GetItemsAtPath(FName(*Path), Filter.ItemTypeFilter);
	
	// Copy to output
	OutItems = AllItems;
	
	return true;
}

void FN2CMcpContentBrowserUtils::FilterItemsByType(const TArray<FContentBrowserItem>& Items, const FString& FilterType, TArray<FContentBrowserItem>& OutFilteredItems)
{
	OutFilteredItems.Reset();
	
	// If filter is "All", just copy everything
	if (FilterType.Equals(TEXT("All"), ESearchCase::IgnoreCase))
	{
		OutFilteredItems = Items;
		return;
	}
	
	// Filter by specific type
	for (const FContentBrowserItem& Item : Items)
	{
		bool bInclude = false;
		
		if (Item.IsFolder())
		{
			// Check if we want folders
			if (FilterType.Equals(TEXT("Folder"), ESearchCase::IgnoreCase))
			{
				bInclude = true;
			}
		}
		else
		{
			// Get asset data to check type
			FAssetData AssetData;
			if (Item.Legacy_TryGetAssetData(AssetData))
			{
				FString AssetClass = AssetData.AssetClassPath.GetAssetName().ToString();
				
				// Check against filter type
				if (FilterType.Equals(TEXT("Blueprint"), ESearchCase::IgnoreCase) && AssetClass.Contains(TEXT("Blueprint")))
				{
					bInclude = true;
				}
				else if (FilterType.Equals(TEXT("Material"), ESearchCase::IgnoreCase) && 
					(AssetClass.Contains(TEXT("Material")) && !AssetClass.Contains(TEXT("MaterialFunction"))))
				{
					bInclude = true;
				}
				else if (FilterType.Equals(TEXT("Texture"), ESearchCase::IgnoreCase) && AssetClass.Contains(TEXT("Texture")))
				{
					bInclude = true;
				}
				else if (FilterType.Equals(TEXT("StaticMesh"), ESearchCase::IgnoreCase) && AssetClass.Contains(TEXT("StaticMesh")))
				{
					bInclude = true;
				}
			}
		}
		
		if (bInclude)
		{
			OutFilteredItems.Add(Item);
		}
	}
}

void FN2CMcpContentBrowserUtils::FilterItemsByName(const TArray<FContentBrowserItem>& Items, const FString& NameFilter, TArray<FContentBrowserItem>& OutFilteredItems)
{
	OutFilteredItems.Reset();
	
	// If no filter, copy everything
	if (NameFilter.IsEmpty())
	{
		OutFilteredItems = Items;
		return;
	}
	
	// Filter by name (case-insensitive substring match)
	for (const FContentBrowserItem& Item : Items)
	{
		FString ItemName = Item.GetDisplayName().ToString();
		if (ItemName.Contains(NameFilter, ESearchCase::IgnoreCase))
		{
			OutFilteredItems.Add(Item);
		}
	}
}

TSharedPtr<FJsonObject> FN2CMcpContentBrowserUtils::ConvertItemToJson(const FContentBrowserItem& Item)
{
	TSharedPtr<FJsonObject> ItemJson = MakeShareable(new FJsonObject);
	
	// Basic info
	ItemJson->SetStringField(TEXT("path"), Item.GetVirtualPath().ToString());
	ItemJson->SetStringField(TEXT("name"), Item.GetDisplayName().ToString());
	ItemJson->SetBoolField(TEXT("is_folder"), Item.IsFolder());
	
	if (!Item.IsFolder())
	{
		// Get asset-specific information
		FAssetData AssetData;
		if (Item.Legacy_TryGetAssetData(AssetData))
		{
			// Asset type from class name
			FString AssetClassName = AssetData.AssetClassPath.GetAssetName().ToString();
			ItemJson->SetStringField(TEXT("type"), AssetClassName);
			
			// Full class path
			ItemJson->SetStringField(TEXT("class"), AssetData.AssetClassPath.ToString());
			
			// Additional metadata if needed
			if (AssetData.GetClass())
			{
				ItemJson->SetStringField(TEXT("native_class"), AssetData.GetClass()->GetName());
			}
		}
		else
		{
			// Fallback if we can't get asset data
			ItemJson->SetStringField(TEXT("type"), TEXT("Unknown"));
			ItemJson->SetStringField(TEXT("class"), TEXT("Unknown"));
		}
	}
	else
	{
		// It's a folder
		ItemJson->SetStringField(TEXT("type"), TEXT("Folder"));
		ItemJson->SetStringField(TEXT("class"), TEXT("Folder"));
	}
	
	return ItemJson;
}

bool FN2CMcpContentBrowserUtils::CalculatePagination(int32 TotalItems, int32 Page, int32 PageSize, int32& OutStartIndex, int32& OutEndIndex, bool& OutHasMore)
{
	// Validate inputs
	if (Page < 1 || PageSize < 1)
	{
		FN2CLogger::Get().LogWarning(TEXT("Invalid pagination parameters: Page and PageSize must be >= 1"));
		return false;
	}
	
	// Calculate indices
	OutStartIndex = (Page - 1) * PageSize;
	OutEndIndex = FMath::Min(OutStartIndex + PageSize, TotalItems);
	
	// Check if start is beyond total items
	if (OutStartIndex >= TotalItems && TotalItems > 0)
	{
		FN2CLogger::Get().LogWarning(FString::Printf(TEXT("Page %d is beyond available items"), Page));
		return false;
	}
	
	// Determine if there are more pages
	OutHasMore = OutEndIndex < TotalItems;
	
	return true;
}