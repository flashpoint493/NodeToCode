// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#include "MCP/Tools/N2CMcpToolRegistry.h"
#include "MCP/Tools/N2CMcpToolManager.h"
#include "Utils/N2CLogger.h"

FN2CMcpToolRegistry& FN2CMcpToolRegistry::Get()
{
	static FN2CMcpToolRegistry Instance;
	return Instance;
}

void FN2CMcpToolRegistry::RegisterTool(TSharedPtr<IN2CMcpTool> Tool)
{
	if (!Tool.IsValid())
	{
		FN2CLogger::Get().LogWarning(TEXT("Attempted to register invalid MCP tool"));
		return;
	}
	
	FScopeLock Lock(&RegistrationLock);
	
	FMcpToolDefinition Definition = Tool->GetDefinition();
	FN2CLogger::Get().Log(FString::Printf(TEXT("Registering MCP tool: %s"), *Definition.Name), EN2CLogSeverity::Debug);
	
	Tools.Add(Tool);
}

void FN2CMcpToolRegistry::RegisterAllToolsWithManager()
{
	FScopeLock Lock(&RegistrationLock);
	
	FN2CLogger::Get().Log(FString::Printf(TEXT("Registering %d MCP tools with manager"), Tools.Num()), EN2CLogSeverity::Info);
	
	for (const TSharedPtr<IN2CMcpTool>& Tool : Tools)
	{
		if (!Tool.IsValid())
		{
			continue;
		}
		
		FMcpToolDefinition Definition = Tool->GetDefinition();
		
		// Create a handler delegate that captures the tool instance
		FMcpToolHandlerDelegate Handler;
		TWeakPtr<IN2CMcpTool> WeakTool = Tool;
		
		Handler.BindLambda([WeakTool, Definition](const TSharedPtr<FJsonObject>& Args) -> FMcpToolCallResult
		{
			TSharedPtr<IN2CMcpTool> PinnedTool = WeakTool.Pin();
			if (!PinnedTool.IsValid())
			{
				FN2CLogger::Get().LogError(FString::Printf(TEXT("MCP tool '%s' is no longer valid"), *Definition.Name));
				return FMcpToolCallResult::CreateErrorResult(TEXT("Tool is no longer valid"));
			}
			
			// Execute the tool
			return PinnedTool->Execute(Args);
		});
		
		// Register with the tool manager
		if (FN2CMcpToolManager::Get().RegisterTool(Definition, Handler))
		{
			FN2CLogger::Get().Log(FString::Printf(TEXT("Successfully registered MCP tool: %s"), *Definition.Name), EN2CLogSeverity::Info);
		}
		else
		{
			FN2CLogger::Get().LogError(FString::Printf(TEXT("Failed to register MCP tool: %s"), *Definition.Name));
		}
	}
}