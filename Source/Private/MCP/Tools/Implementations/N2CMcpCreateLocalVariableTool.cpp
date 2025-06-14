// Copyright Protospatial 2025. All Rights Reserved.

#include "N2CMcpCreateLocalVariableTool.h"
#include "MCP/Tools/N2CMcpToolRegistry.h"
#include "Core/N2CEditorIntegration.h"
#include "Utils/N2CLogger.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_Event.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "EdGraphSchema_K2.h"
#include "BlueprintEditorModule.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// Register this tool with the MCP tool manager
REGISTER_MCP_TOOL(FN2CMcpCreateLocalVariableTool)

#define LOCTEXT_NAMESPACE "NodeToCode"

FMcpToolDefinition FN2CMcpCreateLocalVariableTool::GetDefinition() const
{
	FMcpToolDefinition Definition(
		TEXT("create-local-variable"),
		TEXT("Creates a new local variable in the currently focused Blueprint function")
	);

	// Build input schema
	TSharedPtr<FJsonObject> Schema = MakeShareable(new FJsonObject);
	Schema->SetStringField(TEXT("type"), TEXT("object"));
	
	TSharedPtr<FJsonObject> Properties = MakeShareable(new FJsonObject);
	
	// variableName property (required)
	TSharedPtr<FJsonObject> VariableNameProp = MakeShareable(new FJsonObject);
	VariableNameProp->SetStringField(TEXT("type"), TEXT("string"));
	VariableNameProp->SetStringField(TEXT("description"), TEXT("Name for the new local variable"));
	Properties->SetObjectField(TEXT("variableName"), VariableNameProp);

	// typeIdentifier property (required)
	TSharedPtr<FJsonObject> TypeIdentifierProp = MakeShareable(new FJsonObject);
	TypeIdentifierProp->SetStringField(TEXT("type"), TEXT("string"));
	TypeIdentifierProp->SetStringField(TEXT("description"), TEXT("Type identifier from search-variable-types (e.g., 'bool', '/Script/Engine.Actor')"));
	Properties->SetObjectField(TEXT("typeIdentifier"), TypeIdentifierProp);

	// defaultValue property (optional)
	TSharedPtr<FJsonObject> DefaultValueProp = MakeShareable(new FJsonObject);
	DefaultValueProp->SetStringField(TEXT("type"), TEXT("string"));
	DefaultValueProp->SetStringField(TEXT("description"), TEXT("Optional default value for the variable"));
	DefaultValueProp->SetStringField(TEXT("default"), TEXT(""));
	Properties->SetObjectField(TEXT("defaultValue"), DefaultValueProp);

	// tooltip property (optional)
	TSharedPtr<FJsonObject> TooltipProp = MakeShareable(new FJsonObject);
	TooltipProp->SetStringField(TEXT("type"), TEXT("string"));
	TooltipProp->SetStringField(TEXT("description"), TEXT("Tooltip description for the variable"));
	TooltipProp->SetStringField(TEXT("default"), TEXT(""));
	Properties->SetObjectField(TEXT("tooltip"), TooltipProp);

	Schema->SetObjectField(TEXT("properties"), Properties);
	
	// Required parameters
	TArray<TSharedPtr<FJsonValue>> Required;
	Required.Add(MakeShareable(new FJsonValueString(TEXT("variableName"))));
	Required.Add(MakeShareable(new FJsonValueString(TEXT("typeIdentifier"))));
	Schema->SetArrayField(TEXT("required"), Required);
	
	Definition.InputSchema = Schema;
	
	return Definition;
}

