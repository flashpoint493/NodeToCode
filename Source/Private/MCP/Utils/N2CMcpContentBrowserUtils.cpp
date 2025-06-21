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