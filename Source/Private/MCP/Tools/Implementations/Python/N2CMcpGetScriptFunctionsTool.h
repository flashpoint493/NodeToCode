// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "N2CMcpPythonScriptToolBase.h"

/**
 * MCP tool for extracting function signatures from a Python script using AST.
 * Returns function names, parameters, types, and docstrings without full implementation.
 * This is much more token-efficient than get-python-script for discovering available functions.
 */
class FN2CMcpGetScriptFunctionsTool : public FN2CMcpPythonScriptToolBase
{
public:
	virtual FMcpToolDefinition GetDefinition() const override;
	virtual FMcpToolCallResult Execute(const TSharedPtr<FJsonObject>& Arguments) override;
};
