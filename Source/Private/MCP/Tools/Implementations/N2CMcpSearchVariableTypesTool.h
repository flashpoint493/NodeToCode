// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/Tools/N2CMcpToolBase.h"

// Forward declarations
class UEdGraphSchema_K2;
struct FAssetData;

/**
 * MCP tool that searches for available variable types in the Blueprint editor
 * Returns type names, identifiers, categories, and descriptions for use with CreateVariable
 */
class FN2CMcpSearchVariableTypesTool : public FN2CMcpToolBase
{
public:
	virtual FMcpToolDefinition GetDefinition() const override;
	virtual FMcpToolCallResult Execute(const TSharedPtr<FJsonObject>& Arguments) override;
	virtual bool RequiresGameThread() const override { return true; }

private:
	// Type information structure
	struct FVariableTypeInfo
	{
		FString TypeName;
		FString TypeIdentifier;
		FString Category;
		FString Description;
		FString Icon;
		FString ParentClass;
		bool bIsAbstract;
		TArray<FString> EnumValues;

		FVariableTypeInfo() : bIsAbstract(false) {}
	};

	// Type collection methods
	void CollectPrimitiveTypes(TArray<FVariableTypeInfo>& OutTypes) const;
	void CollectClassTypes(TArray<FVariableTypeInfo>& OutTypes, bool bIncludeEngineTypes) const;
	void CollectStructTypes(TArray<FVariableTypeInfo>& OutTypes, bool bIncludeEngineTypes) const;
	void CollectEnumTypes(TArray<FVariableTypeInfo>& OutTypes, bool bIncludeEngineTypes) const;

	// Helper methods - using fully qualified type names
	void ProcessTypeTree(const TArray<TSharedPtr<class UEdGraphSchema_K2::FPinTypeTreeInfo>>& TypeTree, 
		TArray<FVariableTypeInfo>& OutTypes, const FString& Category, bool bIncludeEngineTypes) const;
	void ProcessBlueprintAssets(const TArray<FAssetData>& BlueprintAssets, 
		TArray<FVariableTypeInfo>& OutTypes) const;
	TArray<FVariableTypeInfo> FilterTypesBySearchTerm(const TArray<FVariableTypeInfo>& AllTypes,
		const FString& SearchTerm, int32 MaxResults) const;

	// JSON conversion
	TSharedPtr<FJsonObject> BuildJsonResult(const TArray<FVariableTypeInfo>& FilteredTypes) const;
	
	// Utility
	bool IsEngineType(const FString& TypePath) const;
	FString GetTypeDescription(const UObject* TypeObject) const;
};