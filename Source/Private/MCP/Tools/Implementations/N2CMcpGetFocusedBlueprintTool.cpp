// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#include "N2CMcpGetFocusedBlueprintTool.h"
#include "MCP/Tools/N2CMcpToolRegistry.h"
#include "Core/N2CEditorIntegration.h"
#include "Utils/N2CLogger.h"

// Auto-register this tool
REGISTER_MCP_TOOL(FN2CMcpGetFocusedBlueprintTool)

FMcpToolDefinition FN2CMcpGetFocusedBlueprintTool::GetDefinition() const
{
	FMcpToolDefinition Definition(
		TEXT("get-focused-blueprint"),
		TEXT("Collects and serializes the currently focused Blueprint graph in the Unreal Editor into NodeToCode's N2CJSON format.")
	);
	
	// This tool takes no input parameters
	Definition.InputSchema = BuildEmptyObjectSchema();
	
	// Add read-only annotation
	AddReadOnlyAnnotation(Definition);
	
	return Definition;
}

FMcpToolCallResult FN2CMcpGetFocusedBlueprintTool::Execute(const TSharedPtr<FJsonObject>& Arguments)
{
	// Since this tool requires Game Thread execution, use the base class helper
	return ExecuteOnGameThread([this]() -> FMcpToolCallResult
	{
		FString ErrorMsg;
		FString JsonOutput = FN2CEditorIntegration::Get().GetFocusedBlueprintAsJson(false /* no pretty print */, ErrorMsg);
		
		if (!JsonOutput.IsEmpty())
		{
			FN2CLogger::Get().Log(TEXT("get-focused-blueprint tool successfully retrieved Blueprint JSON"), EN2CLogSeverity::Info);
			return FMcpToolCallResult::CreateTextResult(JsonOutput);
		}
		else
		{
			FN2CLogger::Get().LogWarning(FString::Printf(TEXT("get-focused-blueprint tool failed: %s"), *ErrorMsg));
			return FMcpToolCallResult::CreateErrorResult(ErrorMsg);
		}
	});
}