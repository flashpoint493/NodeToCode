// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/Tools/N2CMcpToolBase.h"

/**
 * Base class for MCP tools that call nodetocode Python script management functions.
 * Provides common Python execution infrastructure for script CRUD operations.
 */
class FN2CMcpPythonScriptToolBase : public FN2CMcpToolBase
{
public:
	// All Python script tools need Game Thread for UE API access
	virtual bool RequiresGameThread() const override { return true; }

protected:
	/**
	 * Execute a nodetocode module function and return the result.
	 * @param FunctionCall The Python function call (e.g., "list_scripts(category='gameplay')")
	 * @return Tool result containing the function's return value
	 */
	FMcpToolCallResult ExecuteNodeToCodeFunction(const FString& FunctionCall);

	/**
	 * Build a Python string literal with proper escaping.
	 * @param Value The string value to escape
	 * @return Escaped string suitable for Python code
	 */
	static FString EscapePythonString(const FString& Value);

	/**
	 * Build a Python list literal from a string array.
	 * @param Values The array of strings
	 * @return Python list literal (e.g., "['tag1', 'tag2']")
	 */
	static FString BuildPythonList(const TArray<FString>& Values);

	/** Default timeout in seconds for Python execution */
	static constexpr float DefaultTimeoutSeconds = 30.0f;
};