FMcpToolCallResult FN2CMcpCreateLocalVariableTool::Execute(const TSharedPtr<FJsonObject>& Arguments)
{
	return ExecuteOnGameThread([this, Arguments]() -> FMcpToolCallResult
	{
		// Extract parameters
		FString VariableName;
		if (!Arguments->TryGetStringField(TEXT("variableName"), VariableName))
		{
			return FMcpToolCallResult::CreateErrorResult(TEXT("Missing required parameter: variableName"));
		}
		
		FString TypeIdentifier;
		if (!Arguments->TryGetStringField(TEXT("typeIdentifier"), TypeIdentifier))
		{
			return FMcpToolCallResult::CreateErrorResult(TEXT("Missing required parameter: typeIdentifier"));
		}
		
		// Optional parameters
		FString DefaultValue;
		Arguments->TryGetStringField(TEXT("defaultValue"), DefaultValue);
		
		FString Tooltip;
		Arguments->TryGetStringField(TEXT("tooltip"), Tooltip);
		
		// Get focused function graph
		UEdGraph* FocusedGraph = FN2CEditorIntegration::Get().GetFocusedGraphFromActiveEditor();
		if (!FocusedGraph)
		{
			return FMcpToolCallResult::CreateErrorResult(TEXT("No focused graph found in the editor"));
		}
		
		// Ensure we're in a K2 graph
		if (!FocusedGraph->GetSchema()->IsA<UEdGraphSchema_K2>())
		{
			return FMcpToolCallResult::CreateErrorResult(TEXT("Focused graph is not a Blueprint graph"));
		}
		
		// Find the function entry node
		UK2Node_FunctionEntry* FunctionEntry = FindFunctionEntryNode(FocusedGraph);
		if (!FunctionEntry)
		{
			return FMcpToolCallResult::CreateErrorResult(TEXT("Not in a function graph. Local variables can only be created in functions, not event graphs."));
		}
		
		// Resolve type identifier to FEdGraphPinType
		FEdGraphPinType PinType;
		FString ResolveError;
		if (!ResolveTypeIdentifier(TypeIdentifier, PinType, ResolveError))
		{
			return FMcpToolCallResult::CreateErrorResult(ResolveError);
		}
		
		// Create the local variable
		FName ActualVariableName = CreateLocalVariable(FunctionEntry, VariableName, PinType, DefaultValue, Tooltip);
		
		// Build and return success result
		TSharedPtr<FJsonObject> Result = BuildSuccessResult(FunctionEntry, FocusedGraph, VariableName, ActualVariableName, PinType);
		
		FString JsonString;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
		FJsonSerializer::Serialize(Result.ToSharedRef(), Writer);
		
		return FMcpToolCallResult::CreateTextResult(JsonString);
	});
}

UK2Node_FunctionEntry* FN2CMcpCreateLocalVariableTool::FindFunctionEntryNode(UEdGraph* Graph) const
{
	if (!Graph)
	{
		return nullptr;
	}
	
	// Iterate through all nodes in the graph
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node))
		{
			return Entry;
		}
	}
	
	// For event graphs, check for event nodes (which don't support local variables)
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (Cast<UK2Node_Event>(Node) || Cast<UK2Node_CustomEvent>(Node))
		{
			// Events don't support local variables directly
			return nullptr;
		}
	}
	
	return nullptr;
}

bool FN2CMcpCreateLocalVariableTool::ResolveTypeIdentifier(const FString& TypeIdentifier, FEdGraphPinType& OutPinType, FString& OutError) const
{
	// Check if it's a primitive type first
	if (IsPrimitiveType(TypeIdentifier))
	{
		return ResolvePrimitiveType(TypeIdentifier, OutPinType);
	}
	
	// Otherwise, try to resolve as an object type
	return ResolveObjectType(TypeIdentifier, OutPinType, OutError);
}

bool FN2CMcpCreateLocalVariableTool::IsPrimitiveType(const FString& TypeIdentifier) const
{
	static const TSet<FString> PrimitiveTypes = {
		TEXT("bool"),
		TEXT("byte"),
		TEXT("int"),
		TEXT("int64"),
		TEXT("float"),
		TEXT("double"),
		TEXT("name"),
		TEXT("string"),
		TEXT("text"),
		TEXT("vector"),
		TEXT("rotator"),
		TEXT("transform")
	};
	
	return PrimitiveTypes.Contains(TypeIdentifier.ToLower());
}

