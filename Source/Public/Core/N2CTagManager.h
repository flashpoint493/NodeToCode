// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Models/N2CTaggedBlueprintGraph.h"
#include "N2CTagManager.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnBlueprintTagAdded, const FN2CTaggedBlueprintGraph&);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnBlueprintTagRemoved, const FGuid& /*GraphGuid*/, const FString& /*Tag*/);

/**
 * @class UN2CTagManager
 * @brief Singleton manager for Blueprint graph tagging functionality
 * 
 * This class manages tags applied to Blueprint graphs, providing functionality
 * to add, remove, query, and persist tags. It maintains an in-memory cache
 * of tags and handles JSON file persistence.
 */
UCLASS()
class NODETOCODE_API UN2CTagManager : public UObject
{
	GENERATED_BODY()

public:
	/** Get the singleton instance */
	static UN2CTagManager& Get();

	/** Initialize the tag manager (loads persisted tags) */
	void Initialize();

	/** Shutdown the tag manager (saves tags) */
	void Shutdown();

	/**
	 * Add a tag to a Blueprint graph
	 * @param TaggedGraph The tag information to add
	 * @return True if the tag was added successfully
	 */
	bool AddTag(const FN2CTaggedBlueprintGraph& TaggedGraph);

	/**
	 * Remove a specific tag from a graph
	 * @param GraphGuid The GUID of the graph
	 * @param Tag The tag name to remove
	 * @param Category The tag category
	 * @return True if the tag was removed
	 */
	bool RemoveTag(const FGuid& GraphGuid, const FString& Tag, const FString& Category);

	/**
	 * Remove tag(s) from a graph by tag name only (removes all matching tags regardless of category)
	 * @param GraphGuid The GUID of the graph
	 * @param Tag The tag name to remove
	 * @param OutRemovedTag If successful, contains the removed tag info (first one if multiple)
	 * @return Number of tags removed (0 if none found)
	 */
	int32 RemoveTagByName(const FGuid& GraphGuid, const FString& Tag, FN2CTaggedBlueprintGraph& OutRemovedTag);

	/**
	 * Remove all tags from a graph
	 * @param GraphGuid The GUID of the graph
	 * @return Number of tags removed
	 */
	int32 RemoveAllTagsFromGraph(const FGuid& GraphGuid);

	/**
	 * Get all tags for a specific graph
	 * @param GraphGuid The GUID of the graph
	 * @return Array of tags for the graph
	 */
	TArray<FN2CTaggedBlueprintGraph> GetTagsForGraph(const FGuid& GraphGuid) const;

	/**
	 * Get all graphs with a specific tag
	 * @param Tag The tag name
	 * @param Category Optional category filter (empty string for all categories)
	 * @return Array of tagged graphs
	 */
	TArray<FN2CTaggedBlueprintGraph> GetGraphsWithTag(const FString& Tag, const FString& Category = TEXT("")) const;

	/**
	 * Get all tags in a specific category
	 * @param Category The category name
	 * @return Array of tagged graphs in the category
	 */
	TArray<FN2CTaggedBlueprintGraph> GetTagsInCategory(const FString& Category) const;

	/**
	 * Get all unique tag names
	 * @return Array of unique tag names
	 */
	TArray<FString> GetAllTagNames() const;

	/**
	 * Get all unique categories
	 * @return Array of unique category names
	 */
	TArray<FString> GetAllCategories() const;

	/**
	 * Check if a graph has a specific tag
	 * @param GraphGuid The GUID of the graph
	 * @param Tag The tag name
	 * @param Category The tag category
	 * @return True if the graph has the tag
	 */
	bool GraphHasTag(const FGuid& GraphGuid, const FString& Tag, const FString& Category) const;

	/** Get all tags in the system */
	const TArray<FN2CTaggedBlueprintGraph>& GetAllTags() const { return Tags; }

	/** Clear all tags (use with caution) */
	void ClearAllTags();

	/** Save tags to persistent storage */
	bool SaveTags();

	/** Load tags from persistent storage */
	bool LoadTags();

	/** Delegate fired when a tag is added */
	FOnBlueprintTagAdded OnBlueprintTagAdded;

	/** Delegate fired when a tag is removed */
	FOnBlueprintTagRemoved OnBlueprintTagRemoved;

private:
	/** Constructor */
	UN2CTagManager();

	/** Get the file path for tag persistence */
	FString GetTagsFilePath() const;

	/** The singleton instance */
	static UN2CTagManager* Instance;

	/** Array of all tags */
	UPROPERTY()
	TArray<FN2CTaggedBlueprintGraph> Tags;

	/** Flag indicating if tags have been modified since last save */
	bool bIsDirty;
};