// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BlueprintLibraries/N2CTagBlueprintLibrary.h"
#include "N2CBatchTranslationBlueprintLibrary.generated.h"

class UN2CBatchTranslationOrchestrator;

/**
 * @class UN2CBatchTranslationBlueprintLibrary
 * @brief Blueprint function library for batch translation operations
 *
 * This library exposes batch translation functionality to Blueprints,
 * allowing UI integration for translating multiple tagged graphs at once.
 */
UCLASS()
class NODETOCODE_API UN2CBatchTranslationBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Start a batch translation with the given array of tagged graphs
	 * @param TaggedGraphs Array of FN2CTagInfo structs identifying graphs to translate
	 * @param bSuccess Whether the batch started successfully
	 * @param ErrorMessage Error message if the operation failed
	 */
	UFUNCTION(BlueprintCallable, Category = "NodeToCode|Batch Translation",
		meta = (DisplayName = "Start Batch Translation", ExpandBoolAsExecs = "bSuccess"))
	static void StartBatchTranslation(
		const TArray<FN2CTagInfo>& TaggedGraphs,
		bool& bSuccess,
		FString& ErrorMessage
	);

	/**
	 * Translate all graphs with a specific tag
	 * @param Tag The tag name to match
	 * @param OptionalCategory Optional category filter (empty for all)
	 * @param bSuccess Whether the batch started successfully
	 * @param ErrorMessage Error message if the operation failed
	 */
	UFUNCTION(BlueprintCallable, Category = "NodeToCode|Batch Translation",
		meta = (DisplayName = "Translate Graphs With Tag", ExpandBoolAsExecs = "bSuccess"))
	static void TranslateGraphsWithTag(
		const FString& Tag,
		const FString& OptionalCategory,
		bool& bSuccess,
		FString& ErrorMessage
	);

	/**
	 * Translate all graphs in a specific category
	 * @param Category The category name
	 * @param bSuccess Whether the batch started successfully
	 * @param ErrorMessage Error message if the operation failed
	 */
	UFUNCTION(BlueprintCallable, Category = "NodeToCode|Batch Translation",
		meta = (DisplayName = "Translate Graphs In Category", ExpandBoolAsExecs = "bSuccess"))
	static void TranslateGraphsInCategory(
		const FString& Category,
		bool& bSuccess,
		FString& ErrorMessage
	);

	/**
	 * Cancel the current batch translation
	 */
	UFUNCTION(BlueprintCallable, Category = "NodeToCode|Batch Translation",
		meta = (DisplayName = "Cancel Batch Translation"))
	static void CancelBatchTranslation();

	/**
	 * Check if a batch translation is currently in progress
	 * @return True if batch is running
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "NodeToCode|Batch Translation",
		meta = (DisplayName = "Is Batch Translation In Progress"))
	static bool IsBatchTranslationInProgress();

	/**
	 * Get the current batch progress as a percentage (0.0 - 1.0)
	 * @return Progress percentage
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "NodeToCode|Batch Translation",
		meta = (DisplayName = "Get Batch Progress"))
	static float GetBatchProgress();

	/**
	 * Get the batch translation orchestrator for binding to delegates
	 * @return The batch translation orchestrator instance
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "NodeToCode|Batch Translation",
		meta = (DisplayName = "Get Batch Orchestrator"))
	static UN2CBatchTranslationOrchestrator* GetBatchOrchestrator();
};
