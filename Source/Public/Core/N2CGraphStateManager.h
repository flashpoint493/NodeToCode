// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/N2CGraphStateTypes.h"
#include "Models/N2CTaggedBlueprintGraph.h"
#include "Models/N2CTranslation.h"
#include "N2CGraphStateManager.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnN2CGraphStateChanged, const FGuid& /*GraphGuid*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnN2CGraphTagAdded, const FN2CGraphState& /*GraphState*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnN2CGraphTagRemoved, const FGuid& /*GraphGuid*/, const FString& /*Tag*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnN2CGraphTranslationUpdated, const FGuid& /*GraphGuid*/);

/**
 * @class UN2CGraphStateManager
 * @brief Singleton manager for unified Blueprint graph state tracking
 *
 * This class provides a single source of truth for tracking:
 * - Tags applied to Blueprint graphs
 * - Translation state (path, provider, model, language, summary)
 * - JSON export state (path, timestamp, minification)
 *
 * It replaces the previous tag-only system with a unified approach that enables
 * the UI to accurately reflect graph state, including whether translations exist.
 */
UCLASS()
class NODETOCODE_API UN2CGraphStateManager : public UObject
{
	GENERATED_BODY()

public:
	/** Get the singleton instance */
	static UN2CGraphStateManager& Get();

	/** Initialize the manager (loads persisted state, migrates legacy tags) */
	void Initialize();

	/** Shutdown the manager (saves state if dirty) */
	void Shutdown();

	// ========================================================================
	// Graph State Queries
	// ========================================================================

	/**
	 * Find graph state by GUID
	 * @param GraphGuid The unique identifier for the graph
	 * @return Pointer to the graph state, or nullptr if not found
	 */
	FN2CGraphState* FindGraphState(const FGuid& GraphGuid);
	const FN2CGraphState* FindGraphState(const FGuid& GraphGuid) const;

	/**
	 * Get all tracked graph states
	 * @return Array of all graph states
	 */
	const TArray<FN2CGraphState>& GetAllGraphStates() const { return StateFile.Graphs; }

	/**
	 * Get all graphs that have a specific tag
	 * @param Tag The tag name to search for
	 * @param Category Optional category filter (empty for all categories)
	 * @return Array of matching graph states
	 */
	TArray<FN2CGraphState> GetGraphsWithTag(const FString& Tag, const FString& Category = TEXT("")) const;

	/**
	 * Get all graphs in a specific category
	 * @param Category The category name
	 * @return Array of graph states with tags in the category
	 */
	TArray<FN2CGraphState> GetGraphsInCategory(const FString& Category) const;

	/**
	 * Get all graphs that have translations
	 * @return Array of graph states with bExists == true
	 */
	TArray<FN2CGraphState> GetGraphsWithTranslation() const;

	/**
	 * Get all unique tag names across all graphs
	 * @return Array of unique tag names
	 */
	TArray<FString> GetAllTagNames() const;

	/**
	 * Get all unique categories across all graphs
	 * @return Array of unique category names
	 */
	TArray<FString> GetAllCategories() const;

	// ========================================================================
	// Tag Operations (preserves existing API for backward compatibility)
	// ========================================================================

	/**
	 * Add a tag to a graph
	 * @param GraphGuid The unique identifier for the graph
	 * @param GraphName The human-readable graph name
	 * @param Blueprint The owning Blueprint asset
	 * @param Tag The tag name
	 * @param Category The tag category
	 * @param Description Optional description
	 * @return True if the tag was added
	 */
	bool AddTag(const FGuid& GraphGuid, const FString& GraphName,
		const FSoftObjectPath& Blueprint, const FString& Tag,
		const FString& Category, const FString& Description = TEXT(""));

	/**
	 * Add a tag using the legacy structure (for backward compatibility)
	 * @param TaggedGraph The tag information
	 * @return True if the tag was added
	 */
	bool AddTag(const FN2CTaggedBlueprintGraph& TaggedGraph);

	/**
	 * Remove a specific tag from a graph
	 * @param GraphGuid The GUID of the graph
	 * @param Tag The tag name
	 * @param Category The tag category
	 * @return True if removed
	 */
	bool RemoveTag(const FGuid& GraphGuid, const FString& Tag, const FString& Category);

	/**
	 * Remove all instances of a tag from a graph (ignoring category)
	 * @param GraphGuid The GUID of the graph
	 * @param Tag The tag name
	 * @return Number of tags removed
	 */
	int32 RemoveTagByName(const FGuid& GraphGuid, const FString& Tag);

	/**
	 * Remove all tags from a graph (does not remove translation/export state)
	 * @param GraphGuid The GUID of the graph
	 * @return Number of tags removed
	 */
	int32 RemoveAllTagsFromGraph(const FGuid& GraphGuid);

	/**
	 * Check if a graph has a specific tag
	 * @param GraphGuid The GUID of the graph
	 * @param Tag The tag name
	 * @param Category The tag category (empty for any category)
	 * @return True if the tag exists
	 */
	bool GraphHasTag(const FGuid& GraphGuid, const FString& Tag, const FString& Category = TEXT("")) const;

	/**
	 * Get all tags for a specific graph
	 * @param GraphGuid The GUID of the graph
	 * @return Array of tag entries
	 */
	TArray<FN2CTagEntry> GetTagsForGraph(const FGuid& GraphGuid) const;

	/**
	 * Get all tags as legacy format (for backward compatibility)
	 * @return Array of all tags in FN2CTaggedBlueprintGraph format
	 */
	TArray<FN2CTaggedBlueprintGraph> GetAllTagsLegacy() const;

