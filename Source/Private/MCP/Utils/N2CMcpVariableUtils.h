// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphPin.h"
#include "Dom/JsonObject.h"

// Forward declarations
class FN2CMcpArgumentParser;

/**
 * Shared utilities for MCP variable creation tools
 * Provides common functionality for both member variables and local variables
 */
class FN2CMcpVariableUtils
{
public:
	/**
	 * Validates that a key type is valid for use in Blueprint TMap containers
	 * @param KeyType The type identifier to validate
	 * @param OutError Error message if validation fails
	 * @return true if the key type is valid, false otherwise
	 */
	static bool ValidateMapKeyType(const FString& KeyType, FString& OutError);

	/**
	 * Validates container type and key type combination
	 * @param ContainerType The container type (none, array, set, map)
	 * @param KeyType The key type (only used for maps)
	 * @param OutError Error message if validation fails
	 * @return true if the combination is valid, false otherwise
	 */
	static bool ValidateContainerTypeParameters(const FString& ContainerType, const FString& KeyType, FString& OutError);

	/**
	 * Adds container type properties to the input schema
	 * @param Properties The properties JSON object to add to
	 */
	static void AddContainerTypeSchemaProperties(TSharedPtr<FJsonObject> Properties);

	/**
	 * Parses container type parameters from arguments
	 * @param ArgParser The argument parser
	 * @param OutContainerType The parsed container type
	 * @param OutKeyType The parsed key type
	 */
	static void ParseContainerTypeArguments(const FN2CMcpArgumentParser& ArgParser, FString& OutContainerType, FString& OutKeyType);

	/**
	 * Adds container type information to a success result
	 * @param Result The result JSON object
	 * @param ContainerType The container type used
	 * @param bIsLocalVariable Whether this is a local variable (affects the note message)
	 */
	static void AddContainerInfoToResult(TSharedPtr<FJsonObject> Result, const FString& ContainerType, bool bIsLocalVariable = false);

	/**
	 * Builds type info JSON object from a pin type
	 * @param PinType The pin type to describe
	 * @return JSON object with type information
	 */
	static TSharedPtr<FJsonObject> BuildTypeInfo(const FEdGraphPinType& PinType);

private:
	// Private constructor to prevent instantiation
	FN2CMcpVariableUtils() = delete;
};