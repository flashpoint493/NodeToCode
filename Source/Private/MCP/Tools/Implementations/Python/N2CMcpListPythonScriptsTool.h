// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "N2CMcpPythonScriptToolBase.h"

/**
 * MCP tool for listing available Python scripts from the script library.
 * Returns scripts with metadata (name, description, tags, usage stats).
 */
class FN2CMcpListPythonScriptsTool : public FN2CMcpPythonScriptToolBase
{
public:
	virtual FMcpToolDefinition GetDefinition() const override;
	virtual FMcpToolCallResult Execute(const TSharedPtr<FJsonObject>& Arguments) override;
};
