// Copyright Protospatial 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/Tools/N2CMcpToolBase.h"
#include "Engine/EngineTypes.h"
#include "EdGraph/EdGraphPin.h"

// Forward declarations
class UBlueprint;
class FProperty;

/**
 * MCP tool that creates a new variable in the active Blueprint
 * Uses type identifiers from search-variable-types to create variables with specific types
 */
class FN2CMcpCreateVariableTool : public FN2CMcpToolBase
{
public:
	virtual FMcpToolDefinition GetDefinition() const override;
	virtual FMcpToolCallResult Execute(const TSharedPtr<FJsonObject>& Arguments) override;
	virtual bool RequiresGameThread() const override { return true; }

private:
	// Variable creation settings
	struct FVariableCreationSettings
	{
		FString VariableName;
		FString TypeIdentifier;
		FString DefaultValue;
		FString Category = TEXT("Default");
		bool bInstanceEditable = true;
		bool bBlueprintReadOnly = false;	
		FString Tooltip;
		FString ReplicationCondition = TEXT("none");
	};

	// Main operations
	UBlueprint* GetActiveBlueprint() const;
	bool ResolveTypeIdentifier(const FString& TypeIdentifier, FEdGraphPinType& OutPinType, FString& OutError) const;
	FName CreateVariable(UBlueprint* Blueprint, const FString& DesiredName, 
		const FEdGraphPinType& PinType, const FString& DefaultValue, const FString& Category);
	void ApplyVariableProperties(UBlueprint* Blueprint, FName VariableName,
		bool bInstanceEditable, bool bBlueprintReadOnly,
		const FString& Tooltip, const FString& ReplicationCondition);

	// Type resolution helpers
	bool ResolvePrimitiveType(const FString& TypeIdentifier, FEdGraphPinType& OutPinType) const;
	bool ResolveObjectType(const FString& TypeIdentifier, FEdGraphPinType& OutPinType, FString& OutError) const;
	
	// Result building
	TSharedPtr<FJsonObject> BuildSuccessResult(const UBlueprint* Blueprint, const FString& RequestedName,
		FName ActualName, const FEdGraphPinType& PinType) const;
	
	// Validation
	bool ValidateVariableName(const FString& VariableName, FString& OutError) const;
	bool ValidateBlueprintModifiable(const UBlueprint* Blueprint, FString& OutError) const;
};