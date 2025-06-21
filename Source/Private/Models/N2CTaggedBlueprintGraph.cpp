// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#include "Models/N2CTaggedBlueprintGraph.h"
#include "Dom/JsonObject.h"

TSharedPtr<FJsonObject> FN2CTaggedBlueprintGraph::ToJson() const
{
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
	
	JsonObject->SetStringField(TEXT("tag"), Tag);
	JsonObject->SetStringField(TEXT("category"), Category);
	JsonObject->SetStringField(TEXT("description"), Description);
	JsonObject->SetStringField(TEXT("graphGuid"), GraphGuid.ToString(EGuidFormats::DigitsWithHyphens));
	JsonObject->SetStringField(TEXT("graphName"), GraphName);
	JsonObject->SetStringField(TEXT("owningBlueprint"), OwningBlueprint.ToString());
	JsonObject->SetStringField(TEXT("timestamp"), Timestamp.ToIso8601());
	
	return JsonObject;
}

FN2CTaggedBlueprintGraph FN2CTaggedBlueprintGraph::FromJson(const TSharedPtr<FJsonObject>& JsonObject)
{
	FN2CTaggedBlueprintGraph Result;
	
	if (JsonObject.IsValid())
	{
		JsonObject->TryGetStringField(TEXT("tag"), Result.Tag);
		JsonObject->TryGetStringField(TEXT("category"), Result.Category);
		JsonObject->TryGetStringField(TEXT("description"), Result.Description);
		
		FString GraphGuidString;
		if (JsonObject->TryGetStringField(TEXT("graphGuid"), GraphGuidString))
		{
			FGuid::Parse(GraphGuidString, Result.GraphGuid);
		}
		
		JsonObject->TryGetStringField(TEXT("graphName"), Result.GraphName);
		
		FString OwningBlueprintString;
		if (JsonObject->TryGetStringField(TEXT("owningBlueprint"), OwningBlueprintString))
		{
			Result.OwningBlueprint = FSoftObjectPath(OwningBlueprintString);
		}
		
		FString TimestampString;
		if (JsonObject->TryGetStringField(TEXT("timestamp"), TimestampString))
		{
			FDateTime::ParseIso8601(*TimestampString, Result.Timestamp);
		}
	}
	
	return Result;
}