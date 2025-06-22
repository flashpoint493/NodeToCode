// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#include "N2CMcpFunctionPinUtils.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_CallFunction.h"
#include "K2Node_EditablePinBase.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Dom/JsonObject.h"
#include "Utils/N2CLogger.h"

UK2Node_FunctionEntry* FN2CMcpFunctionPinUtils::FindFunctionEntryNode(UEdGraph* Graph)
{
	if (!Graph)
	{
		return nullptr;
	}

	// Find the function entry node in the graph
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(Node))
		{
			return EntryNode;
		}
	}

	return nullptr;
}

UK2Node_FunctionResult* FN2CMcpFunctionPinUtils::FindFunctionResultNode(UEdGraph* Graph)
{
	if (!Graph)
	{
		return nullptr;
	}

	// Find the function result node in the graph
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (UK2Node_FunctionResult* ResultNode = Cast<UK2Node_FunctionResult>(Node))
		{
			return ResultNode;
		}
	}

	return nullptr;
}

UK2Node_FunctionResult* FN2CMcpFunctionPinUtils::EnsureFunctionResultNode(UEdGraph* FunctionGraph)
{
	if (!FunctionGraph)
	{
		return nullptr;
	}

	// Check if we already have a result node
	UK2Node_FunctionResult* ResultNode = FindFunctionResultNode(FunctionGraph);
	if (ResultNode)
	{
		return ResultNode;
	}

	// Create a new function result node
	ResultNode = NewObject<UK2Node_FunctionResult>(FunctionGraph);
	if (!ResultNode)
	{
		UE_LOG(LogNodeToCode, Error, TEXT("Failed to create UK2Node_FunctionResult"));
		return nullptr;
	}

	// Add the node to the graph BEFORE calling any initialization methods
	FunctionGraph->AddNode(ResultNode, true);

	// Position it appropriately (to the right of function entry if it exists)
	UK2Node_FunctionEntry* EntryNode = FindFunctionEntryNode(FunctionGraph);
	if (EntryNode)
	{
		ResultNode->NodePosX = EntryNode->NodePosX + 400;
		ResultNode->NodePosY = EntryNode->NodePosY;
	}
	else
	{
		// Default position if no entry node
		ResultNode->NodePosX = 400;
		ResultNode->NodePosY = 0;
	}

	// Call PostPlacedNewNode to properly initialize the node
	// This is critical for K2Node_FunctionResult to set up its internal state
	ResultNode->PostPlacedNewNode();
	
	// Allocate default pins after the node is properly initialized
	ResultNode->AllocateDefaultPins();
	
	// Reconstruct to ensure everything is properly set up
	ResultNode->ReconstructNode();
	
	// Snap to grid for visual alignment
	ResultNode->SnapToGrid(16);

	UE_LOG(LogNodeToCode, Verbose, TEXT("Created new UK2Node_FunctionResult for function graph: %s"), 
		*FunctionGraph->GetName());

	return ResultNode;
}

void FN2CMcpFunctionPinUtils::UpdateFunctionCallSites(UEdGraph* FunctionGraph, UBlueprint* Blueprint)
{
	if (!FunctionGraph || !Blueprint)
	{
		return;
	}

	// Find all references to this function
	TArray<UK2Node_CallFunction*> CallSites;
	FBlueprintEditorUtils::GetAllNodesOfClass<UK2Node_CallFunction>(Blueprint, CallSites);

	int32 UpdatedCount = 0;
	for (UK2Node_CallFunction* CallSite : CallSites)
	{
		// Check if this call site references our function
		if (CallSite->FunctionReference.GetMemberName() == FunctionGraph->GetFName())
		{
			// Reconstruct the node to update its pins
			CallSite->ReconstructNode();
			UpdatedCount++;
		}
	}

	UE_LOG(LogNodeToCode, Verbose, TEXT("Updated %d call sites for function: %s"), 
		UpdatedCount, *FunctionGraph->GetName());
}

void FN2CMcpFunctionPinUtils::SetPinTooltip(UK2Node_EditablePinBase* Node, UEdGraphPin* Pin, const FString& Tooltip)
{
	if (!Node || !Pin || Tooltip.IsEmpty())
	{
		return;
	}

	// Pin tooltips are stored on the pin itself
	Pin->PinToolTip = Tooltip;
}

