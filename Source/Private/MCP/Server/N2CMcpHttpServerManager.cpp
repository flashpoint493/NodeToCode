// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#include "MCP/Server/N2CMcpHttpServerManager.h"
#include "MCP/Server/N2CMcpHttpRequestHandler.h"
#include "Utils/N2CLogger.h"
#include "HttpServerModule.h"
#include "HttpServerResponse.h"
#include "HttpPath.h"
#include "MCP/Tools/N2CMcpToolManager.h"
#include "Core/N2CSettings.h"
#include "Core/N2CEditorIntegration.h"
#include "Async/Async.h"
#include "Async/TaskGraphInterfaces.h"

FN2CMcpHttpServerManager& FN2CMcpHttpServerManager::Get()
{
	static FN2CMcpHttpServerManager Instance;
	return Instance;
}

bool FN2CMcpHttpServerManager::StartServer(int32 Port)
{
	if (bIsServerRunning)
	{
		FN2CLogger::Get().LogWarning(FString::Printf(TEXT("MCP HTTP server is already running on port %d"), ServerPort));
		return true;
	}

	// Get the HTTP server module
	HttpServerModule = &FModuleManager::LoadModuleChecked<FHttpServerModule>(FName("HTTPServer"));
	if (!HttpServerModule)
	{
		FN2CLogger::Get().LogError(TEXT("Failed to load HTTPServer module"));
		return false;
	}

	// Create HTTP router
	HttpRouter = HttpServerModule->GetHttpRouter(Port);
	if (!HttpRouter.IsValid())
	{
		FN2CLogger::Get().LogError(FString::Printf(TEXT("Failed to create HTTP router for port %d"), Port));
		return false;
	}

	// Register MCP endpoint handler
	FHttpRequestHandler McpHandler;
	McpHandler.BindLambda([this](const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete) -> bool
	{
		return HandleMcpRequest(Request, OnComplete);
	});
	HttpRouter->BindRoute(FHttpPath(TEXT("/mcp")), EHttpServerRequestVerbs::VERB_POST, McpHandler);

	// Register health check endpoint
	FHttpRequestHandler HealthHandler;
	HealthHandler.BindLambda([this](const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete) -> bool
	{
		return HandleHealthRequest(Request, OnComplete);
	});
	HttpRouter->BindRoute(FHttpPath(TEXT("/mcp/health")), EHttpServerRequestVerbs::VERB_GET, HealthHandler);

	// Start listening
	HttpServerModule->StartAllListeners();

	bIsServerRunning = true;
	ServerPort = Port;

	FN2CLogger::Get().Log(FString::Printf(TEXT("MCP HTTP server started successfully on localhost:%d"), Port), EN2CLogSeverity::Info);
	FN2CLogger::Get().Log(FString::Printf(TEXT("MCP endpoint available at: http://localhost:%d/mcp"), Port), EN2CLogSeverity::Info);
	FN2CLogger::Get().Log(FString::Printf(TEXT("Health check available at: http://localhost:%d/mcp/health"), Port), EN2CLogSeverity::Info);

	// Register tools
	RegisterMcpTools();

	return true;
}

void FN2CMcpHttpServerManager::StopServer()
{
	if (!bIsServerRunning)
	{
		return;
	}

	if (HttpServerModule)
	{
		HttpServerModule->StopAllListeners();
		HttpRouter.Reset();
	}

	bIsServerRunning = false;
	ServerPort = -1;

	FN2CLogger::Get().Log(TEXT("MCP HTTP server stopped"), EN2CLogSeverity::Info);
}

bool FN2CMcpHttpServerManager::HandleMcpRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	// Get request body
	FString RequestBody = FString(UTF8_TO_TCHAR(Request.Body.GetData()));

	// Process the MCP request
	FString ResponseBody;
	int32 StatusCode = 200;

	FN2CMcpHttpRequestHandler::ProcessMcpRequest(RequestBody, ResponseBody, StatusCode);

	// Create response
	TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(ResponseBody, TEXT("application/json"));
	Response->Code = static_cast<EHttpServerResponseCodes>(StatusCode);

	// Add CORS headers for local development
	Response->Headers.Add(TEXT("Access-Control-Allow-Origin"), { TEXT("*") });
	Response->Headers.Add(TEXT("Access-Control-Allow-Methods"), { TEXT("POST, GET, OPTIONS") });
	Response->Headers.Add(TEXT("Access-Control-Allow-Headers"), { TEXT("Content-Type, Mcp-Session-Id") });

	// Send response
	OnComplete(MoveTemp(Response));

	return true;
}

bool FN2CMcpHttpServerManager::HandleHealthRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	// Create simple health check response
	FString HealthResponse = TEXT("{\"status\":\"ok\",\"service\":\"NodeToCode MCP Server\"}");
	
	TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(HealthResponse, TEXT("application/json"));
	Response->Code = EHttpServerResponseCodes::Ok;

	// Add CORS headers
	Response->Headers.Add(TEXT("Access-Control-Allow-Origin"), { TEXT("*") });

	OnComplete(MoveTemp(Response));

	FN2CLogger::Get().Log(TEXT("Health check request processed"), EN2CLogSeverity::Debug);

	return true;
}

