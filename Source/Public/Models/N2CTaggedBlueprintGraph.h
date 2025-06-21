// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "N2CTaggedBlueprintGraph.generated.h"

/**
 * @struct FN2CTaggedBlueprintGraph
 * @brief Represents a tag applied to a specific Blueprint graph
 * 
 * This struct stores information about tags applied to Blueprint graphs,
 * including the tag name, category, associated graph and Blueprint references,
 * and timestamp information.
 */
USTRUCT()
struct NODETOCODE_API FN2CTaggedBlueprintGraph
{
	GENERATED_BODY()

	/** The tag name applied to the graph */
	UPROPERTY()
	FString Tag;

	/** The category for organizing tags (default: "Default") */
	UPROPERTY()
	FString Category = TEXT("Default");

	/** Optional description or notes about the tag */
	UPROPERTY()
	FString Description;

	/** GUID of the tagged graph */
	UPROPERTY()
	FGuid GraphGuid;

	/** Name of the tagged graph */
	UPROPERTY()
	FString GraphName;

	/** Soft object reference to the owning Blueprint */
	UPROPERTY()
	FSoftObjectPath OwningBlueprint;

	/** UTC timestamp when the tag was applied */
	UPROPERTY()
	FDateTime Timestamp;

	/** Default constructor */
	FN2CTaggedBlueprintGraph()
	{
		Timestamp = FDateTime::UtcNow();
	}

	/**
	 * Constructor with parameters
	 * @param InTag The tag name
	 * @param InCategory The tag category
	 * @param InDescription Optional description
	 * @param InGraphGuid GUID of the graph
	 * @param InGraphName Name of the graph
	 * @param InOwningBlueprint Path to the owning Blueprint
	 */
	FN2CTaggedBlueprintGraph(
		const FString& InTag,
		const FString& InCategory,
		const FString& InDescription,
		const FGuid& InGraphGuid,
		const FString& InGraphName,
		const FSoftObjectPath& InOwningBlueprint)
		: Tag(InTag)
		, Category(InCategory)
		, Description(InDescription)
		, GraphGuid(InGraphGuid)
		, GraphName(InGraphName)
		, OwningBlueprint(InOwningBlueprint)
		, Timestamp(FDateTime::UtcNow())
	{
	}

	/** Check if this tag matches the given graph GUID */
	bool MatchesGraph(const FGuid& InGraphGuid) const
	{
		return GraphGuid == InGraphGuid;
	}

	/** Check if this tag matches the given tag name and category */
	bool MatchesTag(const FString& InTag, const FString& InCategory) const
	{
		return Tag.Equals(InTag, ESearchCase::IgnoreCase) && 
		       Category.Equals(InCategory, ESearchCase::IgnoreCase);
	}

	/** Get a display string for this tagged graph */
	FString GetDisplayString() const
	{
		return FString::Printf(TEXT("[%s] %s - %s (%s)"), 
			*Category, *Tag, *GraphName, *Timestamp.ToString());
	}

	/** Convert to JSON object for serialization */
	TSharedPtr<FJsonObject> ToJson() const;

	/** Create from JSON object */
	static FN2CTaggedBlueprintGraph FromJson(const TSharedPtr<FJsonObject>& JsonObject);
};