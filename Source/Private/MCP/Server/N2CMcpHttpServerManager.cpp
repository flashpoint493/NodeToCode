// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#include "MCP/Server/N2CMcpHttpServerManager.h"
#include "MCP/Server/N2CMcpHttpRequestHandler.h"
#include "MCP/Server/N2CSseServer.h"
#include "Utils/N2CLogger.h"
#include "HttpServerModule.h"
#include "HttpServerResponse.h"
#include "HttpPath.h"
#include "MCP/Tools/N2CMcpToolManager.h"
#include "MCP/Tools/N2CMcpToolRegistry.h"
#include "MCP/Resources/N2CMcpResourceManager.h"
#include "MCP/Resources/Implementations/N2CMcpBlueprintResource.h"
#include "MCP/Prompts/N2CMcpPromptManager.h"
#include "MCP/Prompts/Implementations/N2CMcpCodeGenerationPrompt.h"
#include "Core/N2CSettings.h"
#include "MCP/Server/N2CMcpSSEResponseManager.h"
#include "MCP/Async/N2CToolAsyncTaskManager.h"

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
	
	// Register resources
	RegisterMcpResources();
	
	// Register prompts
	RegisterMcpPrompts();

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

	// Check if this is a tools/call request for a long-running tool
	bool bIsLongRunningTool = false;
	FString ToolName;
	TSharedPtr<FJsonObject> ToolArguments;
	TSharedPtr<FJsonValue> RequestId; // This is the JSON-RPC request ID
	FString ProgressToken;           // This is the MCP progress token for SSE
	
	// Parse the request to check if it's a tools/call
	TSharedPtr<FJsonValue> JsonValue;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RequestBody);
	if (FJsonSerializer::Deserialize(Reader, JsonValue) && JsonValue.IsValid() && JsonValue->Type == EJson::Object)
	{
		TSharedPtr<FJsonObject> RequestObject = JsonValue->AsObject();
		FString Method;
		if (RequestObject->TryGetStringField(TEXT("method"), Method) && Method == TEXT("tools/call"))
		{
			// Extract the tool name and check if it's long-running
			const TSharedPtr<FJsonObject>* ParamsObj = nullptr;
			if (RequestObject->TryGetObjectField(TEXT("params"), ParamsObj) && ParamsObj->IsValid())
			{
				(*ParamsObj)->TryGetStringField(TEXT("name"), ToolName);
				
				// Check if tool is long-running
				if (!ToolName.IsEmpty())
				{
					const FMcpToolDefinition* ToolDef = FN2CMcpToolManager::Get().GetToolDefinition(ToolName);
					if (ToolDef && ToolDef->bIsLongRunning)
					{
						bIsLongRunningTool = true;
						
						// Extract arguments
						const TSharedPtr<FJsonObject>* ArgsObj = nullptr;
						if ((*ParamsObj)->TryGetObjectField(TEXT("arguments"), ArgsObj) && ArgsObj->IsValid())
						{
							ToolArguments = *ArgsObj;
						}
						else
						{
							ToolArguments = MakeShareable(new FJsonObject);
						}
						
						// Extract request ID
						RequestId = RequestObject->TryGetField(TEXT("id"));
						if (!RequestId.IsValid())
						{
							RequestId = MakeShareable(new FJsonValueNull());
						}
						
						// Extract progress token from _meta
						const TSharedPtr<FJsonObject>* MetaObj = nullptr;
						if ((*ParamsObj)->TryGetObjectField(TEXT("_meta"), MetaObj) && MetaObj->IsValid())
						{
                            // PROGRESS TOKEN EXTRACTION
                            FString ClientProvidedToken;
							// Ensure _meta.progressToken is a string. If it's present but not a string, treat as invalid.
							if ((*MetaObj)->TryGetStringField(TEXT("progressToken"), ClientProvidedToken) && !ClientProvidedToken.IsEmpty()) {
                                ProgressToken = ClientProvidedToken;
                            } else {
                                if ((*MetaObj)->HasField(TEXT("progressToken"))) {
                                    FN2CLogger::Get().LogWarning(TEXT("Client provided _meta.progressToken but it was not a valid string. Will generate a new one."), TEXT("MCP"));
                                }
                                // ProgressToken remains empty, will be generated below if needed.
                            }
						}
					}
				}
			}
		}
	}
	
	// Handle long-running tools differently
    if (bIsLongRunningTool)
	{
        if (ProgressToken.IsEmpty())
        {
            ProgressToken = FGuid::NewGuid().ToString();
            FN2CLogger::Get().Log(FString::Printf(TEXT("Generated progressToken %s for long-running tool %s"), *ProgressToken, *ToolName), EN2CLogSeverity::Info);
        }

		// Launch the async task to get the task ID
		FGuid TaskId = FN2CToolAsyncTaskManager::Get().LaunchTask(
			ToolName, 
			ToolArguments, 
			ProgressToken, 
			CurrentSessionId,
			RequestId);
		
		if (TaskId.IsValid())
		{
			// Build the SSE URL
			int32 SsePort = NodeToCodeSseServer::GetSseServerPort();
			if (SsePort < 0)
			{
				FN2CLogger::Get().LogError(TEXT("SSE server is not running, cannot handle long-running tools"));
				FN2CToolAsyncTaskManager::Get().CancelTask(TaskId);
				
				ResponseBody = FJsonRpcUtils::SerializeResponse(
					FJsonRpcUtils::CreateErrorResponse(RequestId, JsonRpcErrorCodes::InternalError, 
						TEXT("SSE server is not available for long-running operations")
					)
				);
				StatusCode = 500;
			}
			else
			{
				// Create the SSE URL for this task
				FString SseUrl = FString::Printf(TEXT("http://localhost:%d/mcp/events/%s"), SsePort, *TaskId.ToString());
				
				// Return a synchronous response with the SSE URL
				TSharedPtr<FJsonObject> ResultObject = MakeShared<FJsonObject>();
				ResultObject->SetStringField(TEXT("status"), TEXT("accepted"));
				ResultObject->SetStringField(TEXT("taskId"), TaskId.ToString());
				ResultObject->SetStringField(TEXT("progressToken"), ProgressToken);
				ResultObject->SetStringField(TEXT("sseUrl"), SseUrl);
				
				FJsonRpcResponse Response(RequestId, MakeShared<FJsonValueObject>(ResultObject));
				ResponseBody = FJsonRpcUtils::SerializeResponse(Response);
				
				FN2CLogger::Get().Log(FString::Printf(TEXT("Launched async task %s for tool %s. SSE URL: %s"), 
					*TaskId.ToString(), *ToolName, *SseUrl), EN2CLogSeverity::Info);
			}
		}
		else
		{
			FN2CLogger::Get().LogError(FString::Printf(TEXT("Failed to launch async task for tool: %s"), *ToolName));
			ResponseBody = FJsonRpcUtils::SerializeResponse(
				FJsonRpcUtils::CreateErrorResponse(RequestId, JsonRpcErrorCodes::InternalError, 
					FString::Printf(TEXT("Failed to initiate asynchronous task execution for tool '%s'"), *ToolName)
				)
			);
			StatusCode = 500;
		}
	}
	else
	{
		// Process the request normally (synchronous)
		FN2CMcpHttpRequestHandler::ProcessMcpRequest(RequestBody, ResponseBody, StatusCode, bWasInitializeCall);
	}

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

