// Copyright Protospatial 2025. All Rights Reserved.

#include "N2CMcpListBlueprintFunctionsTool.h"
#include "MCP/Tools/N2CMcpToolRegistry.h"
#include "MCP/Tools/N2CMcpToolTypes.h"
#include "Utils/N2CLogger.h"
#include "Core/N2CEditorIntegration.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "BlueprintEditorModule.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "EdGraphSchema_K2.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

REGISTER_MCP_TOOL(FN2CMcpListBlueprintFunctionsTool)

FMcpToolDefinition FN2CMcpListBlueprintFunctionsTool::GetDefinition() const
{
	FMcpToolDefinition Definition(
		TEXT("list-blueprint-functions"),
		TEXT("Lists all functions defined in a Blueprint with their parameters and metadata")
	);

	// Build input schema
	TSharedPtr<FJsonObject> Schema = MakeShareable(new FJsonObject);
	Schema->SetStringField(TEXT("type"), TEXT("object"));
	
	TSharedPtr<FJsonObject> Properties = MakeShareable(new FJsonObject);
	
	// blueprintPath property (optional)
	TSharedPtr<FJsonObject> BlueprintPathProp = MakeShareable(new FJsonObject);
	BlueprintPathProp->SetStringField(TEXT("type"), TEXT("string"));
	BlueprintPathProp->SetStringField(TEXT("description"), TEXT("Asset path of the Blueprint (e.g., '/Game/Blueprints/MyActor.MyActor'). If not provided, uses focused Blueprint."));
	Properties->SetObjectField(TEXT("blueprintPath"), BlueprintPathProp);

	// includeInherited property (optional)
	TSharedPtr<FJsonObject> IncludeInheritedProp = MakeShareable(new FJsonObject);
	IncludeInheritedProp->SetStringField(TEXT("type"), TEXT("boolean"));
	IncludeInheritedProp->SetStringField(TEXT("description"), TEXT("Whether to include inherited functions from parent classes"));
	IncludeInheritedProp->SetBoolField(TEXT("default"), false);
	Properties->SetObjectField(TEXT("includeInherited"), IncludeInheritedProp);

	// includeOverridden property (optional)
	TSharedPtr<FJsonObject> IncludeOverriddenProp = MakeShareable(new FJsonObject);
	IncludeOverriddenProp->SetStringField(TEXT("type"), TEXT("boolean"));
	IncludeOverriddenProp->SetStringField(TEXT("description"), TEXT("Whether to include overridden parent functions"));
	IncludeOverriddenProp->SetBoolField(TEXT("default"), false);
	Properties->SetObjectField(TEXT("includeOverridden"), IncludeOverriddenProp);
	
	Schema->SetObjectField(TEXT("properties"), Properties);
	
	// No required fields
	TArray<TSharedPtr<FJsonValue>> RequiredArray;
	Schema->SetArrayField(TEXT("required"), RequiredArray);
	
	Definition.InputSchema = Schema;
	
	// Mark as read-only
	AddReadOnlyAnnotation(Definition);
	
	return Definition;
}

