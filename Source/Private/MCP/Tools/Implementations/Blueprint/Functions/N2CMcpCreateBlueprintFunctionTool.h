// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/Tools/N2CMcpToolBase.h"
#include "Engine/EngineTypes.h"
#include "EdGraph/EdGraphPin.h"

// Forward declarations
class UBlueprint;
class UEdGraph;
class UK2Node_FunctionEntry;
class UK2Node_FunctionResult;

/**
 * MCP tool that creates a new Blueprint function with specified parameters
 * Supports various parameter types with containers and automatically opens the created function
 */
class FN2CMcpCreateBlueprintFunctionTool : public FN2CMcpToolBase
{
public:
	virtual FMcpToolDefinition GetDefinition() const override;
	virtual FMcpToolCallResult Execute(const TSharedPtr<FJsonObject>& Arguments) override;
	virtual bool RequiresGameThread() const override { return true; }

private:
	// Parameter definition structure
	struct FParameterDefinition
	{
		FString Name;
		FString Direction = TEXT("input");  // "input" or "output"
		FString Type;
		FString SubType;
		FString Container = TEXT("none");  // "none", "array", "set", "map"
		FString KeyType;  // For map containers
		bool bIsReference = false;
		bool bIsConst = false;
		FString DefaultValue;
	};

	// Function flags structure
	struct FFunctionFlags
	{
		bool bIsPure = false;
		bool bIsCallInEditor = false;
		bool bIsStatic = false;
		bool bIsConst = false;
		bool bIsOverride = false;
		bool bIsReliableReplication = false;
		bool bIsMulticast = false;
		FString Category = TEXT("Default");
		FString Keywords;
		FString Tooltip;
		FString AccessSpecifier = TEXT("public");  // "public", "protected", "private"
	};

	// Main operations
	bool ValidateFunctionName(const UBlueprint* Blueprint, const FString& FunctionName, FString& OutError) const;
	UEdGraph* CreateFunctionGraph(UBlueprint* Blueprint, const FString& FunctionName);
	void CreateFunctionParameters(UK2Node_FunctionEntry* EntryNode, UK2Node_FunctionResult* ResultNode,
		const TArray<FParameterDefinition>& Parameters);
	void SetFunctionMetadata(UK2Node_FunctionEntry* EntryNode, const FFunctionFlags& Flags);
	FGuid GetOrCreateFunctionGuid(UEdGraph* FunctionGraph) const;
	
	// Type conversion
	// bool ConvertToPinType(const FParameterDefinition& ParamDef, FEdGraphPinType& OutPinType, FString& OutError) const; // Removed
	// bool ResolvePrimitiveType(const FString& Type, FEdGraphPinType& OutPinType) const; // Removed
	// bool ResolveMathType(const FString& Type, FEdGraphPinType& OutPinType) const; // Removed
	// bool ResolveReferenceType(const FString& Type, const FString& SubType, FEdGraphPinType& OutPinType, FString& OutError) const; // Removed
	// bool ResolveSpecialType(const FString& Type, FEdGraphPinType& OutPinType) const; // Removed
	// UObject* ResolveSubType(const FString& Type, const FString& SubType) const; // Removed
	
	// Parameter creation
	void CreateParameter(UK2Node_FunctionEntry* EntryNode, UK2Node_FunctionResult* ResultNode,
		const FParameterDefinition& ParamDef, const FEdGraphPinType& PinType);
	UK2Node_FunctionResult* FindOrCreateResultNode(UK2Node_FunctionEntry* EntryNode);
	
	// Result building
	TSharedPtr<FJsonObject> BuildSuccessResult(const UBlueprint* Blueprint, const FString& FunctionName,
		const FGuid& FunctionGuid, UEdGraph* FunctionGraph) const;
	
	// Parsing helpers
	bool ParseParameters(const TSharedPtr<FJsonValue>& ParametersValue, TArray<FParameterDefinition>& OutParameters, FString& OutError) const;
	bool ParseFunctionFlags(const TSharedPtr<FJsonObject>& FlagsObject, FFunctionFlags& OutFlags) const;
	
	// Editor integration
	void OpenFunctionInEditor(UBlueprint* Blueprint, UEdGraph* FunctionGraph);
};
