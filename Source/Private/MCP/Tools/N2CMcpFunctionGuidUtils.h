// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Forward declarations
class UEdGraph;
class UK2Node_FunctionEntry;
class UBlueprint;

/**
 * Utility class for managing function GUIDs consistently across MCP tools
 * Provides methods to generate, store, and retrieve GUIDs for Blueprint functions
 */
class FN2CMcpFunctionGuidUtils
{
public:
	/**
	 * Gets or creates a GUID for a function graph
	 * If a GUID already exists in metadata, it returns that GUID
	 * Otherwise, it generates a deterministic GUID and stores it
	 */
	static FGuid GetOrCreateFunctionGuid(UEdGraph* FunctionGraph);
	
	/**
	 * Gets the stored GUID for a function graph
	 * Returns an invalid GUID if none is stored
	 */
	static FGuid GetStoredFunctionGuid(const UEdGraph* FunctionGraph);
	
	/**
	 * Stores a GUID for a function graph in its metadata
	 */
	static void StoreFunctionGuid(UEdGraph* FunctionGraph, const FGuid& Guid);
	
	/**
	 * Generates a deterministic GUID for a function based on Blueprint and function name
	 * This ensures the same function always gets the same GUID
	 */
	static FGuid GenerateDeterministicGuid(const UBlueprint* Blueprint, const FString& FunctionName);
	
	/**
	 * Searches for a function by GUID in a Blueprint
	 * Returns the function graph if found, nullptr otherwise
	 */
	static UEdGraph* FindFunctionByGuid(UBlueprint* Blueprint, const FGuid& FunctionGuid);
	
private:
	// Metadata key for storing GUIDs
	static const FString GuidMetadataKey;
	
	/**
	 * Gets the function entry node from a function graph
	 */
	static UK2Node_FunctionEntry* GetFunctionEntryNode(UEdGraph* FunctionGraph);
};