FMcpToolCallResult FN2CMcpListBlueprintFunctionsTool::Execute(const TSharedPtr<FJsonObject>& Arguments)
{
	return ExecuteOnGameThread([this, Arguments]() -> FMcpToolCallResult
	{
		// Parse arguments
		FString BlueprintPath;
		Arguments->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);
		
		bool bIncludeInherited = false;
		Arguments->TryGetBoolField(TEXT("includeInherited"), bIncludeInherited);
		
		bool bIncludeOverridden = false;
		Arguments->TryGetBoolField(TEXT("includeOverridden"), bIncludeOverridden);
		
		// Resolve target Blueprint
		UBlueprint* TargetBlueprint = ResolveTargetBlueprint(BlueprintPath);
		if (!TargetBlueprint)
		{
			if (BlueprintPath.IsEmpty())
			{
				return FMcpToolCallResult::CreateErrorResult(TEXT("NO_ACTIVE_BLUEPRINT: No blueprint path provided and no focused editor"));
			}
			else
			{
				return FMcpToolCallResult::CreateErrorResult(FString::Printf(TEXT("ASSET_NOT_FOUND: Blueprint not found at path: %s"), *BlueprintPath));
			}
		}
		
		// Collect function information
		TSharedPtr<FJsonObject> Result = CollectFunctionInformation(TargetBlueprint);
		
		// Add metadata
		Result->SetStringField(TEXT("blueprintName"), TargetBlueprint->GetName());
		Result->SetStringField(TEXT("blueprintPath"), TargetBlueprint->GetPathName());
		
		// Add parent class info if relevant
		if (TargetBlueprint->ParentClass)
		{
			Result->SetStringField(TEXT("parentClass"), TargetBlueprint->ParentClass->GetName());
		}
		
		// Convert JSON object to string
		FString ResultString;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultString);
		FJsonSerializer::Serialize(Result.ToSharedRef(), Writer);
		
		return FMcpToolCallResult::CreateTextResult(ResultString);
	});
}

UBlueprint* FN2CMcpListBlueprintFunctionsTool::ResolveTargetBlueprint(const FString& BlueprintPath) const
{
	// If blueprint path is provided, load it
	if (!BlueprintPath.IsEmpty())
	{
		// Try to find the asset
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
		
		FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(BlueprintPath));
		if (AssetData.IsValid())
		{
			UObject* LoadedAsset = AssetData.GetAsset();
			return Cast<UBlueprint>(LoadedAsset);
		}
		
		// Try alternate loading method
		UObject* LoadedObject = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
		return Cast<UBlueprint>(LoadedObject);
	}
	
	// Otherwise, try to get the focused Blueprint
	FBlueprintEditorModule& BPEditorModule = FModuleManager::LoadModuleChecked<FBlueprintEditorModule>("Kismet");
	TArray<TSharedRef<IBlueprintEditor>> BlueprintEditors = BPEditorModule.GetBlueprintEditors();
	
	// Find the most recently activated Blueprint editor
	TSharedPtr<IBlueprintEditor> ActiveEditor;
	double LatestActivationTime = 0.0;
	
	for (const TSharedRef<IBlueprintEditor>& Editor : BlueprintEditors)
	{
		double ActivationTime = Editor->GetLastActivationTime();
		if (ActivationTime > LatestActivationTime)
		{
			LatestActivationTime = ActivationTime;
			ActiveEditor = Editor;
		}
	}
	
	if (ActiveEditor.IsValid())
	{
		// Cast IBlueprintEditor to FBlueprintEditor to access GetBlueprintObj
		if (TSharedPtr<FBlueprintEditor> BPEditor = StaticCastSharedPtr<FBlueprintEditor>(ActiveEditor))
		{
			return BPEditor->GetBlueprintObj();
		}
	}
	
	// Alternative: Get from focused graph
	if (UEdGraph* FocusedGraph = FN2CEditorIntegration::Get().GetFocusedGraphFromActiveEditor())
	{
		return FBlueprintEditorUtils::FindBlueprintForGraph(FocusedGraph);
	}
	
	return nullptr;
}

