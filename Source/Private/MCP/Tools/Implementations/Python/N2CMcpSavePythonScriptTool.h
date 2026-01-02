// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "N2CMcpPythonScriptToolBase.h"

/**
 * MCP tool for saving a new Python script to the script library.
 * Saves the script code to disk and updates the registry with metadata.
 */
class FN2CMcpSavePythonScriptTool : public FN2CMcpPythonScriptToolBase
{
public:
	virtual FMcpToolDefinition GetDefinition() const override;
	virtual FMcpToolCallResult Execute(const TSharedPtr<FJsonObject>& Arguments) override;
};
