// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "N2CPythonBridge.generated.h"

/**
 * Bridge class exposing NodeToCode functionality to Python scripts.
 *
 * These functions are callable from Python via:
 *   unreal.N2CPythonBridge.get_focused_blueprint_json()
 */
UCLASS()
class NODETOCODE_API UN2CPythonBridge : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Get the currently focused Blueprint graph as N2CJSON format.
	 *
	 * @return JSON string containing the Blueprint data, or error JSON if no Blueprint is focused.
	 *         Format: {"success": true/false, "data": {...} or null, "error": "..." or null}
	 */
	UFUNCTION(BlueprintCallable, Category = "NodeToCode|Python")
	static FString GetFocusedBlueprintJson();

	/**
	 * Compile the currently focused Blueprint.
	 *
	 * @return JSON string with compilation result.
	 *         Format: {"success": true/false, "data": {"status": "..."}, "error": "..." or null}
	 */
	UFUNCTION(BlueprintCallable, Category = "NodeToCode|Python")
	static FString CompileFocusedBlueprint();

	/**
	 * Save the currently focused Blueprint to disk.
	 *
	 * @param bOnlyIfDirty If true, only saves if the Blueprint has unsaved changes.
	 * @return JSON string with save result.
	 *         Format: {"success": true/false, "data": {"was_saved": true/false}, "error": "..." or null}
	 */
	UFUNCTION(BlueprintCallable, Category = "NodeToCode|Python")
	static FString SaveFocusedBlueprint(bool bOnlyIfDirty = true);

	// ========== Tagging System ==========

	/**
	 * Tag the currently focused Blueprint graph.
	 *
	 * @param Tag The tag name to apply
	 * @param Category Optional category (default: "Default")
	 * @param Description Optional description
	 * @return JSON string with tag info or error
	 */
	UFUNCTION(BlueprintCallable, Category = "NodeToCode|Python")
	static FString TagFocusedGraph(const FString& Tag, const FString& Category = TEXT("Default"), const FString& Description = TEXT(""));

	/**
	 * List all tags, optionally filtered by category or tag name.
	 *
	 * @param Category Optional category filter (empty string for all)
	 * @param Tag Optional tag name filter (empty string for all)
	 * @return JSON array of matching tags
	 */
	UFUNCTION(BlueprintCallable, Category = "NodeToCode|Python")
	static FString ListTags(const FString& Category = TEXT(""), const FString& Tag = TEXT(""));

	/**
	 * Remove a tag from a graph.
	 *
	 * @param GraphGuid The GUID of the graph (string format)
	 * @param Tag The tag name to remove
	 * @return JSON result with removal status
	 */
	UFUNCTION(BlueprintCallable, Category = "NodeToCode|Python")
	static FString RemoveTag(const FString& GraphGuid, const FString& Tag);

	// ========== LLM Provider Info ==========

	/**
	 * Get available LLM providers and their configuration.
	 *
	 * @return JSON with provider list, current provider, and status
	 */
	UFUNCTION(BlueprintCallable, Category = "NodeToCode|Python")
	static FString GetLLMProviders();

	/**
	 * Get the currently active LLM provider info.
	 *
	 * @return JSON with current provider name, model, and endpoint
	 */
	UFUNCTION(BlueprintCallable, Category = "NodeToCode|Python")
	static FString GetActiveProvider();

private:
	/** Helper to create a success JSON response */
	static FString MakeSuccessJson(const FString& DataJson);

	/** Helper to create an error JSON response */
	static FString MakeErrorJson(const FString& ErrorMessage);
};
