// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#include "N2CMcpAddFunctionInputPinTool.h"
#include "MCP/Utils/N2CMcpArgumentParser.h"
#include "MCP/Utils/N2CMcpTypeResolver.h"
#include "MCP/Utils/N2CMcpBlueprintUtils.h"
#include "MCP/Tools/N2CMcpToolRegistry.h"
#include "MCP/Tools/N2CMcpToolTypes.h"
#include "MCP/Tools/N2CMcpFunctionGuidUtils.h"
#include "Core/N2CEditorIntegration.h"
#include "Utils/N2CLogger.h"
#include "Engine/Blueprint.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_CallFunction.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

REGISTER_MCP_TOOL(FN2CMcpAddFunctionInputPinTool)

FMcpToolDefinition FN2CMcpAddFunctionInputPinTool::GetDefinition() const
{
	FMcpToolDefinition Definition(
		TEXT("add-function-input-pin"),
		TEXT("Adds a new input parameter to the currently focused Blueprint function")
	);

	// Build input schema
	TSharedPtr<FJsonObject> Schema = MakeShareable(new FJsonObject);
	Schema->SetStringField(TEXT("type"), TEXT("object"));
	
	TSharedPtr<FJsonObject> Properties = MakeShareable(new FJsonObject);
	
	// pinName property
	TSharedPtr<FJsonObject> PinNameProp = MakeShareable(new FJsonObject);
	PinNameProp->SetStringField(TEXT("type"), TEXT("string"));
	PinNameProp->SetStringField(TEXT("description"), TEXT("Name for the new input parameter"));
	Properties->SetObjectField(TEXT("pinName"), PinNameProp);

	// typeIdentifier property
	TSharedPtr<FJsonObject> TypeIdentifierProp = MakeShareable(new FJsonObject);
	TypeIdentifierProp->SetStringField(TEXT("type"), TEXT("string"));
	TypeIdentifierProp->SetStringField(TEXT("description"), TEXT("Type identifier from search-variable-types (e.g., 'bool', '/Script/Engine.Actor')"));
	Properties->SetObjectField(TEXT("typeIdentifier"), TypeIdentifierProp);

	// defaultValue property (optional)
	TSharedPtr<FJsonObject> DefaultValueProp = MakeShareable(new FJsonObject);
	DefaultValueProp->SetStringField(TEXT("type"), TEXT("string"));
	DefaultValueProp->SetStringField(TEXT("description"), TEXT("Optional default value for the parameter"));
	DefaultValueProp->SetStringField(TEXT("default"), TEXT(""));
	Properties->SetObjectField(TEXT("defaultValue"), DefaultValueProp);

	// isPassByReference property (optional)
	TSharedPtr<FJsonObject> IsPassByRefProp = MakeShareable(new FJsonObject);
	IsPassByRefProp->SetStringField(TEXT("type"), TEXT("boolean"));
	IsPassByRefProp->SetStringField(TEXT("description"), TEXT("Whether the parameter is passed by reference"));
	IsPassByRefProp->SetBoolField(TEXT("default"), false);
	Properties->SetObjectField(TEXT("isPassByReference"), IsPassByRefProp);

	// tooltip property (optional)
	TSharedPtr<FJsonObject> TooltipProp = MakeShareable(new FJsonObject);
	TooltipProp->SetStringField(TEXT("type"), TEXT("string"));
	TooltipProp->SetStringField(TEXT("description"), TEXT("Tooltip description for the parameter"));
	TooltipProp->SetStringField(TEXT("default"), TEXT(""));
	Properties->SetObjectField(TEXT("tooltip"), TooltipProp);
	
	Schema->SetObjectField(TEXT("properties"), Properties);
	
	// Required fields
	TArray<TSharedPtr<FJsonValue>> RequiredArray;
	RequiredArray.Add(MakeShareable(new FJsonValueString(TEXT("pinName"))));
	RequiredArray.Add(MakeShareable(new FJsonValueString(TEXT("typeIdentifier"))));
	Schema->SetArrayField(TEXT("required"), RequiredArray);
	
	Definition.InputSchema = Schema;
	
	return Definition;
}

