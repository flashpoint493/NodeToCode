// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "N2CMcpPythonScriptToolBase.h"

/**
 * MCP tool for deleting a Python script from the script library.
 * Removes both the script file and its registry entry.
 */
class FN2CMcpDeletePythonScriptTool : public FN2CMcpPythonScriptToolBase
{
public:
	virtual FMcpToolDefinition GetDefinition() const override;
	virtual FMcpToolCallResult Execute(const TSharedPtr<FJsonObject>& Arguments) override;
};