bool FN2CMcpCreateLocalVariableTool::ResolvePrimitiveType(const FString& TypeIdentifier, FEdGraphPinType& OutPinType) const
{
	const FString LowerType = TypeIdentifier.ToLower();
	
	OutPinType.ResetToDefaults();
	OutPinType.ContainerType = EPinContainerType::None;
	
	if (LowerType == TEXT("bool"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
	}
	else if (LowerType == TEXT("byte"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
	}
	else if (LowerType == TEXT("int"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Int;
	}
	else if (LowerType == TEXT("int64"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Int64;
	}
	else if (LowerType == TEXT("float"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		OutPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
	}
	else if (LowerType == TEXT("double"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		OutPinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
	}
	else if (LowerType == TEXT("name"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Name;
	}
	else if (LowerType == TEXT("string"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_String;
	}
	else if (LowerType == TEXT("text"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Text;
	}
	else if (LowerType == TEXT("vector"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		OutPinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
	}
	else if (LowerType == TEXT("rotator"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		OutPinType.PinSubCategoryObject = TBaseStructure<FRotator>::Get();
	}
	else if (LowerType == TEXT("transform"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		OutPinType.PinSubCategoryObject = TBaseStructure<FTransform>::Get();
	}
	else
	{
		return false;
	}
	
	return true;
}

bool FN2CMcpCreateLocalVariableTool::ResolveObjectType(const FString& TypeIdentifier, FEdGraphPinType& OutPinType, FString& OutError) const
{
	// Handle potential _C suffix for Blueprint classes
	FString CleanPath = TypeIdentifier;
	bool bIsBlueprintClass = false;
	
	if (TypeIdentifier.EndsWith(TEXT("_C")))
	{
		CleanPath = TypeIdentifier.LeftChop(2); // Remove _C suffix
		bIsBlueprintClass = true;
	}
	
	// Try to find the object
	UObject* FoundObject = FindObject<UObject>(nullptr, *TypeIdentifier);
	
	// If not found and has _C suffix, try without it
	if (!FoundObject && bIsBlueprintClass)
	{
		FoundObject = FindObject<UObject>(nullptr, *CleanPath);
	}
	
	if (!FoundObject)
	{
		OutError = FString::Printf(TEXT("Failed to resolve type: '%s'"), *TypeIdentifier);
		return false;
	}
	
	// Determine the appropriate pin type based on the object
	OutPinType.ResetToDefaults();
	OutPinType.ContainerType = EPinContainerType::None;
	
	if (UClass* AsClass = Cast<UClass>(FoundObject))
	{
		// Check if it's a valid Blueprint variable type
		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
		if (!K2Schema->IsAllowableBlueprintVariableType(AsClass))
		{
			OutError = FString::Printf(TEXT("Type '%s' is not allowed as a Blueprint variable"), *AsClass->GetName());
			return false;
		}
		
		// Determine if it's an object reference or class reference
		if (AsClass->IsChildOf(UObject::StaticClass()) && !AsClass->IsChildOf(UClass::StaticClass()))
		{
			OutPinType.PinCategory = UEdGraphSchema_K2::PC_Object;
			OutPinType.PinSubCategoryObject = AsClass;
		}
		else
		{
			OutPinType.PinCategory = UEdGraphSchema_K2::PC_Class;
			OutPinType.PinSubCategoryObject = AsClass;
		}
	}
	else if (UScriptStruct* AsStruct = Cast<UScriptStruct>(FoundObject))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		OutPinType.PinSubCategoryObject = AsStruct;
	}
	else if (UEnum* AsEnum = Cast<UEnum>(FoundObject))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
		OutPinType.PinSubCategoryObject = AsEnum;
	}
	else
	{
		OutError = FString::Printf(TEXT("Object '%s' is not a valid type for Blueprint variables"), *TypeIdentifier);
		return false;
	}
	
	return true;
}

FName FN2CMcpCreateLocalVariableTool::MakeUniqueLocalVariableName(UK2Node_FunctionEntry* FunctionEntry, const FString& BaseName) const
{
	if (!FunctionEntry)
	{
		return FName(*BaseName);
	}
	
	FName TestName(*BaseName);
	int32 Counter = 0;
	
	// Check against existing local variables
	while (true)
	{
		bool bNameExists = false;
		
		for (const FBPVariableDescription& LocalVar : FunctionEntry->LocalVariables)
		{
			if (LocalVar.VarName == TestName)
			{
				bNameExists = true;
				break;
			}
		}
		
		if (!bNameExists)
		{
			return TestName;
		}
		
		// Try with counter
		Counter++;
		TestName = FName(*FString::Printf(TEXT("%s_%d"), *BaseName, Counter));
	}
}

FName FN2CMcpCreateLocalVariableTool::CreateLocalVariable(UK2Node_FunctionEntry* FunctionEntry, const FString& DesiredName,
	const FEdGraphPinType& PinType, const FString& DefaultValue, const FString& Tooltip)
{
	if (!FunctionEntry)
	{
		return NAME_None;
	}
	
	// Create FBPVariableDescription
	FBPVariableDescription NewVar;
	NewVar.VarName = MakeUniqueLocalVariableName(FunctionEntry, DesiredName);
	NewVar.VarGuid = FGuid::NewGuid();
	NewVar.VarType = PinType;
	NewVar.FriendlyName = DesiredName;
	NewVar.DefaultValue = DefaultValue;
	NewVar.Category = FText::FromString(TEXT("Local"));
	
	// Set metadata
	if (!Tooltip.IsEmpty())
	{
		NewVar.SetMetaData(TEXT("ToolTip"), Tooltip);
	}
	
	// Add to function entry's local variables
	FunctionEntry->LocalVariables.Add(NewVar);
	
	// Reconstruct the node to show the new local variable
	FunctionEntry->ReconstructNode();
	
	// Mark Blueprint as modified
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(FunctionEntry);
	if (Blueprint)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}
	
	// Show notification
	FNotificationInfo Info(FText::Format(
		LOCTEXT("LocalVariableCreated", "Local variable '{0}' created successfully"),
		FText::FromName(NewVar.VarName)
	));
	Info.ExpireDuration = 3.0f;
	Info.bFireAndForget = true;
	Info.Image = FCoreStyle::Get().GetBrush(TEXT("Icons.SuccessWithCircle"));
	FSlateNotificationManager::Get().AddNotification(Info);
	
	return NewVar.VarName;
}

TSharedPtr<FJsonObject> FN2CMcpCreateLocalVariableTool::BuildSuccessResult(UK2Node_FunctionEntry* FunctionEntry, UEdGraph* FunctionGraph,
	const FString& RequestedName, FName ActualName, const FEdGraphPinType& PinType) const
{
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("variableName"), RequestedName);
	Result->SetStringField(TEXT("actualName"), ActualName.ToString());
	
	// Add type info
	TSharedPtr<FJsonObject> TypeInfo = MakeShareable(new FJsonObject);
	
	// Determine category
	FString Category;
	if (PinType.PinCategory == UEdGraphSchema_K2::PC_Boolean)
		Category = TEXT("boolean");
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Byte)
		Category = TEXT("byte");
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Int)
		Category = TEXT("integer");
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Int64)
		Category = TEXT("integer64");
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Real)
		Category = TEXT("float");
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Name)
		Category = TEXT("name");
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_String)
		Category = TEXT("string");
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Text)
		Category = TEXT("text");
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Object)
		Category = TEXT("object");
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Class)
		Category = TEXT("class");
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Struct)
		Category = TEXT("struct");
	else
		Category = PinType.PinCategory.ToString();
	
	TypeInfo->SetStringField(TEXT("category"), Category);
	
	// Add class/struct name if applicable
	if (PinType.PinSubCategoryObject.IsValid())
	{
		TypeInfo->SetStringField(TEXT("className"), PinType.PinSubCategoryObject->GetName());
		TypeInfo->SetStringField(TEXT("path"), PinType.PinSubCategoryObject->GetPathName());
	}
	
	Result->SetObjectField(TEXT("typeInfo"), TypeInfo);
	
	// Add function and Blueprint info
	if (FunctionGraph)
	{
		Result->SetStringField(TEXT("functionName"), FunctionGraph->GetName());
		
		if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(FunctionGraph))
		{
			Result->SetStringField(TEXT("blueprintName"), Blueprint->GetName());
		}
	}
	
	Result->SetStringField(TEXT("message"), FString::Printf(
		TEXT("Local variable '%s' created successfully in function '%s'"),
		*ActualName.ToString(),
		FunctionGraph ? *FunctionGraph->GetName() : TEXT("Unknown")
	));
	
	return Result;
}

#undef LOCTEXT_NAMESPACE