TSharedPtr<FJsonObject> FN2CMcpFunctionPinUtils::BuildPinCreationSuccessResult(
	UEdGraph* FunctionGraph,
	const FString& RequestedName,
	UEdGraphPin* CreatedPin,
	const FEdGraphPinType& PinType,
	bool bIsReturnPin)
{
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("pinName"), RequestedName);
	
	// The actual name might be different if it was made unique
	if (CreatedPin)
	{
		Result->SetStringField(TEXT("actualName"), CreatedPin->PinName.ToString());
		Result->SetStringField(TEXT("displayName"), CreatedPin->GetDisplayName().ToString());
		Result->SetStringField(TEXT("pinId"), CreatedPin->PinId.ToString());
	}
	
	// Add type info
	TSharedPtr<FJsonObject> TypeInfo = MakeShareable(new FJsonObject);
	TypeInfo->SetStringField(TEXT("category"), PinType.PinCategory.ToString());
	
	if (PinType.PinSubCategoryObject.IsValid())
	{
		TypeInfo->SetStringField(TEXT("className"), PinType.PinSubCategoryObject->GetName());
		TypeInfo->SetStringField(TEXT("path"), PinType.PinSubCategoryObject->GetPathName());
	}
	
	// Add a type name for clarity
	FString TypeName = PinType.PinCategory.ToString();
	if (PinType.PinSubCategoryObject.IsValid())
	{
		TypeName = PinType.PinSubCategoryObject->GetName();
	}
	TypeInfo->SetStringField(TEXT("typeName"), TypeName);
	
	Result->SetObjectField(TEXT("typeInfo"), TypeInfo);
	
	// Add function and blueprint info
	if (FunctionGraph)
	{
		Result->SetStringField(TEXT("functionName"), FunctionGraph->GetName());
		
		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(FunctionGraph);
		if (Blueprint)
		{
			Result->SetStringField(TEXT("blueprintName"), Blueprint->GetName());
		}
	}
	
	// Build appropriate message
	FString PinTypeDescription = bIsReturnPin ? TEXT("Return pin") : TEXT("Input pin");
	Result->SetStringField(TEXT("message"), FString::Printf(
		TEXT("%s '%s' added successfully to function '%s'"), 
		*PinTypeDescription,
		*RequestedName, 
		FunctionGraph ? *FunctionGraph->GetName() : TEXT("Unknown")));
	
	return Result;
}

bool FN2CMcpFunctionPinUtils::ValidatePinForRemoval(UK2Node_EditablePinBase* Node, UEdGraphPin* Pin, FString& OutError)
{
	if (!Node || !Pin)
	{
		OutError = TEXT("Invalid node or pin");
		return false;
	}

	UE_LOG(LogNodeToCode, Verbose, TEXT("ValidatePinForRemoval: Validating pin '%s' for removal"), 
		*Pin->PinName.ToString());

	// Don't allow removal of execution pins
	if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
	{
		OutError = TEXT("Cannot remove execution pin");
		UE_LOG(LogNodeToCode, Verbose, TEXT("ValidatePinForRemoval: Pin is execution pin, cannot remove"));
		return false;
	}

	// Log user-defined pins for debugging
	UE_LOG(LogNodeToCode, Verbose, TEXT("ValidatePinForRemoval: Node has %d user-defined pins"), 
		Node->UserDefinedPins.Num());
	for (const TSharedPtr<FUserPinInfo>& UserPin : Node->UserDefinedPins)
	{
		if (UserPin.IsValid())
		{
			UE_LOG(LogNodeToCode, Verbose, TEXT("  UserDefinedPin: Name='%s', FName='%s'"), 
				*UserPin->PinName.ToString(), 
				*UserPin->PinName.ToString());
		}
	}

	// Check if it's a user-defined pin by searching the UserDefinedPins array
	bool bIsUserDefined = false;
	for (const TSharedPtr<FUserPinInfo>& UserPin : Node->UserDefinedPins)
	{
		if (UserPin.IsValid() && UserPin->PinName == Pin->GetFName())
		{
			bIsUserDefined = true;
			UE_LOG(LogNodeToCode, Verbose, TEXT("ValidatePinForRemoval: Found matching user-defined pin"));
			break;
		}
	}
	
	if (!bIsUserDefined)
	{
		OutError = TEXT("Cannot remove non-user-defined pin. Only pins added by users can be removed.");
		UE_LOG(LogNodeToCode, Verbose, TEXT("ValidatePinForRemoval: Pin '%s' is not user-defined, cannot remove"), 
			*Pin->PinName.ToString());
		return false;
	}

	UE_LOG(LogNodeToCode, Verbose, TEXT("ValidatePinForRemoval: Pin '%s' validated successfully"), 
		*Pin->PinName.ToString());
	return true;
}

