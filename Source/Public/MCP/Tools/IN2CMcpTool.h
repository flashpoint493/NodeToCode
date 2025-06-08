// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/Tools/N2CMcpToolTypes.h"

/**
 * Interface for MCP tools
 * All MCP tools must implement this interface to be registered with the tool system
 */
class NODETOCODE_API IN2CMcpTool
{
public:
	virtual ~IN2CMcpTool() = default;
	
	/**
	 * Get the tool definition containing metadata (name, description, schema)
	 * @return The tool definition
	 */
	virtual FMcpToolDefinition GetDefinition() const = 0;
	
	/**
	 * Execute the tool with given arguments
	 * @param Arguments The JSON arguments passed to the tool
	 * @return The result of the tool execution
	 */
	virtual FMcpToolCallResult Execute(const TSharedPtr<FJsonObject>& Arguments) = 0;
	
	/**
	 * Check if tool requires Game Thread execution
	 * Tools that interact with Unreal Editor APIs should return true
	 * @return true if the tool must run on the Game Thread
	 */
	virtual bool RequiresGameThread() const { return false; }
};