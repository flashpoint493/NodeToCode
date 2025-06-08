// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#include "MCP/Server/N2CMcpHttpServerManager.h"
#include "MCP/Server/N2CMcpHttpRequestHandler.h"
#include "Utils/N2CLogger.h"
#include "HttpServerModule.h"
#include "HttpServerResponse.h"
#include "HttpPath.h"
#include "MCP/Tools/N2CMcpToolManager.h"
#include "Core/N2CSettings.h"

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
	FN2CLogger::Get().Log(TEXT("Registering MCP tools for testing"), EN2CLogSeverity::Info);

	// Register dummy echo tool for testing
	{
		FMcpToolDefinition EchoTool(TEXT("echo-test"), TEXT("A simple echo tool for testing MCP functionality"));
		
		// Create input schema
		TSharedPtr<FJsonObject> InputSchema = MakeShareable(new FJsonObject);
		InputSchema->SetStringField(TEXT("type"), TEXT("object"));
		
		TSharedPtr<FJsonObject> Properties = MakeShareable(new FJsonObject);
		TSharedPtr<FJsonObject> MessageProp = MakeShareable(new FJsonObject);
		MessageProp->SetStringField(TEXT("type"), TEXT("string"));
		MessageProp->SetStringField(TEXT("description"), TEXT("The message to echo back"));
		Properties->SetObjectField(TEXT("message"), MessageProp);
		
		InputSchema->SetObjectField(TEXT("properties"), Properties);
		
		EchoTool.InputSchema = InputSchema;

		// Create handler
		FMcpToolHandlerDelegate EchoHandler;
		EchoHandler.BindLambda([](const TSharedPtr<FJsonObject>& Args) -> FMcpToolCallResult
		{
			FString Message = TEXT("Hello from echo tool!");
			if (Args.IsValid())
			{
				Args->TryGetStringField(TEXT("message"), Message);
			}

			return FMcpToolCallResult::CreateTextResult(FString::Printf(TEXT("Echo: %s"), *Message));
		});

		FN2CMcpToolManager::Get().RegisterTool(EchoTool, EchoHandler);
	}

	// Register dummy math tool for testing
	{
		FMcpToolDefinition MathTool(TEXT("simple-math"), TEXT("Performs simple addition of two numbers"));
		
		// Create input schema
		TSharedPtr<FJsonObject> InputSchema = MakeShareable(new FJsonObject);
		InputSchema->SetStringField(TEXT("type"), TEXT("object"));
		
		TSharedPtr<FJsonObject> Properties = MakeShareable(new FJsonObject);
		
		TSharedPtr<FJsonObject> AProp = MakeShareable(new FJsonObject);
		AProp->SetStringField(TEXT("type"), TEXT("number"));
		AProp->SetStringField(TEXT("description"), TEXT("First number"));
		Properties->SetObjectField(TEXT("a"), AProp);
		
		TSharedPtr<FJsonObject> BProp = MakeShareable(new FJsonObject);
		BProp->SetStringField(TEXT("type"), TEXT("number"));
		BProp->SetStringField(TEXT("description"), TEXT("Second number"));
		Properties->SetObjectField(TEXT("b"), BProp);
		
		InputSchema->SetObjectField(TEXT("properties"), Properties);
		
		// Required fields
		TArray<TSharedPtr<FJsonValue>> RequiredArray;
		RequiredArray.Add(MakeShareable(new FJsonValueString(TEXT("a"))));
		RequiredArray.Add(MakeShareable(new FJsonValueString(TEXT("b"))));
		InputSchema->SetArrayField(TEXT("required"), RequiredArray);
		
		MathTool.InputSchema = InputSchema;

		// Set read-only annotation
		TSharedPtr<FJsonObject> Annotations = MakeShareable(new FJsonObject);
		Annotations->SetBoolField(TEXT("readOnlyHint"), true);
		MathTool.Annotations = Annotations;

		// Create handler
		FMcpToolHandlerDelegate MathHandler;
		MathHandler.BindLambda([](const TSharedPtr<FJsonObject>& Args) -> FMcpToolCallResult
		{
			if (!Args.IsValid())
			{
				return FMcpToolCallResult::CreateErrorResult(TEXT("Arguments required"));
			}

			double A = 0, B = 0;
			if (!Args->TryGetNumberField(TEXT("a"), A) || !Args->TryGetNumberField(TEXT("b"), B))
			{
				return FMcpToolCallResult::CreateErrorResult(TEXT("Both 'a' and 'b' must be numbers"));
			}

			double Result = A + B;
			return FMcpToolCallResult::CreateTextResult(FString::Printf(TEXT("Result: %f + %f = %f"), A, B, Result));
		});

		FN2CMcpToolManager::Get().RegisterTool(MathTool, MathHandler);
	}

	FN2CLogger::Get().Log(TEXT("MCP tools registered successfully"), EN2CLogSeverity::Info);
}