FMcpToolCallResult FN2CMcpAddFunctionInputPinTool::Execute(const TSharedPtr<FJsonObject>& Arguments)
{
	return ExecuteOnGameThread([this, Arguments]() -> FMcpToolCallResult
	{
		FN2CMcpArgumentParser ArgParser(Arguments);
		FString Error;

		// Parse arguments
		FString PinName;
		if (!ArgParser.TryGetRequiredString(TEXT("pinName"), PinName, Error))
		{
			return FMcpToolCallResult::CreateErrorResult(Error);
		}

		FString TypeIdentifier;
		if (!ArgParser.TryGetRequiredString(TEXT("typeIdentifier"), TypeIdentifier, Error))
		{
			return FMcpToolCallResult::CreateErrorResult(Error);
		}

		FString DefaultValue = ArgParser.GetOptionalString(TEXT("defaultValue"), TEXT(""));
		bool bIsPassByReference = ArgParser.GetOptionalBool(TEXT("isPassByReference"), false);
		FString Tooltip = ArgParser.GetOptionalString(TEXT("tooltip"), TEXT(""));

		// Get focused function graph
		UEdGraph* FocusedGraph = FN2CEditorIntegration::Get().GetFocusedGraphFromActiveEditor();
		if (!FocusedGraph)
		{
			return FMcpToolCallResult::CreateErrorResult(TEXT("No focused graph found. Please open a Blueprint function in the editor."));
		}

		// Check if this is a K2 graph
		if (!FocusedGraph->GetSchema()->IsA<UEdGraphSchema_K2>())
		{
			return FMcpToolCallResult::CreateErrorResult(TEXT("The focused graph is not a Blueprint graph"));
		}

		// Find the function entry node
		UK2Node_FunctionEntry* FunctionEntry = FindFunctionEntryNode(FocusedGraph);
		if (!FunctionEntry)
		{
			return FMcpToolCallResult::CreateErrorResult(TEXT("Not in a function graph. Please focus on a Blueprint function."));
		}

		// Resolve type identifier to FEdGraphPinType
		FEdGraphPinType PinType;
		FString ResolveError;
		
		// For simple types, we can resolve directly
		if (!FN2CMcpTypeResolver::ResolvePinType(
			TypeIdentifier,     // Primary type
			TEXT(""),          // SubType (empty for primitives)
			TEXT("none"),      // Container type
			TEXT(""),          // Key type (for maps)
			bIsPassByReference, // Is reference
			false,             // Is const
			PinType,
			ResolveError))
		{
			// If direct resolution fails, try with the type identifier as a subtype for object types
			if (!FN2CMcpTypeResolver::ResolvePinType(
				TEXT("object"),    // Primary type
				TypeIdentifier,    // SubType
				TEXT("none"),      // Container type
				TEXT(""),          // Key type
				bIsPassByReference,
				false,
				PinType,
				ResolveError))
			{
				return FMcpToolCallResult::CreateErrorResult(
					FString::Printf(TEXT("Failed to resolve type '%s': %s"), *TypeIdentifier, *ResolveError));
			}
		}

		// Start a transaction for undo/redo
		const FScopedTransaction Transaction(
			FText::Format(NSLOCTEXT("NodeToCode", "AddFunctionInputPin", "Add Input Pin '{0}'"), 
			FText::FromString(PinName))
		);

		// Create the input pin
		UEdGraphPin* NewPin = CreateInputPin(FunctionEntry, PinName, PinType, DefaultValue, Tooltip);
		if (!NewPin)
		{
			return FMcpToolCallResult::CreateErrorResult(TEXT("Failed to create input pin"));
		}

		// Update all function call sites
		UpdateFunctionCallSites(FunctionEntry);

		// Mark Blueprint as modified
		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(FunctionEntry);
		if (Blueprint)
		{
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		}

		// Show notification
		FNotificationInfo Info(FText::Format(
			NSLOCTEXT("NodeToCode", "InputPinAdded", "Input pin '{0}' added to function '{1}'"),
			FText::FromString(PinName),
			FText::FromString(FocusedGraph->GetName())
		));
		Info.ExpireDuration = 3.0f;
		FSlateNotificationManager::Get().AddNotification(Info);

		// Build and return success result
		TSharedPtr<FJsonObject> ResultJson = BuildSuccessResult(FunctionEntry, FocusedGraph, PinName, NewPin, PinType);
		
		// Convert JSON object to string
		FString ResultString;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultString);
		FJsonSerializer::Serialize(ResultJson.ToSharedRef(), Writer);
		
		return FMcpToolCallResult::CreateTextResult(ResultString);
	});
}