	// ========================================================================
	// Translation State Operations
	// ========================================================================

	/**
	 * Set/update the translation state for a graph
	 * @param GraphGuid The GUID of the graph
	 * @param State The translation state to set
	 */
	void SetTranslationState(const FGuid& GraphGuid, const FN2CTranslationState& State);

	/**
	 * Set translation state with individual parameters
	 * @param GraphGuid The GUID of the graph
	 * @param GraphName The graph name (for creating new state if needed)
	 * @param Blueprint The owning Blueprint
	 * @param OutputPath Path to translation output directory
	 * @param Provider LLM provider name
	 * @param Model Model name
	 * @param Language Target language
	 * @param Summary Translation summary metadata
	 */
	void SetTranslationState(const FGuid& GraphGuid, const FString& GraphName,
		const FSoftObjectPath& Blueprint, const FString& OutputPath,
		const FString& Provider, const FString& Model, const FString& Language,
		const FN2CTranslationSummary& Summary);

	/**
	 * Clear the translation state for a graph
	 * @param GraphGuid The GUID of the graph
	 */
	void ClearTranslationState(const FGuid& GraphGuid);

	/**
	 * Check if a graph has a translation
	 * @param GraphGuid The GUID of the graph
	 * @return True if translation exists
	 */
	bool HasTranslation(const FGuid& GraphGuid) const;

	/**
	 * Load a translation from disk
	 * @param GraphGuid The GUID of the graph
	 * @param OutTranslation The loaded translation data
	 * @return True if loaded successfully
	 */
	bool LoadTranslation(const FGuid& GraphGuid, FN2CGraphTranslation& OutTranslation) const;

	/**
	 * Load the N2C JSON representation of a graph from disk
	 * @param GraphGuid The GUID of the graph
	 * @param OutJsonContent The loaded JSON content as a string
	 * @return True if loaded successfully
	 */
	bool LoadN2CJson(const FGuid& GraphGuid, FString& OutJsonContent) const;

	// ========================================================================
	// JSON Export State Operations
	// ========================================================================

	/**
	 * Set/update the JSON export state for a graph
	 * @param GraphGuid The GUID of the graph
	 * @param State The export state to set
	 */
	void SetJsonExportState(const FGuid& GraphGuid, const FN2CJsonExportState& State);

	/**
	 * Set JSON export state with individual parameters
	 * @param GraphGuid The GUID of the graph
	 * @param GraphName The graph name (for creating new state if needed)
	 * @param Blueprint The owning Blueprint
	 * @param OutputPath Path to exported JSON file
	 * @param bMinified Whether the JSON was minified
	 */
	void SetJsonExportState(const FGuid& GraphGuid, const FString& GraphName,
		const FSoftObjectPath& Blueprint, const FString& OutputPath, bool bMinified);

	/**
	 * Clear the JSON export state for a graph
	 * @param GraphGuid The GUID of the graph
	 */
	void ClearJsonExportState(const FGuid& GraphGuid);

	/**
	 * Check if a graph has been exported to JSON
	 * @param GraphGuid The GUID of the graph
	 * @return True if export exists
	 */
	bool HasJsonExport(const FGuid& GraphGuid) const;

	// ========================================================================
	// Graph State Lifecycle
	// ========================================================================

	/**
	 * Ensure a graph state exists, creating it if needed
	 * @param GraphGuid The GUID of the graph
	 * @param GraphName The graph name
	 * @param Blueprint The owning Blueprint
	 * @return Reference to the graph state
	 */
	FN2CGraphState& GetOrCreateGraphState(const FGuid& GraphGuid, const FString& GraphName,
		const FSoftObjectPath& Blueprint);

	/**
	 * Remove a graph state entirely (tags, translation, export state)
	 * @param GraphGuid The GUID of the graph
	 * @return True if removed
	 */
	bool RemoveGraphState(const FGuid& GraphGuid);

	/**
	 * Clear all state (use with caution)
	 */
	void ClearAllState();

	// ========================================================================
	// Persistence
	// ========================================================================

	/** Save state to disk */
	bool SaveState();

	/** Load state from disk */
	bool LoadState();

	/** Check if there are unsaved changes */
	bool IsDirty() const { return bIsDirty; }

	/** Migrate from legacy BlueprintTags.json file */
	void MigrateFromLegacyTags();

	// ========================================================================
	// Delegates
	// ========================================================================

	/** Fired when any graph state changes */
	FOnN2CGraphStateChanged OnGraphStateChanged;

	/** Fired when a tag is added */
	FOnN2CGraphTagAdded OnGraphTagAdded;

	/** Fired when a tag is removed */
	FOnN2CGraphTagRemoved OnGraphTagRemoved;

	/** Fired when translation state is updated */
	FOnN2CGraphTranslationUpdated OnGraphTranslationUpdated;

private:
	/** Constructor */
	UN2CGraphStateManager();

	/** Get the file path for state persistence */
	FString GetStateFilePath() const;

	/** Get the legacy tags file path */
	FString GetLegacyTagsFilePath() const;

	/** Mark state as modified */
	void MarkDirty();

	/** The singleton instance */
	static UN2CGraphStateManager* Instance;

	/** The state file data */
	UPROPERTY()
	FN2CGraphStateFile StateFile;

	/** Flag indicating if state has been modified since last save */
	bool bIsDirty;

	/** Flag to track if migration has been attempted */
	bool bMigrationAttempted;
};