void FN2CMcpHttpServerManager::RegisterMcpTools()
{
	FN2CLogger::Get().Log(TEXT("Registering NodeToCode MCP tools"), EN2CLogSeverity::Info);

	// Register get-focused-blueprint tool
	{
		FMcpToolDefinition GetFocusedBlueprintTool(
			TEXT("get-focused-blueprint"), 
			TEXT("Collects and serializes the currently focused Blueprint graph in the Unreal Editor into NodeToCode's N2CJSON format.")
		);
		
		// Create input schema - no input arguments needed
		TSharedPtr<FJsonObject> InputSchema = MakeShareable(new FJsonObject);
		InputSchema->SetStringField(TEXT("type"), TEXT("object"));
		
		// Empty properties object since no input arguments are required
		TSharedPtr<FJsonObject> Properties = MakeShareable(new FJsonObject);
		InputSchema->SetObjectField(TEXT("properties"), Properties);
		
		// No required fields (empty array)
		TArray<TSharedPtr<FJsonValue>> RequiredArray;
		InputSchema->SetArrayField(TEXT("required"), RequiredArray);
		
		GetFocusedBlueprintTool.InputSchema = InputSchema;
		
		// Set read-only annotation
		TSharedPtr<FJsonObject> Annotations = MakeShareable(new FJsonObject);
		Annotations->SetBoolField(TEXT("readOnlyHint"), true);
		GetFocusedBlueprintTool.Annotations = Annotations;
		
		// Create handler that executes on the Game Thread
		FMcpToolHandlerDelegate GetFocusedBlueprintHandler;
		GetFocusedBlueprintHandler.BindLambda([](const TSharedPtr<FJsonObject>& Args) -> FMcpToolCallResult
		{
			// Check if we're already on the Game Thread
			if (IsInGameThread())
			{
				FN2CLogger::Get().Log(TEXT("get-focused-blueprint: Already on Game Thread, executing directly"), EN2CLogSeverity::Debug);
				
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
			}
			
			// We're on a worker thread, need to dispatch to Game Thread
			FN2CLogger::Get().Log(TEXT("get-focused-blueprint: On worker thread, dispatching to Game Thread"), EN2CLogSeverity::Debug);
			
			// Use a simple event for synchronization
			FEvent* TaskEvent = FPlatformProcess::GetSynchEventFromPool();
			FString ResultJson;
			FString ResultError;
			
			// Create a task to run on the Game Thread
			FGraphEventRef Task = FFunctionGraphTask::CreateAndDispatchWhenReady([&ResultJson, &ResultError, TaskEvent]()
			{
				FN2CLogger::Get().Log(TEXT("get-focused-blueprint: Game Thread task executing"), EN2CLogSeverity::Debug);
				
				FString ErrorMsg;
				FString JsonOutput = FN2CEditorIntegration::Get().GetFocusedBlueprintAsJson(false /* no pretty print */, ErrorMsg);
				
				ResultJson = JsonOutput;
				ResultError = ErrorMsg;
				
				FN2CLogger::Get().Log(FString::Printf(TEXT("get-focused-blueprint: Game Thread task completed. Success: %s"), 
					JsonOutput.IsEmpty() ? TEXT("No") : TEXT("Yes")), EN2CLogSeverity::Debug);
				
				// Signal completion
				TaskEvent->Trigger();
			}, TStatId(), nullptr, ENamedThreads::GameThread);
			
			// Wait for completion with timeout
			const uint32 TimeoutMs = 10000; // 10 seconds timeout
			bool bCompleted = TaskEvent->Wait(TimeoutMs);
			
			// Return event to pool
			FPlatformProcess::ReturnSynchEventToPool(TaskEvent);
			
			if (bCompleted)
			{
				if (!ResultJson.IsEmpty())
				{
					FN2CLogger::Get().Log(TEXT("get-focused-blueprint tool successfully retrieved Blueprint JSON"), EN2CLogSeverity::Info);
					return FMcpToolCallResult::CreateTextResult(ResultJson);
				}
				else
				{
					FN2CLogger::Get().LogWarning(FString::Printf(TEXT("get-focused-blueprint tool failed: %s"), *ResultError));
					return FMcpToolCallResult::CreateErrorResult(ResultError);
				}
			}
			else
			{
				FN2CLogger::Get().LogError(TEXT("get-focused-blueprint tool timed out waiting for Game Thread"));
				return FMcpToolCallResult::CreateErrorResult(TEXT("Timeout waiting for Blueprint processing on Game Thread."));
			}
		});
		
		FN2CMcpToolManager::Get().RegisterTool(GetFocusedBlueprintTool, GetFocusedBlueprintHandler);
	}

	FN2CLogger::Get().Log(TEXT("MCP tools registered successfully"), EN2CLogSeverity::Info);
}