void FN2CMcpHttpServerManager::RegisterMcpResources()
{
	FN2CLogger::Get().Log(TEXT("Registering NodeToCode MCP resources"), EN2CLogSeverity::Info);
	
	// Clear any existing resources first
	FN2CMcpResourceManager::Get().ClearAllResources();
	
	// Register current blueprint resource
	{
		FMcpResourceDefinition CurrentBlueprintDef;
		CurrentBlueprintDef.Uri = TEXT("nodetocode://blueprint/current");
		CurrentBlueprintDef.Name = TEXT("Current Blueprint");
		CurrentBlueprintDef.Description = TEXT("The currently focused Blueprint in N2CJSON format");
		CurrentBlueprintDef.MimeType = TEXT("application/json");
		
		auto Handler = FMcpResourceReadDelegate::CreateLambda([](const FString& Uri) -> FMcpResourceContents
		{
			FN2CMcpCurrentBlueprintResource Resource;
			return Resource.Read(Uri);
		});
		
		FN2CMcpResourceManager::Get().RegisterStaticResource(CurrentBlueprintDef, Handler, true);
	}
	
	// Register all blueprints resource
	{
		FMcpResourceDefinition AllBlueprintsDef;
		AllBlueprintsDef.Uri = TEXT("nodetocode://blueprints/all");
		AllBlueprintsDef.Name = TEXT("All Open Blueprints");
		AllBlueprintsDef.Description = TEXT("List of all currently open Blueprints");
		AllBlueprintsDef.MimeType = TEXT("application/json");
		
		auto Handler = FMcpResourceReadDelegate::CreateLambda([](const FString& Uri) -> FMcpResourceContents
		{
			FN2CMcpAllBlueprintsResource Resource;
			return Resource.Read(Uri);
		});
		
		FN2CMcpResourceManager::Get().RegisterStaticResource(AllBlueprintsDef, Handler, true);
	}
	
	// Register blueprint by name template
	{
		FMcpResourceTemplate BlueprintByNameTemplate = FN2CMcpBlueprintByNameResource::GetResourceTemplate();
		
		auto Handler = FMcpResourceTemplateHandler::CreateLambda([](const FString& Uri) -> FMcpResourceContents
		{
			FN2CMcpBlueprintByNameResource Resource;
			return Resource.Read(Uri);
		});
		
		FN2CMcpResourceManager::Get().RegisterDynamicResource(BlueprintByNameTemplate, Handler, true);
	}
	
	FN2CLogger::Get().Log(TEXT("MCP resources registered successfully"), EN2CLogSeverity::Info);
}

