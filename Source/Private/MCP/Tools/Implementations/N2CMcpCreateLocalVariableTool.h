// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/Tools/N2CMcpToolBase.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraphNode.h"
#include "Engine/BlueprintGeneratedClass.h"

// Forward declarations
class UK2Node_FunctionEntry;
class UEdGraph;
struct FEdGraphPinType;
struct FBPVariableDescription;

/**
 * MCP tool that creates a new local variable within the currently focused Blueprint function.
 * Local variables are scoped to a specific function and stored in the UK2Node_FunctionEntry node.
 */
class FN2CMcpCreateLocalVariableTool : public FN2CMcpToolBase
{
public:
	virtual FMcpToolDefinition GetDefinition() const override;
	virtual FMcpToolCallResult Execute(const TSharedPtr<FJsonObject>& Arguments) override;
	virtual bool RequiresGameThread() const override { return true; }

private:
	// Core workflow methods
	UK2Node_FunctionEntry* FindFunctionEntryNode(UEdGraph* Graph) const;
	// bool ResolveTypeIdentifier(const FString& TypeIdentifier, FEdGraphPinType& OutPinType, FString& OutError) const; // Removed
	FName MakeUniqueLocalVariableName(UK2Node_FunctionEntry* FunctionEntry, const FString& BaseName) const;
	FName CreateLocalVariable(UK2Node_FunctionEntry* FunctionEntry, const FString& DesiredName,
		const FEdGraphPinType& PinType, const FString& DefaultValue, const FString& Tooltip);
	
	// Type resolution helpers (similar to CreateVariable tool)
	// bool IsPrimitiveType(const FString& TypeIdentifier) const; // Removed
	// bool ResolvePrimitiveType(const FString& TypeIdentifier, FEdGraphPinType& OutPinType) const; // Removed
	// bool ResolveObjectType(const FString& TypeIdentifier, FEdGraphPinType& OutPinType, FString& OutError) const; // Removed
	
	// Result building
	TSharedPtr<FJsonObject> BuildSuccessResult(UK2Node_FunctionEntry* FunctionEntry, UEdGraph* FunctionGraph,
		const FString& RequestedName, FName ActualName, const FEdGraphPinType& PinType, const FString& ContainerType) const;
};