TSharedPtr<FJsonObject> FN2CMcpListBlueprintFunctionsTool::CollectFunctionInformation(const UBlueprint* Blueprint) const
{
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	TArray<TSharedPtr<FJsonValue>> FunctionsArray;
	
	// Collect all function graphs
	for (const UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph)
		{
			TSharedPtr<FJsonObject> FunctionInfo = CollectFunctionDetails(Graph);
			if (FunctionInfo.IsValid())
			{
				FunctionsArray.Add(MakeShareable(new FJsonValueObject(FunctionInfo)));
			}
		}
	}
	
	// Collect event graphs (which may contain custom events)
	for (const UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (Graph)
		{
			// Look for custom event nodes in the graph
			for (const UEdGraphNode* Node : Graph->Nodes)
			{
				if (const UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(Node))
				{
					// This is a custom event entry point
					TSharedPtr<FJsonObject> EventInfo = MakeShareable(new FJsonObject);
					EventInfo->SetStringField(TEXT("name"), EntryNode->GetNodeTitle(ENodeTitleType::MenuTitle).ToString());
					EventInfo->SetStringField(TEXT("type"), TEXT("CustomEvent"));
					EventInfo->SetStringField(TEXT("guid"), FGuid::NewGuid().ToString()); // Events don't have persistent GUIDs
					EventInfo->SetStringField(TEXT("graphName"), Graph->GetName());
					
					// Get parameters
					TArray<TSharedPtr<FJsonValue>> InputParams;
					TArray<TSharedPtr<FJsonValue>> OutputParams;
					ExtractFunctionParameters(EntryNode, nullptr, InputParams, OutputParams);
					
					EventInfo->SetArrayField(TEXT("inputs"), InputParams);
					EventInfo->SetArrayField(TEXT("outputs"), OutputParams);
					
					// Get flags
					EventInfo->SetObjectField(TEXT("flags"), GetFunctionFlags(EntryNode));
					
					FunctionsArray.Add(MakeShareable(new FJsonValueObject(EventInfo)));
				}
			}
		}
	}
	
	Result->SetArrayField(TEXT("functions"), FunctionsArray);
	Result->SetNumberField(TEXT("functionCount"), FunctionsArray.Num());
	
	return Result;
}

TSharedPtr<FJsonObject> FN2CMcpListBlueprintFunctionsTool::CollectFunctionDetails(const UEdGraph* FunctionGraph) const
{
	if (!FunctionGraph)
	{
		return nullptr;
	}
	
	TSharedPtr<FJsonObject> FunctionInfo = MakeShareable(new FJsonObject);
	
	// Basic info
	FunctionInfo->SetStringField(TEXT("name"), FunctionGraph->GetName());
	FunctionInfo->SetStringField(TEXT("type"), TEXT("Function"));
	
	// Get function GUID
	FGuid FunctionGuid = GetFunctionGuid(FunctionGraph);
	FunctionInfo->SetStringField(TEXT("guid"), FunctionGuid.ToString());
	
	// Find entry and result nodes
	UK2Node_FunctionEntry* EntryNode = nullptr;
	UK2Node_FunctionResult* ResultNode = nullptr;
	
	for (UEdGraphNode* Node : FunctionGraph->Nodes)
	{
		if (UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node))
		{
			EntryNode = Entry;
		}
		else if (UK2Node_FunctionResult* Result = Cast<UK2Node_FunctionResult>(Node))
		{
			ResultNode = Result;
		}
	}
	
	if (!EntryNode)
	{
		// This shouldn't happen for valid function graphs
		FN2CLogger::Get().LogError(FString::Printf(TEXT("Function graph '%s' has no entry node"), *FunctionGraph->GetName()));
		return nullptr;
	}
	
	// Extract parameters
	TArray<TSharedPtr<FJsonValue>> InputParams;
	TArray<TSharedPtr<FJsonValue>> OutputParams;
	ExtractFunctionParameters(EntryNode, ResultNode, InputParams, OutputParams);
	
	FunctionInfo->SetArrayField(TEXT("inputs"), InputParams);
	FunctionInfo->SetArrayField(TEXT("outputs"), OutputParams);
	
	// Get function flags and metadata
	FunctionInfo->SetObjectField(TEXT("flags"), GetFunctionFlags(EntryNode));
	
	// Add graph info
	TSharedPtr<FJsonObject> GraphInfo = MakeShareable(new FJsonObject);
	GraphInfo->SetStringField(TEXT("graphName"), FunctionGraph->GetName());
	GraphInfo->SetNumberField(TEXT("nodeCount"), FunctionGraph->Nodes.Num());
	FunctionInfo->SetObjectField(TEXT("graphInfo"), GraphInfo);
	
	return FunctionInfo;
}

