// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#include "N2CMcpTranslateBlueprintTool.h"
#include "MCP/Tools/N2CMcpToolRegistry.h" // For REGISTER_MCP_TOOL
#include "Utils/N2CLogger.h"

REGISTER_MCP_TOOL(FN2CMcpTranslateBlueprintTool)

FMcpToolDefinition FN2CMcpTranslateBlueprintTool::GetDefinition() const
{
    FMcpToolDefinition Definition(
        TEXT("translate-focused-blueprint"),
        TEXT("Translates the currently focused Blueprint graph using an LLM provider. This is a long-running task and requires a _meta.progressToken for SSE streaming."),
        TEXT("Translation")
    );

    Definition.bIsLongRunning = true; // Mark as long-running

    TSharedPtr<FJsonObject> Properties = MakeShareable(new FJsonObject);

    TSharedPtr<FJsonObject> ProviderProp = MakeShareable(new FJsonObject);
    ProviderProp->SetStringField(TEXT("type"), TEXT("string"));
    ProviderProp->SetStringField(TEXT("description"), TEXT("Optional: LLM Provider ID (e.g., 'openai', 'anthropic', 'ollama'). Uses settings default if empty."));
    Properties->SetObjectField(TEXT("provider"), ProviderProp);

    TSharedPtr<FJsonObject> ModelProp = MakeShareable(new FJsonObject);
    ModelProp->SetStringField(TEXT("type"), TEXT("string"));
    ModelProp->SetStringField(TEXT("description"), TEXT("Optional: Specific model ID. Uses provider's default from settings if empty."));
    Properties->SetObjectField(TEXT("model"), ModelProp);

    TSharedPtr<FJsonObject> LanguageProp = MakeShareable(new FJsonObject);
    LanguageProp->SetStringField(TEXT("type"), TEXT("string"));
    LanguageProp->SetStringField(TEXT("description"), TEXT("Optional: Target language ID (e.g., 'cpp', 'python'). Uses settings default if empty."));
    Properties->SetObjectField(TEXT("language"), LanguageProp);
    
    TSharedPtr<FJsonObject> Schema = MakeShareable(new FJsonObject);
    Schema->SetStringField(TEXT("type"), TEXT("object"));
    Schema->SetObjectField(TEXT("properties"), Properties);
    // No required fields, all are optional and will use defaults from settings.
    
    Definition.InputSchema = Schema;

    return Definition;
}

FMcpToolCallResult FN2CMcpTranslateBlueprintTool::Execute(const TSharedPtr<FJsonObject>& Arguments)
{
    // This tool is designed to be run asynchronously via SSE.
    // If this Execute method is called directly, it means the async path in FN2CMcpHttpServerManager was not taken.
    FN2CLogger::Get().LogWarning(TEXT("translate-focused-blueprint tool was called synchronously. This indicates an issue with async task setup or a client calling without SSE support."));
    
    // Provide an error indicating that this tool expects asynchronous handling.
    return FMcpToolCallResult::CreateErrorResult(
        TEXT("The 'translate-focused-blueprint' tool is a long-running task and expects to be handled asynchronously via SSE. ")
        TEXT("This synchronous execution path should not typically be reached. Check server logs for async setup issues.")
    );
}
