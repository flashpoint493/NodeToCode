// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Models/N2CTranslation.h"
#include "BlueprintLibraries/N2CTagBlueprintLibrary.h"
#include "N2CBatchTranslationTypes.generated.h"

/**
 * @enum EN2CBatchItemStatus
 * @brief Status of an individual item in a batch translation
 */
UENUM(BlueprintType)
enum class EN2CBatchItemStatus : uint8
{
	Pending		UMETA(DisplayName = "Pending"),
	Processing	UMETA(DisplayName = "Processing"),
	Completed	UMETA(DisplayName = "Completed"),
	Failed		UMETA(DisplayName = "Failed"),
	Skipped		UMETA(DisplayName = "Skipped")
};

/**
 * @struct FN2CBatchTranslationItem
 * @brief Internal tracking struct for a single item in a batch translation
 */
USTRUCT()
struct NODETOCODE_API FN2CBatchTranslationItem
{
	GENERATED_BODY()

	/** The tag info identifying this graph */
	UPROPERTY()
	FN2CTagInfo TagInfo;

	/** Cached reference to the loaded Blueprint */
	UPROPERTY()
	TWeakObjectPtr<UBlueprint> CachedBlueprint;

	/** Cached reference to the graph within the Blueprint */
	TWeakObjectPtr<UEdGraph> CachedGraph;

	/** Current processing status */
	UPROPERTY()
	EN2CBatchItemStatus Status = EN2CBatchItemStatus::Pending;

	/** Error message if processing failed */
	UPROPERTY()
	FString ErrorMessage;

	/** The translation response once completed */
	UPROPERTY()
	FN2CTranslationResponse TranslationResponse;

	FN2CBatchTranslationItem() = default;
};

/**
 * @struct FN2CBatchTranslationResult
 * @brief Summary result of a batch translation operation
 */
USTRUCT(BlueprintType)
struct NODETOCODE_API FN2CBatchTranslationResult
{
	GENERATED_BODY()

	/** Total number of items in the batch */
	UPROPERTY(BlueprintReadOnly, Category = "NodeToCode|Batch")
	int32 TotalCount = 0;

	/** Number of successfully translated items */
	UPROPERTY(BlueprintReadOnly, Category = "NodeToCode|Batch")
	int32 SuccessCount = 0;

	/** Number of failed items */
	UPROPERTY(BlueprintReadOnly, Category = "NodeToCode|Batch")
	int32 FailureCount = 0;

	/** Number of skipped items (e.g., due to cancellation) */
	UPROPERTY(BlueprintReadOnly, Category = "NodeToCode|Batch")
	int32 SkippedCount = 0;

	/** Path to the batch output directory */
	UPROPERTY(BlueprintReadOnly, Category = "NodeToCode|Batch")
	FString BatchOutputPath;

	/** Names of graphs that failed to translate */
	UPROPERTY(BlueprintReadOnly, Category = "NodeToCode|Batch")
	TArray<FString> FailedGraphNames;

	/** Total time taken for the batch in seconds */
	UPROPERTY(BlueprintReadOnly, Category = "NodeToCode|Batch")
	float TotalTimeSeconds = 0.0f;

	/** Total input tokens used across all translations */
	UPROPERTY(BlueprintReadOnly, Category = "NodeToCode|Batch")
	int32 TotalInputTokens = 0;

	/** Total output tokens used across all translations */
	UPROPERTY(BlueprintReadOnly, Category = "NodeToCode|Batch")
	int32 TotalOutputTokens = 0;

	FN2CBatchTranslationResult() = default;
};

/**
 * Per-item completion delegate with batch context
 * @param TagInfo The tag info for the completed item
 * @param Response The translation response
 * @param bSuccess Whether the translation was successful
 * @param ItemIndex The zero-based index of this item in the batch
 * @param TotalCount The total number of items in the batch
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(
	FOnBatchItemTranslationComplete,
	const FN2CTagInfo&, TagInfo,
	const FN2CTranslationResponse&, Response,
	bool, bSuccess,
	int32, ItemIndex,
	int32, TotalCount
);

/**
 * Batch completion delegate
 * @param Result The final batch result with statistics
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnBatchTranslationComplete,
	const FN2CBatchTranslationResult&, Result
);

/**
 * Progress delegate for UI updates
 * @param CurrentIndex The current item index being processed
 * @param TotalCount The total number of items
 * @param CurrentGraphName The name of the graph currently being processed
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnBatchTranslationProgress,
	int32, CurrentIndex,
	int32, TotalCount,
	const FString&, CurrentGraphName
);
