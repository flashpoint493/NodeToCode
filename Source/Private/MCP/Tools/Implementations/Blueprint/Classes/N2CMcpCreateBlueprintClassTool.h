// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/Tools/N2CMcpToolBase.h"

/**
 * MCP tool for creating new Blueprint classes with specified parent classes.
 * Integrates with search-blueprint-classes tool results to provide a complete
 * Blueprint creation workflow.
 */
class FN2CMcpCreateBlueprintClassTool : public FN2CMcpToolBase
{
public:
	virtual FMcpToolDefinition GetDefinition() const override;
	virtual FMcpToolCallResult Execute(const TSharedPtr<FJsonObject>& Arguments) override;
	virtual bool RequiresGameThread() const override { return true; }

private:
	/**
	 * Validates the parent class for Blueprint creation
	 * @param ClassPath The path to the parent class (e.g., "/Script/Engine.Actor")
	 * @param OutClass The resolved UClass if found
	 * @param OutErrorMessage Error message if validation fails
	 * @return True if the class is valid for Blueprint creation
	 */
	bool ValidateParentClass(const FString& ClassPath, UClass*& OutClass, FString& OutErrorMessage) const;

	/**
	 * Validates and prepares the asset path for Blueprint creation
	 * @param AssetPath The desired asset path for the Blueprint
	 * @param BlueprintName The desired Blueprint name
	 * @param OutPackageName The resulting package name
	 * @param OutAssetName The resulting asset name (may be modified for uniqueness)
	 * @param OutErrorMessage Error message if validation fails
	 * @return True if the path is valid
	 */
	bool ValidateAndPrepareAssetPath(const FString& AssetPath, const FString& BlueprintName, 
		FString& OutPackageName, FString& OutAssetName, FString& OutErrorMessage) const;

	/**
	 * Determines the appropriate Blueprint type based on the parent class
	 * @param ParentClass The parent class
	 * @return The Blueprint type (Normal, Interface, Const, etc.)
	 */
	EBlueprintType DetermineBlueprintType(const UClass* ParentClass) const;

	/**
	 * Creates the Blueprint asset
	 * @param ParentClass The parent class
	 * @param PackageName The package name
	 * @param AssetName The asset name
	 * @param BlueprintType The Blueprint type
	 * @param OutErrorMessage Error message if creation fails
	 * @return The created Blueprint, or nullptr on failure
	 */
	class UBlueprint* CreateBlueprintAsset(UClass* ParentClass, const FString& PackageName, 
		const FString& AssetName, EBlueprintType BlueprintType, FString& OutErrorMessage) const;

	/**
	 * Applies initial settings to the newly created Blueprint
	 * @param Blueprint The Blueprint to configure
	 * @param Arguments The tool arguments containing optional settings
	 */
	void ApplyBlueprintSettings(class UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& Arguments) const;

	/**
	 * Opens the Blueprint in the editor
	 * @param Blueprint The Blueprint to open
	 * @param bOpenInFullEditor Whether to open in full Blueprint editor or not
	 */
	void OpenBlueprintInEditor(class UBlueprint* Blueprint, bool bOpenInFullEditor) const;

	/**
	 * Builds the JSON result for successful Blueprint creation
	 * @param Blueprint The created Blueprint
	 * @param PackageName The package name
	 * @param AssetName The asset name
	 * @return JSON object with creation details
	 */
	TSharedPtr<FJsonObject> BuildSuccessResult(const class UBlueprint* Blueprint, 
		const FString& PackageName, const FString& AssetName) const;
};