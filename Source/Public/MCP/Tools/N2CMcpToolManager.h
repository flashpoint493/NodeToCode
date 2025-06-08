// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/Tools/N2CMcpToolTypes.h"

/**
 * Manager for MCP tools registration and execution
 * Singleton pattern to manage all available MCP tools
 */
class NODETOCODE_API FN2CMcpToolManager
{
public:
	/** Get the singleton instance */
	static FN2CMcpToolManager& Get();

	/** Destructor */
	~FN2CMcpToolManager() = default;

	/**
	 * Register a new tool with the manager
	 * @param Definition The tool definition containing metadata
	 * @param Handler The delegate to call when the tool is invoked
	 * @return true if registration was successful
	 */
	bool RegisterTool(const FMcpToolDefinition& Definition, const FMcpToolHandlerDelegate& Handler);

	/**
	 * Unregister a tool by name
	 * @param ToolName The name of the tool to unregister
	 * @return true if the tool was found and unregistered
	 */
	bool UnregisterTool(const FString& ToolName);

	/**
	 * Get a tool definition by name
	 * @param ToolName The name of the tool
	 * @return The tool definition, or nullptr if not found
	 */
	const FMcpToolDefinition* GetToolDefinition(const FString& ToolName) const;

	/**
	 * Get all registered tool definitions
	 * @return Array of all tool definitions
	 */
	TArray<FMcpToolDefinition> GetAllToolDefinitions() const;

	/**
	 * Execute a tool by name
	 * @param ToolName The name of the tool to execute
	 * @param Arguments The JSON arguments to pass to the tool
	 * @return The result of the tool execution
	 */
	FMcpToolCallResult ExecuteTool(const FString& ToolName, const TSharedPtr<FJsonObject>& Arguments);

	/**
	 * Check if a tool is registered
	 * @param ToolName The name of the tool
	 * @return true if the tool is registered
	 */
	bool IsToolRegistered(const FString& ToolName) const;

	/** Clear all registered tools */
	void ClearAllTools();

private:
	/** Private constructor for singleton */
	FN2CMcpToolManager() = default;

	/** Prevent copying */
	FN2CMcpToolManager(const FN2CMcpToolManager&) = delete;
	FN2CMcpToolManager& operator=(const FN2CMcpToolManager&) = delete;

	/** Structure to hold tool definition and handler together */
	struct FToolEntry
	{
		FMcpToolDefinition Definition;
		FMcpToolHandlerDelegate Handler;
	};

	/** Map of tool names to their entries */
	TMap<FString, FToolEntry> RegisteredTools;

	/** Mutex for thread-safe access */
	mutable FCriticalSection ToolsLock;
};