void FN2CMcpHttpServerManager::RegisterMcpPrompts()
{
	FN2CLogger::Get().Log(TEXT("Registering NodeToCode MCP prompts"), EN2CLogSeverity::Info);
	
	// Clear any existing prompts first
	FN2CMcpPromptManager::Get().ClearAllPrompts();
	
	// Register generate-code prompt
	{
		FN2CMcpCodeGenerationPrompt TempPrompt;
		FMcpPromptDefinition CodeGenDef = TempPrompt.GetDefinition();
		
		auto Handler = FMcpPromptGetDelegate::CreateLambda([](const FMcpPromptArguments& Arguments) -> FMcpPromptResult
		{
			FN2CMcpCodeGenerationPrompt Prompt;
			return Prompt.GetPrompt(Arguments);
		});
		
		FN2CMcpPromptManager::Get().RegisterPrompt(CodeGenDef, Handler, true);
	}
	
	// Register analyze-blueprint prompt
	{
		FN2CMcpBlueprintAnalysisPrompt TempPrompt;
		FMcpPromptDefinition AnalysisDef = TempPrompt.GetDefinition();
		
		auto Handler = FMcpPromptGetDelegate::CreateLambda([](const FMcpPromptArguments& Arguments) -> FMcpPromptResult
		{
			FN2CMcpBlueprintAnalysisPrompt Prompt;
			return Prompt.GetPrompt(Arguments);
		});
		
		FN2CMcpPromptManager::Get().RegisterPrompt(AnalysisDef, Handler, true);
	}
	
	// Register refactor-blueprint prompt
	{
		FN2CMcpRefactorPrompt TempPrompt;
		FMcpPromptDefinition RefactorDef = TempPrompt.GetDefinition();
		
		auto Handler = FMcpPromptGetDelegate::CreateLambda([](const FMcpPromptArguments& Arguments) -> FMcpPromptResult
		{
			FN2CMcpRefactorPrompt Prompt;
			return Prompt.GetPrompt(Arguments);
		});
		
		FN2CMcpPromptManager::Get().RegisterPrompt(RefactorDef, Handler, true);
	}
	
	FN2CLogger::Get().Log(TEXT("MCP prompts registered successfully"), EN2CLogSeverity::Info);
}

