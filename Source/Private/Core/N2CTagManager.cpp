// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#include "Core/N2CTagManager.h"
#include "Utils/N2CLogger.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"

UN2CTagManager* UN2CTagManager::Instance = nullptr;

UN2CTagManager& UN2CTagManager::Get()
{
	if (!Instance)
	{
		Instance = NewObject<UN2CTagManager>(GetTransientPackage(), UN2CTagManager::StaticClass(), NAME_None, RF_Standalone);
		Instance->AddToRoot(); // Prevent garbage collection
		Instance->Initialize();
	}
	return *Instance;
}

UN2CTagManager::UN2CTagManager()
	: bIsDirty(false)
{
}

void UN2CTagManager::Initialize()
{
	FN2CLogger::Get().Log(TEXT("Initializing Tag Manager"), EN2CLogSeverity::Info);
	LoadTags();
}

void UN2CTagManager::Shutdown()
{
	FN2CLogger::Get().Log(TEXT("Shutting down Tag Manager"), EN2CLogSeverity::Info);
	if (bIsDirty)
	{
		SaveTags();
	}
	
	if (Instance)
	{
		Instance->RemoveFromRoot();
		Instance = nullptr;
	}
}

bool UN2CTagManager::AddTag(const FN2CTaggedBlueprintGraph& TaggedGraph)
{
	// Check if this exact tag already exists (idempotent operation)
	for (const FN2CTaggedBlueprintGraph& ExistingTag : Tags)
	{
		if (ExistingTag.MatchesGraph(TaggedGraph.GraphGuid) && 
		    ExistingTag.MatchesTag(TaggedGraph.Tag, TaggedGraph.Category))
		{
			FN2CLogger::Get().Log(FString::Printf(TEXT("Tag '%s' in category '%s' already exists for graph %s"), 
				*TaggedGraph.Tag, *TaggedGraph.Category, *TaggedGraph.GraphGuid.ToString()), 
				EN2CLogSeverity::Warning);
			
			// Still return true as the desired state is achieved
			return true;
		}
	}
	
	// Add the new tag
	Tags.Add(TaggedGraph);
	bIsDirty = true;
	
	FN2CLogger::Get().Log(FString::Printf(TEXT("Added tag '%s' in category '%s' to graph %s"), 
		*TaggedGraph.Tag, *TaggedGraph.Category, *TaggedGraph.GraphGuid.ToString()), 
		EN2CLogSeverity::Info);
	
	// Fire delegate
	OnBlueprintTagAdded.Broadcast(TaggedGraph);
	
	// Auto-save if enabled
	SaveTags();
	
	return true;
}

bool UN2CTagManager::RemoveTag(const FGuid& GraphGuid, const FString& Tag, const FString& Category)
{
	bool bRemoved = false;
	
	for (int32 i = Tags.Num() - 1; i >= 0; --i)
	{
		if (Tags[i].MatchesGraph(GraphGuid) && Tags[i].MatchesTag(Tag, Category))
		{
			Tags.RemoveAt(i);
			bRemoved = true;
			bIsDirty = true;
			
			FN2CLogger::Get().Log(FString::Printf(TEXT("Removed tag '%s' in category '%s' from graph %s"), 
				*Tag, *Category, *GraphGuid.ToString()), 
				EN2CLogSeverity::Info);
			
			// Fire delegate
			OnBlueprintTagRemoved.Broadcast(GraphGuid, Tag);
			break;
		}
	}
	
	if (bRemoved)
	{
		SaveTags();
	}
	
	return bRemoved;
}

int32 UN2CTagManager::RemoveAllTagsFromGraph(const FGuid& GraphGuid)
{
	int32 RemovedCount = 0;
	
	for (int32 i = Tags.Num() - 1; i >= 0; --i)
	{
		if (Tags[i].MatchesGraph(GraphGuid))
		{
			FString Tag = Tags[i].Tag;
			Tags.RemoveAt(i);
			RemovedCount++;
			
			// Fire delegate
			OnBlueprintTagRemoved.Broadcast(GraphGuid, Tag);
		}
	}
	
	if (RemovedCount > 0)
	{
		bIsDirty = true;
		SaveTags();
		
		FN2CLogger::Get().Log(FString::Printf(TEXT("Removed %d tags from graph %s"), 
			RemovedCount, *GraphGuid.ToString()), 
			EN2CLogSeverity::Info);
	}
	
	return RemovedCount;
}

TArray<FN2CTaggedBlueprintGraph> UN2CTagManager::GetTagsForGraph(const FGuid& GraphGuid) const
{
	TArray<FN2CTaggedBlueprintGraph> Result;
	
	for (const FN2CTaggedBlueprintGraph& Tag : Tags)
	{
		if (Tag.MatchesGraph(GraphGuid))
		{
			Result.Add(Tag);
		}
	}
	
	return Result;
}

TArray<FN2CTaggedBlueprintGraph> UN2CTagManager::GetGraphsWithTag(const FString& Tag, const FString& Category) const
{
	TArray<FN2CTaggedBlueprintGraph> Result;
	
	for (const FN2CTaggedBlueprintGraph& TaggedGraph : Tags)
	{
		bool bMatches = TaggedGraph.Tag.Equals(Tag, ESearchCase::IgnoreCase);
		
		// If category is specified, also check category
		if (!Category.IsEmpty())
		{
			bMatches = bMatches && TaggedGraph.Category.Equals(Category, ESearchCase::IgnoreCase);
		}
		
		if (bMatches)
		{
			Result.Add(TaggedGraph);
		}
	}
	
	return Result;
}