void FN2CMcpListBlueprintFunctionsTool::ExtractFunctionParameters(
	const UK2Node_FunctionEntry* EntryNode,
	const UK2Node_FunctionResult* ResultNode,
	TArray<TSharedPtr<FJsonValue>>& OutInputParams,
	TArray<TSharedPtr<FJsonValue>>& OutOutputParams) const
{
	// Extract input parameters from entry node (they appear as output pins on the entry node)
	if (EntryNode)
	{
		for (const UEdGraphPin* Pin : EntryNode->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Output && !UEdGraphSchema_K2::IsExecPin(*Pin))
			{
				TSharedPtr<FJsonObject> ParamInfo = MakeShareable(new FJsonObject);
				ParamInfo->SetStringField(TEXT("name"), Pin->PinName.ToString());
				ParamInfo->SetObjectField(TEXT("type"), ConvertPinTypeToJson(Pin->PinType));
				
				// Add default value if present
				if (!Pin->DefaultValue.IsEmpty())
				{
					ParamInfo->SetStringField(TEXT("defaultValue"), Pin->DefaultValue);
				}
				
				OutInputParams.Add(MakeShareable(new FJsonValueObject(ParamInfo)));
			}
		}
	}
	
	// Extract output parameters from result node (they appear as input pins on the result node)
	if (ResultNode)
	{
		for (const UEdGraphPin* Pin : ResultNode->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Input && !UEdGraphSchema_K2::IsExecPin(*Pin))
			{
				TSharedPtr<FJsonObject> ParamInfo = MakeShareable(new FJsonObject);
				ParamInfo->SetStringField(TEXT("name"), Pin->PinName.ToString());
				ParamInfo->SetObjectField(TEXT("type"), ConvertPinTypeToJson(Pin->PinType));
				
				OutOutputParams.Add(MakeShareable(new FJsonValueObject(ParamInfo)));
			}
		}
	}
}

TSharedPtr<FJsonObject> FN2CMcpListBlueprintFunctionsTool::ConvertPinTypeToJson(const FEdGraphPinType& PinType) const
{
	TSharedPtr<FJsonObject> TypeInfo = MakeShareable(new FJsonObject);
	
	// Basic type category
	TypeInfo->SetStringField(TEXT("category"), PinType.PinCategory.ToString());
	
	// Sub-category if present
	if (!PinType.PinSubCategory.IsNone())
	{
		TypeInfo->SetStringField(TEXT("subCategory"), PinType.PinSubCategory.ToString());
	}
	
	// Object type if present
	if (PinType.PinSubCategoryObject.IsValid())
	{
		UObject* SubObject = PinType.PinSubCategoryObject.Get();
		if (SubObject)
		{
			TypeInfo->SetStringField(TEXT("objectType"), SubObject->GetName());
			TypeInfo->SetStringField(TEXT("objectPath"), SubObject->GetPathName());
		}
	}
	
	// Container type
	FString ContainerType;
	switch (PinType.ContainerType)
	{
		case EPinContainerType::None:
			ContainerType = TEXT("none");
			break;
		case EPinContainerType::Array:
			ContainerType = TEXT("array");
			break;
		case EPinContainerType::Set:
			ContainerType = TEXT("set");
			break;
		case EPinContainerType::Map:
			ContainerType = TEXT("map");
			break;
		default:
			ContainerType = TEXT("unknown");
			break;
	}
	TypeInfo->SetStringField(TEXT("container"), ContainerType);
	
	// For maps, include key type info
	if (PinType.ContainerType == EPinContainerType::Map)
	{
		TSharedPtr<FJsonObject> KeyTypeInfo = MakeShareable(new FJsonObject);
		KeyTypeInfo->SetStringField(TEXT("category"), PinType.PinValueType.TerminalCategory.ToString());
		if (!PinType.PinValueType.TerminalSubCategory.IsNone())
		{
			KeyTypeInfo->SetStringField(TEXT("subCategory"), PinType.PinValueType.TerminalSubCategory.ToString());
		}
		TypeInfo->SetObjectField(TEXT("keyType"), KeyTypeInfo);
	}
	
	// Flags
	TypeInfo->SetBoolField(TEXT("isReference"), PinType.bIsReference);
	TypeInfo->SetBoolField(TEXT("isConst"), PinType.bIsConst);
	TypeInfo->SetBoolField(TEXT("isWeakPointer"), PinType.bIsWeakPointer);
	
	return TypeInfo;
}

