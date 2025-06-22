// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_EditablePinBase.h" // Need full definition for FUserPinInfo

// Forward declarations
class UK2Node_FunctionEntry;
class UK2Node_FunctionResult;
class UEdGraph;
class UEdGraphPin;
class UBlueprint;

/**
 * Utility class for common Blueprint function pin operations shared between
 * AddFunctionInputPin and AddFunctionReturnPin tools.
 */
class FN2CMcpFunctionPinUtils
{
public:
	/**
	 * Finds the function entry node in a graph.
	 * @param Graph The graph to search
	 * @return The function entry node, or nullptr if not found
	 */
	static UK2Node_FunctionEntry* FindFunctionEntryNode(UEdGraph* Graph);
	
	/**
	 * Finds the function result node in a graph.
	 * @param Graph The graph to search
	 * @return The function result node, or nullptr if not found
	 */
	static UK2Node_FunctionResult* FindFunctionResultNode(UEdGraph* Graph);
	
	/**
	 * Ensures a function has a result node, creating one if necessary.
	 * @param FunctionGraph The function graph
	 * @return The function result node (existing or newly created)
	 */
	static UK2Node_FunctionResult* EnsureFunctionResultNode(UEdGraph* FunctionGraph);
	
	/**
	 * Updates all call sites of a function after its signature has changed.
	 * @param FunctionGraph The function graph that was modified
	 * @param Blueprint The blueprint containing the function
	 */
	static void UpdateFunctionCallSites(UEdGraph* FunctionGraph, UBlueprint* Blueprint);
	
	/**
	 * Sets the tooltip for a pin. Handles the differences between pin types.
	 * @param Node The node containing the pin
	 * @param Pin The pin to set the tooltip on
	 * @param Tooltip The tooltip text
	 */
	static void SetPinTooltip(UK2Node_EditablePinBase* Node, UEdGraphPin* Pin, const FString& Tooltip);
	
	/**
	 * Builds a standard success result JSON object for pin creation operations.
	 * @param FunctionGraph The function graph
	 * @param RequestedName The originally requested pin name
	 * @param CreatedPin The actually created pin
	 * @param PinType The type of the created pin
	 * @param bIsReturnPin Whether this is a return pin (vs input parameter)
	 * @return JSON object with success information
	 */
	static TSharedPtr<FJsonObject> BuildPinCreationSuccessResult(
		UEdGraph* FunctionGraph,
		const FString& RequestedName,
		UEdGraphPin* CreatedPin,
		const FEdGraphPinType& PinType,
		bool bIsReturnPin
	);
	
	/**
	 * Validates that a pin can be removed from a node.
	 * @param Node The node containing the pin
	 * @param Pin The pin to validate
	 * @param OutError Error message if validation fails
	 * @return True if the pin can be removed, false otherwise
	 */
	static bool ValidatePinForRemoval(UK2Node_EditablePinBase* Node, UEdGraphPin* Pin, FString& OutError);
	
	/**
	 * Removes a user-defined pin from a function node.
	 * @param Node The node to remove the pin from
	 * @param PinName The name of the pin to remove
	 * @param OutError Error message if removal fails
	 * @return True if the pin was removed successfully, false otherwise
	 */
	static bool RemoveFunctionPin(UK2Node_EditablePinBase* Node, const FString& PinName, FString& OutError);
	
	/**
	 * Builds a standard success result JSON object for pin removal operations.
	 * @param FunctionGraph The function graph
	 * @param RemovedPinName The name of the removed pin
	 * @param bIsReturnPin Whether this was a return pin (vs input parameter)
	 * @return JSON object with success information
	 */
	static TSharedPtr<FJsonObject> BuildPinRemovalSuccessResult(
		UEdGraph* FunctionGraph,
		const FString& RemovedPinName,
		bool bIsReturnPin
	);
};