void FN2CMcpHttpServerManager::RegisterClient(const FString& SessionId, TSharedPtr<IN2CMcpNotificationChannel> Channel)
{
	if (!Channel.IsValid())
	{
		FN2CLogger::Get().LogWarning(TEXT("Cannot register null notification channel"));
		return;
	}

	FScopeLock Lock(&ClientChannelLock);
	
	// Unregister any existing channel for this session
	if (ClientChannels.Contains(SessionId))
	{
		FN2CLogger::Get().Log(FString::Printf(TEXT("Replacing existing notification channel for session: %s"), *SessionId), EN2CLogSeverity::Debug);
	}
	
	ClientChannels.Add(SessionId, Channel);
	
	FN2CLogger::Get().Log(FString::Printf(TEXT("Registered notification channel for session: %s"), *SessionId), EN2CLogSeverity::Info);
}

void FN2CMcpHttpServerManager::UnregisterClient(const FString& SessionId)
{
	FScopeLock Lock(&ClientChannelLock);
	
	if (ClientChannels.Remove(SessionId) > 0)
	{
		FN2CLogger::Get().Log(FString::Printf(TEXT("Unregistered notification channel for session: %s"), *SessionId), EN2CLogSeverity::Info);
	}
	
	// Also remove protocol version info
	SessionProtocolVersions.Remove(SessionId);
}

void FN2CMcpHttpServerManager::BroadcastNotification(const FJsonRpcNotification& Notification)
{
	FScopeLock Lock(&ClientChannelLock);
	
	TArray<FString> InactiveChannels;
	
	// Send to all registered clients
	for (const auto& Pair : ClientChannels)
	{
		const FString& SessionId = Pair.Key;
		const TSharedPtr<IN2CMcpNotificationChannel>& Channel = Pair.Value;
		
		if (Channel.IsValid() && Channel->IsActive())
		{
			if (!Channel->SendNotification(Notification))
			{
				FN2CLogger::Get().LogWarning(FString::Printf(TEXT("Failed to send notification to session: %s"), *SessionId));
			}
		}
		else
		{
			// Mark inactive channels for removal
			InactiveChannels.Add(SessionId);
		}
	}
	
	// Clean up inactive channels
	for (const FString& SessionId : InactiveChannels)
	{
		ClientChannels.Remove(SessionId);
		SessionProtocolVersions.Remove(SessionId);
		FN2CLogger::Get().Log(FString::Printf(TEXT("Removed inactive notification channel for session: %s"), *SessionId), EN2CLogSeverity::Debug);
	}
	
	FN2CLogger::Get().Log(FString::Printf(TEXT("Broadcast notification '%s' to %d active clients"), 
		*Notification.Method, ClientChannels.Num() - InactiveChannels.Num()), EN2CLogSeverity::Debug);
}

