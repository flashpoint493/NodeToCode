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