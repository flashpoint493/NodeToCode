// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/Tools/N2CMcpToolBase.h"

class UBlueprint;
class FBlueprintEditor;

class FN2CMcpGetOpenBlueprintEditorsTool : public FN2CMcpToolBase
{
public:
	virtual FMcpToolDefinition GetDefinition() const override;
	virtual FMcpToolCallResult Execute(const TSharedPtr<FJsonObject>& Arguments) override;
	
	// Always requires game thread for editor access
	virtual bool RequiresGameThread() const override { return true; }

private:
	// Collects information about a single Blueprint editor
	TSharedPtr<FJsonObject> CollectEditorInfo(const TSharedPtr<FBlueprintEditor>& Editor) const;
	
	// Gets the string representation of a Blueprint type
	FString GetBlueprintTypeString(EBlueprintType Type) const;
	
	// Collects all graphs from a Blueprint and returns them as JSON array
	TArray<TSharedPtr<FJsonValue>> CollectBlueprintGraphs(UBlueprint* Blueprint) const;
};