// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#include "MCP/Tools/N2CMcpToolManager.h"
#include "Utils/N2CLogger.h"

FN2CMcpToolManager& FN2CMcpToolManager::Get()
{
	static FN2CMcpToolManager Instance;
	return Instance;
}

bool FN2CMcpToolManager::RegisterTool(const FMcpToolDefinition& Definition, const FMcpToolHandlerDelegate& Handler)
{
	if (Definition.Name.IsEmpty())
	{
		FN2CLogger::Get().LogWarning(TEXT("Cannot register tool with empty name"));
		return false;
	}

	if (!Handler.IsBound())
	{
		FN2CLogger::Get().LogWarning(FString::Printf(TEXT("Cannot register tool '%s' with unbound handler"), *Definition.Name));
		return false;
	}

	FScopeLock Lock(&ToolsLock);

	if (RegisteredTools.Contains(Definition.Name))
	{
		FN2CLogger::Get().LogWarning(FString::Printf(TEXT("Tool '%s' is already registered"), *Definition.Name));
		return false;
	}

	FToolEntry& Entry = RegisteredTools.Add(Definition.Name);
	Entry.Definition = Definition;
	Entry.Handler = Handler;

	FN2CLogger::Get().Log(FString::Printf(TEXT("Registered MCP tool: %s"), *Definition.Name), EN2CLogSeverity::Info);
	return true;
}

bool FN2CMcpToolManager::UnregisterTool(const FString& ToolName)
{
	FScopeLock Lock(&ToolsLock);

	if (RegisteredTools.Remove(ToolName) > 0)
	{
		FN2CLogger::Get().Log(FString::Printf(TEXT("Unregistered MCP tool: %s"), *ToolName), EN2CLogSeverity::Info);
		return true;
	}

	return false;
}

const FMcpToolDefinition* FN2CMcpToolManager::GetToolDefinition(const FString& ToolName) const
{
	FScopeLock Lock(&ToolsLock);

	if (const FToolEntry* Entry = RegisteredTools.Find(ToolName))
	{
		return &Entry->Definition;
	}

	return nullptr;
}

TArray<FMcpToolDefinition> FN2CMcpToolManager::GetAllToolDefinitions() const
{
	FScopeLock Lock(&ToolsLock);

	TArray<FMcpToolDefinition> Definitions;
	Definitions.Reserve(RegisteredTools.Num());

	for (const auto& Pair : RegisteredTools)
	{
		Definitions.Add(Pair.Value.Definition);
	}

	return Definitions;
}

FMcpToolCallResult FN2CMcpToolManager::ExecuteTool(const FString& ToolName, const TSharedPtr<FJsonObject>& Arguments)
{
	FScopeLock Lock(&ToolsLock);

	const FToolEntry* Entry = RegisteredTools.Find(ToolName);
	if (!Entry)
	{
		FN2CLogger::Get().LogWarning(FString::Printf(TEXT("Tool '%s' not found"), *ToolName));
		return FMcpToolCallResult::CreateErrorResult(FString::Printf(TEXT("Tool '%s' not found"), *ToolName));
	}

	if (!Entry->Handler.IsBound())
	{
		FN2CLogger::Get().LogError(FString::Printf(TEXT("Tool '%s' has unbound handler"), *ToolName));
		return FMcpToolCallResult::CreateErrorResult(FString::Printf(TEXT("Tool '%s' has invalid handler"), *ToolName));
	}

	// Execute the tool handler
	FN2CLogger::Get().Log(FString::Printf(TEXT("Executing MCP tool: %s"), *ToolName), EN2CLogSeverity::Debug);
	
	try
	{
		return Entry->Handler.Execute(Arguments);
	}
	catch (...)
	{
		FN2CLogger::Get().LogError(FString::Printf(TEXT("Exception thrown while executing tool '%s'"), *ToolName));
		return FMcpToolCallResult::CreateErrorResult(FString::Printf(TEXT("Internal error executing tool '%s'"), *ToolName));
	}
}

bool FN2CMcpToolManager::IsToolRegistered(const FString& ToolName) const
{
	FScopeLock Lock(&ToolsLock);
	return RegisteredTools.Contains(ToolName);
}

void FN2CMcpToolManager::ClearAllTools()
{
	FScopeLock Lock(&ToolsLock);
	RegisteredTools.Empty();
	FN2CLogger::Get().Log(TEXT("Cleared all registered MCP tools"), EN2CLogSeverity::Info);
}