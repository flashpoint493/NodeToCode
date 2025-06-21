// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#include "N2CMcpTagUtils.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

bool FN2CMcpTagUtils::ValidateAndParseGuid(const FString& GuidString, FGuid& OutGuid, FString& OutErrorMsg)
{
	if (GuidString.IsEmpty())
	{
		OutErrorMsg = TEXT("GUID string is empty");
		return false;
	}

	if (!FGuid::Parse(GuidString, OutGuid))
	{
		OutErrorMsg = TEXT("Invalid GUID format");
		return false;
	}

	if (!OutGuid.IsValid())
	{
		OutErrorMsg = TEXT("GUID is not valid");
		return false;
	}

	return true;
}

TSharedPtr<FJsonObject> FN2CMcpTagUtils::TagToJsonObject(const FN2CTaggedBlueprintGraph& Tag)
{
	TSharedPtr<FJsonObject> TagObject = MakeShareable(new FJsonObject);
	TagObject->SetStringField(TEXT("tag"), Tag.Tag);
	TagObject->SetStringField(TEXT("category"), Tag.Category);
	TagObject->SetStringField(TEXT("description"), Tag.Description);
	TagObject->SetStringField(TEXT("graphGuid"), Tag.GraphGuid.ToString(EGuidFormats::DigitsWithHyphens));
	TagObject->SetStringField(TEXT("graphName"), Tag.GraphName);
	TagObject->SetStringField(TEXT("blueprintPath"), Tag.OwningBlueprint.ToString());
	TagObject->SetStringField(TEXT("timestamp"), Tag.Timestamp.ToIso8601());
	return TagObject;
}

TSharedPtr<FJsonObject> FN2CMcpTagUtils::CreateBaseResponse(bool bSuccess, const FString& Message)
{
	TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
	Response->SetBoolField(TEXT("success"), bSuccess);
	Response->SetStringField(TEXT("message"), Message);
	return Response;
}

bool FN2CMcpTagUtils::SerializeToJsonString(const TSharedPtr<FJsonObject>& JsonObject, FString& OutJsonString)
{
	if (!JsonObject.IsValid())
	{
		return false;
	}

	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJsonString);
	return FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
}