TSharedPtr<FJsonObject> FN2CMcpListBlueprintFunctionsTool::GetFunctionFlags(const UK2Node_FunctionEntry* EntryNode) const
{
	TSharedPtr<FJsonObject> Flags = MakeShareable(new FJsonObject);
	
	if (!EntryNode)
	{
		return Flags;
	}
	
	// Get function flags
	int32 ExtraFlags = EntryNode->GetExtraFlags();
	
	// Basic flags
	Flags->SetBoolField(TEXT("isPure"), (ExtraFlags & FUNC_BlueprintPure) != 0);
	Flags->SetBoolField(TEXT("isCallInEditor"), EntryNode->MetaData.bCallInEditor);
	Flags->SetBoolField(TEXT("isStatic"), (ExtraFlags & FUNC_Static) != 0);
	Flags->SetBoolField(TEXT("isConst"), (ExtraFlags & FUNC_Const) != 0);
	Flags->SetBoolField(TEXT("isPrivate"), (ExtraFlags & FUNC_Private) != 0);
	Flags->SetBoolField(TEXT("isProtected"), (ExtraFlags & FUNC_Protected) != 0);
	
	// Replication flags
	Flags->SetBoolField(TEXT("isReliable"), (ExtraFlags & FUNC_NetReliable) != 0);
	Flags->SetBoolField(TEXT("isServer"), (ExtraFlags & FUNC_NetServer) != 0);
	Flags->SetBoolField(TEXT("isClient"), (ExtraFlags & FUNC_NetClient) != 0);
	Flags->SetBoolField(TEXT("isMulticast"), (ExtraFlags & FUNC_NetMulticast) != 0);
	
	// Access specifier
	FString AccessSpecifier = TEXT("public");
	if (ExtraFlags & FUNC_Private)
	{
		AccessSpecifier = TEXT("private");
	}
	else if (ExtraFlags & FUNC_Protected)
	{
		AccessSpecifier = TEXT("protected");
	}
	Flags->SetStringField(TEXT("accessSpecifier"), AccessSpecifier);
	
	// Metadata
	TSharedPtr<FJsonObject> Metadata = MakeShareable(new FJsonObject);
	
	if (!EntryNode->MetaData.Category.IsEmpty())
	{
		Metadata->SetStringField(TEXT("category"), EntryNode->MetaData.Category.ToString());
	}
	
	if (!EntryNode->MetaData.Keywords.IsEmpty())
	{
		Metadata->SetStringField(TEXT("keywords"), EntryNode->MetaData.Keywords.ToString());
	}
	
	if (!EntryNode->MetaData.ToolTip.IsEmpty())
	{
		Metadata->SetStringField(TEXT("tooltip"), EntryNode->MetaData.ToolTip.ToString());
	}
	
	if (!EntryNode->MetaData.CompactNodeTitle.IsEmpty())
	{
		Metadata->SetStringField(TEXT("compactTitle"), EntryNode->MetaData.CompactNodeTitle.ToString());
	}
	
	Flags->SetObjectField(TEXT("metadata"), Metadata);
	
	return Flags;
}

FGuid FN2CMcpListBlueprintFunctionsTool::GetFunctionGuid(const UEdGraph* FunctionGraph) const
{
	if (!FunctionGraph)
	{
		return FGuid();
	}
	
	// Try to find the function entry node which might store a GUID
	for (const UEdGraphNode* Node : FunctionGraph->Nodes)
	{
		if (const UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(Node))
		{
			// In UE5, function GUIDs are typically stored in the FunctionReference
			// For now, we'll generate a consistent GUID based on the graph name
			// In a production implementation, you might store this in metadata
			return FGuid::NewGuid();
		}
	}
	
	return FGuid();
}