TArray<FN2CTaggedBlueprintGraph> UN2CTagManager::GetTagsInCategory(const FString& Category) const
{
	TArray<FN2CTaggedBlueprintGraph> Result;
	
	for (const FN2CTaggedBlueprintGraph& Tag : Tags)
	{
		if (Tag.Category.Equals(Category, ESearchCase::IgnoreCase))
		{
			Result.Add(Tag);
		}
	}
	
	return Result;
}

TArray<FString> UN2CTagManager::GetAllTagNames() const
{
	TSet<FString> UniqueNames;
	
	for (const FN2CTaggedBlueprintGraph& Tag : Tags)
	{
		UniqueNames.Add(Tag.Tag);
	}
	
	return UniqueNames.Array();
}

TArray<FString> UN2CTagManager::GetAllCategories() const
{
	TSet<FString> UniqueCategories;
	
	for (const FN2CTaggedBlueprintGraph& Tag : Tags)
	{
		UniqueCategories.Add(Tag.Category);
	}
	
	return UniqueCategories.Array();
}

bool UN2CTagManager::GraphHasTag(const FGuid& GraphGuid, const FString& Tag, const FString& Category) const
{
	for (const FN2CTaggedBlueprintGraph& TaggedGraph : Tags)
	{
		if (TaggedGraph.MatchesGraph(GraphGuid) && TaggedGraph.MatchesTag(Tag, Category))
		{
			return true;
		}
	}
	
	return false;
}

void UN2CTagManager::ClearAllTags()
{
	int32 TagCount = Tags.Num();
	Tags.Empty();
	bIsDirty = true;
	
	FN2CLogger::Get().Log(FString::Printf(TEXT("Cleared all %d tags"), TagCount), 
		EN2CLogSeverity::Info);
}

FString UN2CTagManager::GetTagsFilePath() const
{
	FString ProjectSavedDir = FPaths::ProjectSavedDir();
	FString TagsDir = FPaths::Combine(ProjectSavedDir, TEXT("NodeToCode"), TEXT("Tags"));
	
	// Ensure directory exists
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.CreateDirectoryTree(*TagsDir);
	
	return FPaths::Combine(TagsDir, TEXT("BlueprintTags.json"));
}

bool UN2CTagManager::SaveTags()
{
	FString FilePath = GetTagsFilePath();
	
	// Create JSON array
	TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject);
	TArray<TSharedPtr<FJsonValue>> TagsArray;
	
	for (const FN2CTaggedBlueprintGraph& Tag : Tags)
	{
		TagsArray.Add(MakeShareable(new FJsonValueObject(Tag.ToJson())));
	}
	
	RootObject->SetArrayField(TEXT("tags"), TagsArray);
	RootObject->SetStringField(TEXT("version"), TEXT("1.0"));
	RootObject->SetStringField(TEXT("lastSaved"), FDateTime::UtcNow().ToIso8601());
	
	// Serialize to string
	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	if (!FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer))
	{
		FN2CLogger::Get().LogError(TEXT("Failed to serialize tags to JSON"));
		return false;
	}
	
	// Save to file
	if (!FFileHelper::SaveStringToFile(JsonString, *FilePath))
	{
		FN2CLogger::Get().LogError(FString::Printf(TEXT("Failed to save tags to file: %s"), *FilePath));
		return false;
	}
	
	bIsDirty = false;
	FN2CLogger::Get().Log(FString::Printf(TEXT("Saved %d tags to %s"), Tags.Num(), *FilePath), 
		EN2CLogSeverity::Info);
	
	return true;
}

bool UN2CTagManager::LoadTags()
{
	FString FilePath = GetTagsFilePath();
	
	// Check if file exists
	if (!FPaths::FileExists(FilePath))
	{
		FN2CLogger::Get().Log(TEXT("No tags file found, starting with empty tags"), EN2CLogSeverity::Info);
		return true; // Not an error, just no tags yet
	}
	
	// Load file content
	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
	{
		FN2CLogger::Get().LogError(FString::Printf(TEXT("Failed to load tags from file: %s"), *FilePath));
		return false;
	}
	
	// Parse JSON
	TSharedPtr<FJsonObject> RootObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		FN2CLogger::Get().LogError(TEXT("Failed to parse tags JSON"));
		return false;
	}
	
	// Clear existing tags
	Tags.Empty();
	
	// Load tags from JSON
	const TArray<TSharedPtr<FJsonValue>>* TagsArray;
	if (RootObject->TryGetArrayField(TEXT("tags"), TagsArray))
	{
		for (const TSharedPtr<FJsonValue>& Value : *TagsArray)
		{
			TSharedPtr<FJsonObject> TagObject = Value->AsObject();
			if (TagObject.IsValid())
			{
				Tags.Add(FN2CTaggedBlueprintGraph::FromJson(TagObject));
			}
		}
	}
	
	bIsDirty = false;
	FN2CLogger::Get().Log(FString::Printf(TEXT("Loaded %d tags from %s"), Tags.Num(), *FilePath), 
		EN2CLogSeverity::Info);
	
	return true;
}