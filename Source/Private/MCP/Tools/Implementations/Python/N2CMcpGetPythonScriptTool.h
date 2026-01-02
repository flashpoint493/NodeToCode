// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "N2CMcpPythonScriptToolBase.h"

/**
 * MCP tool for retrieving a Python script's full code and metadata.
 * Use this to load a script before executing or modifying it.
 */
class FN2CMcpGetPythonScriptTool : public FN2CMcpPythonScriptToolBase
{
public:
	virtual FMcpToolDefinition GetDefinition() const override;
	virtual FMcpToolCallResult Execute(const TSharedPtr<FJsonObject>& Arguments) override;
};