bool FN2CMcpFunctionPinUtils::RemoveFunctionPin(UK2Node_EditablePinBase* Node, const FString& PinName, FString& OutError)
{
	if (!Node)
	{
		OutError = TEXT("Invalid node");
		return false;
	}

	UE_LOG(LogNodeToCode, Verbose, TEXT("RemoveFunctionPin: Attempting to remove pin '%s' from node '%s'"), 
		*PinName, *Node->GetName());

	// Log all current pins for debugging
	UE_LOG(LogNodeToCode, Verbose, TEXT("RemoveFunctionPin: Node has %d pins total"), Node->Pins.Num());
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin)
		{
			UE_LOG(LogNodeToCode, Verbose, TEXT("  Available pin: Name='%s', DisplayName='%s', FName='%s'"), 
				*Pin->PinName.ToString(), 
				*Pin->GetDisplayName().ToString(),
				*Pin->GetFName().ToString());
		}
	}

	// Find the pin
	UEdGraphPin* PinToRemove = Node->FindPin(FName(*PinName));
	if (!PinToRemove)
	{
		// Try case-insensitive search as fallback, checking both PinName and DisplayName
		UE_LOG(LogNodeToCode, Verbose, TEXT("RemoveFunctionPin: Exact match failed, trying case-insensitive and display name search"));
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin)
			{
				// Check PinName (case-insensitive)
				if (Pin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase))
				{
					PinToRemove = Pin;
					UE_LOG(LogNodeToCode, Verbose, TEXT("RemoveFunctionPin: Found pin with case-insensitive match on PinName: %s"), 
						*Pin->PinName.ToString());
					break;
				}
				
				// Check DisplayName (case-insensitive)
				FString DisplayName = Pin->GetDisplayName().ToString();
				if (DisplayName.Equals(PinName, ESearchCase::IgnoreCase))
				{
					PinToRemove = Pin;
					UE_LOG(LogNodeToCode, Verbose, TEXT("RemoveFunctionPin: Found pin with display name match: PinName=%s, DisplayName=%s"), 
						*Pin->PinName.ToString(), *DisplayName);
					break;
				}
			}
		}
		
		if (!PinToRemove)
		{
			// Build list of available pins for error message
			TArray<FString> AvailablePins;
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin)
				{
					FString DisplayName = Pin->GetDisplayName().ToString();
					if (DisplayName != Pin->PinName.ToString())
					{
						AvailablePins.Add(FString::Printf(TEXT("%s (display: %s)"), 
							*Pin->PinName.ToString(), *DisplayName));
					}
					else
					{
						AvailablePins.Add(Pin->PinName.ToString());
					}
				}
			}
			
			OutError = FString::Printf(TEXT("Pin '%s' not found. Available pins: %s"), 
				*PinName, *FString::Join(AvailablePins, TEXT(", ")));
			return false;
		}
	}

	UE_LOG(LogNodeToCode, Verbose, TEXT("RemoveFunctionPin: Found pin to remove: '%s'"), 
		*PinToRemove->PinName.ToString());

	// Validate the pin can be removed
	if (!ValidatePinForRemoval(Node, PinToRemove, OutError))
	{
		UE_LOG(LogNodeToCode, Warning, TEXT("RemoveFunctionPin: Pin validation failed: %s"), *OutError);
		return false;
	}

	// Break all connections before removal
	int32 ConnectionCount = PinToRemove->LinkedTo.Num();
	PinToRemove->BreakAllPinLinks();
	UE_LOG(LogNodeToCode, Verbose, TEXT("RemoveFunctionPin: Broke %d connections"), ConnectionCount);

	// Remove the pin
	Node->RemoveUserDefinedPinByName(PinToRemove->GetFName());

	UE_LOG(LogNodeToCode, Verbose, TEXT("RemoveFunctionPin: Successfully removed pin '%s' from node '%s'"), 
		*PinName, *Node->GetName());

	return true;
}

TSharedPtr<FJsonObject> FN2CMcpFunctionPinUtils::BuildPinRemovalSuccessResult(
	UEdGraph* FunctionGraph,
	const FString& RemovedPinName,
	bool bIsReturnPin)
{
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("removedPinName"), RemovedPinName);
	
	// Add function and blueprint info
	if (FunctionGraph)
	{
		Result->SetStringField(TEXT("functionName"), FunctionGraph->GetName());
		
		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(FunctionGraph);
		if (Blueprint)
		{
			Result->SetStringField(TEXT("blueprintName"), Blueprint->GetName());
		}
	}
	
	// Build appropriate message
	FString PinTypeDescription = bIsReturnPin ? TEXT("Return pin") : TEXT("Input pin");
	Result->SetStringField(TEXT("message"), FString::Printf(
		TEXT("%s '%s' removed successfully from function '%s'"), 
		*PinTypeDescription,
		*RemovedPinName, 
		FunctionGraph ? *FunctionGraph->GetName() : TEXT("Unknown")));
	
	return Result;
}