void FN2CMcpHttpServerManager::SendNotificationToClient(const FString& SessionId, const FJsonRpcNotification& Notification)
{
	FScopeLock Lock(&ClientChannelLock);
	
	TSharedPtr<IN2CMcpNotificationChannel>* ChannelPtr = ClientChannels.Find(SessionId);
	if (!ChannelPtr || !ChannelPtr->IsValid())
	{
		FN2CLogger::Get().LogWarning(FString::Printf(TEXT("No active notification channel for session: %s"), *SessionId));
		return;
	}
	
	TSharedPtr<IN2CMcpNotificationChannel>& Channel = *ChannelPtr;
	if (Channel->IsActive())
	{
		if (!Channel->SendNotification(Notification))
		{
			FN2CLogger::Get().LogWarning(FString::Printf(TEXT("Failed to send notification to session: %s"), *SessionId));
		}
		else
		{
			FN2CLogger::Get().Log(FString::Printf(TEXT("Sent notification '%s' to session: %s"), 
				*Notification.Method, *SessionId), EN2CLogSeverity::Debug);
		}
	}
	else
	{
		// Remove inactive channel
		ClientChannels.Remove(SessionId);
		SessionProtocolVersions.Remove(SessionId);
		FN2CLogger::Get().Log(FString::Printf(TEXT("Removed inactive notification channel for session: %s"), *SessionId), EN2CLogSeverity::Debug);
	}
}

void FN2CMcpHttpServerManager::SetSessionProtocolVersion(const FString& SessionId, const FString& ProtocolVersion)
{
	FScopeLock Lock(&ClientChannelLock);
	SessionProtocolVersions.Add(SessionId, ProtocolVersion);
	
	FN2CLogger::Get().Log(FString::Printf(TEXT("Set protocol version '%s' for session: %s"), 
		*ProtocolVersion, *SessionId), EN2CLogSeverity::Debug);
}

void FN2CMcpHttpServerManager::SendAsyncTaskProgress(const FString& SessionId, const FJsonRpcNotification& ProgressNotification)
{
	// Find the SSE connection for this progress notification
	if (ProgressNotification.Params.IsValid() && ProgressNotification.Params->Type == EJson::Object)
	{
		TSharedPtr<FJsonObject> ParamsObj = ProgressNotification.Params->AsObject();
		FString ProgressToken;
		if (ParamsObj->TryGetStringField(TEXT("progressToken"), ProgressToken))
		{
			FString ConnectionId = FN2CMcpSSEResponseManager::Get().FindConnectionByProgressToken(ProgressToken);
			if (!ConnectionId.IsEmpty())
			{
				FN2CMcpSSEResponseManager::Get().SendProgressNotification(ConnectionId, ProgressNotification);
			}
			else
			{
				FN2CLogger::Get().LogWarning(FString::Printf(TEXT("No SSE connection found for progress token: %s"), *ProgressToken));
			}
		}
	}
}

void FN2CMcpHttpServerManager::SendAsyncTaskResponse(const FGuid& TaskId, const TSharedPtr<FJsonValue>& OriginalRequestId, const FMcpToolCallResult& Result)
{
	// Find the SSE connection using the TaskId
	FString ConnectionId = FN2CMcpSSEResponseManager::Get().FindConnectionByTaskId(TaskId);
	if (ConnectionId.IsEmpty())
	{
		FN2CLogger::Get().LogError(FString::Printf(TEXT("No SSE connection found for task: %s"), *TaskId.ToString()));
		return;
	}

	// Create the JSON-RPC response
	FJsonRpcResponse Response;
	Response.Id = OriginalRequestId;
	
	if (Result.bIsError)
	{
		// Create error response
		FString ErrorMessage = TEXT("Tool execution failed");
		
		// Extract error message from the result content
		if (Result.Content.Num() > 0 && Result.Content[0].IsValid())
		{
			Result.Content[0]->TryGetStringField(TEXT("text"), ErrorMessage);
		}
		
		// Create error object
		FJsonRpcError ErrorObj(-32603, ErrorMessage);
		Response.Error = ErrorObj.ToJson();
	}
	else
	{
		// Create success response with the tool result
		Response.Result = MakeShared<FJsonValueObject>(Result.ToJson());
	}

	// Send the final response and close the SSE connection
	FN2CMcpSSEResponseManager::Get().SendFinalResponse(ConnectionId, Response);
}
