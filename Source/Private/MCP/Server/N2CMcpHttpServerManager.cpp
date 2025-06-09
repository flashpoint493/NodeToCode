// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#include "MCP/Server/N2CMcpHttpServerManager.h"
#include "MCP/Server/N2CMcpHttpRequestHandler.h"
#include "Utils/N2CLogger.h"
#include "HttpServerModule.h"
#include "HttpServerResponse.h"
#include "HttpPath.h"
#include "MCP/Tools/N2CMcpToolManager.h"
#include "MCP/Tools/N2CMcpToolRegistry.h"
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
	// Get request body using a specified length to avoid extraneous characters
	FString RequestBody;
	if (Request.Body.Num() > 0)
	{
		// Use FUTF8ToTCHAR to correctly convert the specified number of bytes from the request body.
		// We'll use the full Body.Num() for now, as we don't have direct access to ContentLength
		int32 LengthToConvert = Request.Body.Num();
		
		// First, find the actual JSON content length by looking for the end of valid JSON
		// This is necessary because Request.Body might contain extra bytes
		const char* BodyData = reinterpret_cast<const char*>(Request.Body.GetData());
		int32 ActualLength = 0;
		int32 BraceDepth = 0;
		bool InString = false;
		bool Escaped = false;
		
		for (int32 i = 0; i < Request.Body.Num(); ++i)
		{
			char c = BodyData[i];
			
			if (!InString)
			{
				if (c == '{') BraceDepth++;
				else if (c == '}') 
				{
					BraceDepth--;
					if (BraceDepth == 0)
					{
						ActualLength = i + 1; // Include the closing brace
						break;
					}
				}
				else if (c == '"' && !Escaped) InString = true;
			}
			else
			{
				if (c == '"' && !Escaped) InString = false;
			}
			
			Escaped = (c == '\\' && !Escaped);
		}
		
		// If we found valid JSON, use that length; otherwise fall back to full buffer
		if (ActualLength > 0)
		{
			LengthToConvert = ActualLength;
		}
		
		FUTF8ToTCHAR Converter(reinterpret_cast<const ANSICHAR*>(Request.Body.GetData()), LengthToConvert);
		RequestBody = FString(Converter.Length(), Converter.Get());
	}

	// Log the received body for debugging - essential for diagnosing these kinds of issues.
	// Limit log length to avoid flooding with very large requests.
	FN2CLogger::Get().Log(FString::Printf(TEXT("MCP HTTP Request Body (Body.Num: %d, First 500 chars): '%s'"),
		Request.Body.Num(), *RequestBody.Left(500)), EN2CLogSeverity::Debug);

	// Process the MCP request
	FString ResponseBody;
	int32 StatusCode = 200;

	// Variables to track if the method was 'initialize' and handle session management
	bool bWasInitializeCall = false;
	FString ClientSentSessionId;
	const TArray<FString>* SessionIdHeaders = Request.Headers.Find(TEXT("Mcp-Session-Id"));
	if (SessionIdHeaders && SessionIdHeaders->Num() > 0)
	{
		ClientSentSessionId = (*SessionIdHeaders)[0];
	}

	// Process the request
	FN2CMcpHttpRequestHandler::ProcessMcpRequest(RequestBody, ResponseBody, StatusCode, bWasInitializeCall);

	// Create response
	TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(ResponseBody, TEXT("application/json"));
	if (!Response.IsValid())
	{
		FN2CLogger::Get().LogError(TEXT("Failed to create FHttpServerResponse object in HandleMcpRequest."));
		return false;
	}
	Response->Code = static_cast<EHttpServerResponseCodes>(StatusCode);

	// Add CORS headers for local development
	Response->Headers.Add(TEXT("Access-Control-Allow-Origin"), { TEXT("*") });
	Response->Headers.Add(TEXT("Access-Control-Allow-Methods"), { TEXT("POST, GET, OPTIONS") });
	Response->Headers.Add(TEXT("Access-Control-Allow-Headers"), { TEXT("Content-Type, Mcp-Session-Id") });
	Response->Headers.Add(TEXT("Access-Control-Expose-Headers"), { TEXT("Mcp-Session-Id") }); // Important for client to read it

	// Session ID Handling for HTTP Transport (as per MCP Spec 2025-03-26)
	if (bWasInitializeCall && StatusCode == 200)
	{
		// Successful initialize - generate or reuse session ID
		if (CurrentSessionId.IsEmpty())
		{
			CurrentSessionId = FGuid::NewGuid().ToString();
		}
		Response->Headers.Add(TEXT("Mcp-Session-Id"), { CurrentSessionId });
		FN2CLogger::Get().Log(FString::Printf(TEXT("MCP Initialize successful. Responding with Mcp-Session-Id: %s"), *CurrentSessionId), EN2CLogSeverity::Info);
	}
	else if (!ClientSentSessionId.IsEmpty())
	{
		// For non-initialize requests, if client sent a session ID, echo it back if it matches our current one
		if (ClientSentSessionId == CurrentSessionId || CurrentSessionId.IsEmpty())
		{
			Response->Headers.Add(TEXT("Mcp-Session-Id"), { ClientSentSessionId });
		}
		else
		{
			FN2CLogger::Get().LogWarning(FString::Printf(TEXT("Mcp-Session-Id mismatch. Client: %s, Server: %s. Not adding Mcp-Session-Id to response."), *ClientSentSessionId, *CurrentSessionId));
		}
	}
	else if (!CurrentSessionId.IsEmpty() && !bWasInitializeCall)
	{
		// Client did not send session ID for an ongoing session
		Response->Headers.Add(TEXT("Mcp-Session-Id"), { CurrentSessionId });
		FN2CLogger::Get().LogWarning(TEXT("Client did not send Mcp-Session-Id for an ongoing session. Responded with current session ID."));
	}

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
	
	// Register all tools via the registry
	FN2CMcpToolRegistry::Get().RegisterAllToolsWithManager();
	
	FN2CLogger::Get().Log(TEXT("MCP tools registered successfully"), EN2CLogSeverity::Info);
}