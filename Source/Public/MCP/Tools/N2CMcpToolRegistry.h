// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/Tools/IN2CMcpTool.h"

/**
 * Registry for MCP tools with auto-registration support
 * Tools can register themselves during static initialization
 */
class NODETOCODE_API FN2CMcpToolRegistry
{
public:
	/** Get the singleton instance */
	static FN2CMcpToolRegistry& Get();
	
	/**
	 * Register a tool instance with the registry
	 * @param Tool The tool instance to register
	 */
	void RegisterTool(TSharedPtr<IN2CMcpTool> Tool);
	
	
	/**
	 * Get all registered tools
	 * @return Array of all registered tool instances
	 */
	const TArray<TSharedPtr<IN2CMcpTool>>& GetTools() const { return Tools; }
	
	/**
	 * Clear all registered tools
	 * Mainly for testing purposes
	 */
	void ClearTools() { Tools.Empty(); }
	
private:
	/** Private constructor for singleton */
	FN2CMcpToolRegistry() = default;
	
	/** Array of registered tool instances */
	TArray<TSharedPtr<IN2CMcpTool>> Tools;
	
	/** Mutex for thread-safe registration */
	FCriticalSection RegistrationLock;
};

/**
 * Helper struct for automatic tool registration during static initialization
 */
struct FN2CMcpToolAutoRegistration
{
	/**
	 * Constructor that registers a tool with the registry
	 * @param ToolFactory Function that creates the tool instance
	 */
	FN2CMcpToolAutoRegistration(TFunction<TSharedPtr<IN2CMcpTool>()> ToolFactory)
	{
		FN2CMcpToolRegistry::Get().RegisterTool(ToolFactory());
	}
};

/**
 * Macro for automatic tool registration
 * Usage: REGISTER_MCP_TOOL(YourToolClass) in the .cpp file
 */
#define REGISTER_MCP_TOOL(ToolClass) \
	static FN2CMcpToolAutoRegistration ToolClass##_AutoReg([]() { \
		return MakeShareable(new ToolClass()); \
	});