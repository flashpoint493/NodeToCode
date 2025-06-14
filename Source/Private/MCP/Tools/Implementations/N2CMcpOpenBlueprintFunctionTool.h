// Copyright Protospatial 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/Tools/N2CMcpToolBase.h"

// Forward declarations
class UBlueprint;
class UEdGraph;
class UK2Node_FunctionEntry;
class IBlueprintEditor;

/**
 * MCP tool that opens a specific Blueprint function in the editor using its GUID
 * The function GUID is obtained from CreateBlueprintFunction or ListBlueprintFunctions tools
 */
class FN2CMcpOpenBlueprintFunctionTool : public FN2CMcpToolBase
{
public:
	virtual FMcpToolDefinition GetDefinition() const override;
	virtual FMcpToolCallResult Execute(const TSharedPtr<FJsonObject>& Arguments) override;
	virtual bool RequiresGameThread() const override { return true; }

private:
	// Function discovery
	UEdGraph* FindFunctionByGuid(const FGuid& FunctionGuid, const FString& BlueprintPath, UBlueprint*& OutBlueprint) const;
	UEdGraph* SearchFunctionInBlueprint(UBlueprint* Blueprint, const FGuid& FunctionGuid) const;
	FGuid GetFunctionGuid(const UEdGraph* FunctionGraph) const;
	
	// Blueprint resolution
	UBlueprint* ResolveTargetBlueprint(const FString& BlueprintPath) const;
	UBlueprint* GetFocusedBlueprint() const;
	
	// Editor operations
	bool OpenBlueprintEditor(UBlueprint* Blueprint, TSharedPtr<IBlueprintEditor>& OutEditor) const;
	bool NavigateToFunction(TSharedPtr<IBlueprintEditor> Editor, UEdGraph* FunctionGraph) const;
	bool CenterViewOnFunction(TSharedPtr<IBlueprintEditor> Editor, UEdGraph* FunctionGraph) const;
	bool SelectAllNodesInFunction(TSharedPtr<IBlueprintEditor> Editor, UEdGraph* FunctionGraph) const;
	
	// Result building
	TSharedPtr<FJsonObject> BuildSuccessResult(UBlueprint* Blueprint, UEdGraph* FunctionGraph, const FGuid& FunctionGuid) const;
	
	// Helper functions
	UK2Node_FunctionEntry* GetFunctionEntryNode(UEdGraph* FunctionGraph) const;
	FString GetFunctionDisplayName(UEdGraph* FunctionGraph) const;
};