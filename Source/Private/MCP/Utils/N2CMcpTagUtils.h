// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Models/N2CTaggedBlueprintGraph.h"
#include "Dom/JsonObject.h"

/**
 * @class FN2CMcpTagUtils
 * @brief Utility functions for MCP tag operations
 * 
 * This class provides common utility functions used by MCP tools that work with
 * Blueprint graph tags, reducing code duplication and ensuring consistency.
 */
class FN2CMcpTagUtils
{
public:
	/**
	 * Validates a GUID string and parses it into a FGuid
	 * @param GuidString The string to validate and parse
	 * @param OutGuid The parsed GUID if successful
	 * @param OutErrorMsg Error message if validation fails
	 * @return True if the GUID is valid, false otherwise
	 */
	static bool ValidateAndParseGuid(const FString& GuidString, FGuid& OutGuid, FString& OutErrorMsg);

	/**
	 * Converts a FN2CTaggedBlueprintGraph to a JSON object
	 * @param Tag The tag to convert
	 * @return JSON representation of the tag
	 */
	static TSharedPtr<FJsonObject> TagToJsonObject(const FN2CTaggedBlueprintGraph& Tag);

	/**
	 * Creates a standard success response JSON object for tag operations
	 * @param bSuccess Whether the operation was successful
	 * @param Message The message to include
	 * @return JSON object with success and message fields
	 */
	static TSharedPtr<FJsonObject> CreateBaseResponse(bool bSuccess, const FString& Message);

	/**
	 * Serializes a JSON object to a string using standard formatting
	 * @param JsonObject The object to serialize
	 * @param OutJsonString The resulting JSON string
	 * @return True if serialization was successful
	 */
	static bool SerializeToJsonString(const TSharedPtr<FJsonObject>& JsonObject, FString& OutJsonString);
};