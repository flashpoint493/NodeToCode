// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "MCP/Tools/N2CMcpToolBase.h"

class UBlueprint;

/**
 * Opens a specified blueprint asset in the Blueprint Editor
 */
class FN2CMcpOpenBlueprintAssetTool : public FN2CMcpToolBase
{
public:
	virtual FMcpToolDefinition GetDefinition() const override;
	virtual FMcpToolCallResult Execute(const TSharedPtr<FJsonObject>& Arguments) override;
	virtual bool RequiresGameThread() const override { return true; }

private:
	/**
	 * Validates that the given asset path is a valid blueprint path
	 * @param AssetPath The path to validate
	 * @param OutErrorMsg Error message if validation fails
	 * @return True if valid blueprint path
	 */
	bool ValidateBlueprintAssetPath(const FString& AssetPath, FString& OutErrorMsg) const;
	
	/**
	 * Gets the blueprint type as a string (Actor, Component, Interface, etc.)
	 * @param Blueprint The blueprint to check
	 * @return String representation of the blueprint type
	 */
	FString GetBlueprintTypeString(const UBlueprint* Blueprint) const;
	
	/**
	 * Focuses on a specific graph within the blueprint editor
	 * @param BlueprintEditor The blueprint editor instance
	 * @param Blueprint The blueprint being edited
	 * @param GraphName The name of the graph to focus on
	 * @return True if the graph was found and focused
	 */
	bool FocusOnGraph(TSharedPtr<class IBlueprintEditor> BlueprintEditor, UBlueprint* Blueprint, const FString& GraphName) const;
	
	/**
	 * Builds the JSON result object for successful execution
	 * @param Blueprint The opened blueprint
	 * @param AssetPath The asset path that was opened
	 * @param FocusedGraph The name of the focused graph (if any)
	 * @return JSON object with result data
	 */
	TSharedPtr<FJsonObject> BuildSuccessResult(const UBlueprint* Blueprint, const FString& AssetPath, const FString& FocusedGraph) const;
};