UK2Node_FunctionEntry* FN2CMcpAddFunctionInputPinTool::FindFunctionEntryNode(UEdGraph* Graph) const
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

UEdGraphPin* FN2CMcpAddFunctionInputPinTool::CreateInputPin(UK2Node_FunctionEntry* FunctionEntry, 
	const FString& DesiredName, const FEdGraphPinType& PinType, 
	const FString& DefaultValue, const FString& Tooltip) const
{
	if (!FunctionEntry)
	{
		return nullptr;
	}

	// Check if we can create output pins (function inputs)
	FText ErrorMessage;
	if (!FunctionEntry->CanCreateUserDefinedPin(PinType, EGPD_Output, ErrorMessage))
	{
		UE_LOG(LogNodeToCode, Error, TEXT("Cannot create pin: %s"), *ErrorMessage.ToString());
		return nullptr;
	}

	// Create the pin (will auto-generate unique name if needed)
	UEdGraphPin* NewPin = FunctionEntry->CreateUserDefinedPin(
		FName(*DesiredName), 
		PinType, 
		EGPD_Output,  // Output from FunctionEntry = Input to function
		true          // Use unique name
	);

	if (NewPin)
	{
		// Set default value if provided
		if (!DefaultValue.IsEmpty())
		{
			// Find the corresponding FUserPinInfo for this pin
			TSharedPtr<FUserPinInfo>* PinInfoPtr = FunctionEntry->UserDefinedPins.FindByPredicate(
				[NewPin](const TSharedPtr<FUserPinInfo>& Info)
				{
					return Info->PinName == NewPin->PinName;
				});
			
			if (PinInfoPtr && PinInfoPtr->IsValid())
			{
				FunctionEntry->ModifyUserDefinedPinDefaultValue(*PinInfoPtr, DefaultValue);
			}
		}

		// Set tooltip metadata
		if (!Tooltip.IsEmpty())
		{
			SetPinTooltip(FunctionEntry, NewPin, Tooltip);
		}
	}

	return NewPin;
}

void FN2CMcpAddFunctionInputPinTool::SetPinTooltip(UK2Node_FunctionEntry* FunctionEntry, 
	UEdGraphPin* Pin, const FString& Tooltip) const
{
	if (!FunctionEntry || !Pin)
	{
		return;
	}

	// Pin tooltips are stored on the pin itself, not in FUserPinInfo
	Pin->PinToolTip = Tooltip;
}

void FN2CMcpAddFunctionInputPinTool::UpdateFunctionCallSites(UK2Node_FunctionEntry* FunctionEntry) const
{
	if (!FunctionEntry)
	{
		return;
	}

	// When we add/remove pins from a function, all call sites need updating
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(FunctionEntry);
	if (!Blueprint)
	{
		return;
	}

	// Get the function's graph
	UEdGraph* FunctionGraph = FunctionEntry->GetGraph();
	if (!FunctionGraph)
	{
		return;
	}

	// Find all references to this function
	TArray<UK2Node_CallFunction*> CallSites;
	FBlueprintEditorUtils::GetAllNodesOfClass<UK2Node_CallFunction>(Blueprint, CallSites);

	for (UK2Node_CallFunction* CallSite : CallSites)
	{
		// Check if this call site references our function
		if (CallSite->FunctionReference.GetMemberName() == FunctionGraph->GetFName())
		{
			// Reconstruct the node to update its pins
			CallSite->ReconstructNode();
		}
	}
}

TSharedPtr<FJsonObject> FN2CMcpAddFunctionInputPinTool::BuildSuccessResult(UK2Node_FunctionEntry* FunctionEntry, 
	UEdGraph* FunctionGraph, const FString& RequestedName, 
	UEdGraphPin* CreatedPin, const FEdGraphPinType& PinType) const
{
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("pinName"), RequestedName);
	
	// The actual name might be different if it was made unique
	if (CreatedPin)
	{
		Result->SetStringField(TEXT("actualName"), CreatedPin->PinName.ToString());
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
	
	Result->SetStringField(TEXT("message"), FString::Printf(
		TEXT("Input pin '%s' added successfully to function '%s'"), 
		*RequestedName, 
		FunctionGraph ? *FunctionGraph->GetName() : TEXT("Unknown")));
	
	return Result;
}