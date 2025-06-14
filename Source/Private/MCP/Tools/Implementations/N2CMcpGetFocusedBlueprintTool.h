// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/Tools/N2CMcpToolBase.h"

/**
 * MCP tool for retrieving the currently focused Blueprint graph as N2CJSON
 */
class FN2CMcpGetFocusedBlueprintTool : public FN2CMcpToolBase
{
public:
	/** Get the tool definition */
	virtual FMcpToolDefinition GetDefinition() const override;
	
	/** Execute the tool */
	virtual FMcpToolCallResult Execute(const TSharedPtr<FJsonObject>& Arguments) override;
	
	/** This tool requires Game Thread execution for accessing Editor APIs */
	virtual bool RequiresGameThread() const override { return true; }

private:
	/**
	 * Enhance the JSON output with GUID information from the translator
	 * @param JsonString The original JSON string  
	 * @return Enhanced JSON string with GUID information
	 */
	FString EnhanceJsonWithGuids(const FString& JsonString) const;
};