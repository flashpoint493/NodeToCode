// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#include "MCP/Server/N2CMcpHttpRequestHandler.h"
#include "Utils/N2CLogger.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Interfaces/IPluginManager.h"
#include "MCP/Tools/N2CMcpToolManager.h"

bool FN2CMcpHttpRequestHandler::ProcessMcpRequest(const FString& RequestBody, FString& OutResponseBody, int32& OutStatusCode)
{
	// Default to success
	OutStatusCode = 200;

	// Parse the request body as JSON
	TSharedPtr<FJsonValue> JsonValue;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RequestBody);

	if (!FJsonSerializer::Deserialize(Reader, JsonValue) || !JsonValue.IsValid())
	{
		FN2CLogger::Get().LogWarning(FString::Printf(TEXT("Failed to parse JSON-RPC request: %s"), *RequestBody));
		
		// Create parse error response
		FJsonRpcResponse ErrorResponse = FJsonRpcUtils::CreateErrorResponse(nullptr, JsonRpcErrorCodes::ParseError, TEXT("Parse error"));
		OutResponseBody = FJsonRpcUtils::SerializeResponse(ErrorResponse);
		
		OutStatusCode = 400; // Bad Request
		return false;
	}

	// Handle batch requests (array) or single requests (object)
	if (JsonValue->Type == EJson::Array)
	{
		// Batch request - not implemented for MVP
		FN2CLogger::Get().LogWarning(TEXT("Batch JSON-RPC requests not supported in MVP"));
		
		FJsonRpcResponse ErrorResponse = FJsonRpcUtils::CreateErrorResponse(nullptr, JsonRpcErrorCodes::InternalError, TEXT("Batch requests not supported"));
		OutResponseBody = FJsonRpcUtils::SerializeResponse(ErrorResponse);
		
		OutStatusCode = 501; // Not Implemented
		return false;
	}
	else if (JsonValue->Type == EJson::Object)
	{
		// Single request
		TSharedPtr<FJsonObject> RequestObject = JsonValue->AsObject();
		
		// Determine message type
		FJsonRpcUtils::EJsonRpcMessageType MessageType = FJsonRpcUtils::GetMessageType(RequestObject);
		
		// Log the message type for debugging
		FN2CLogger::Get().Log(FString::Printf(TEXT("Message type detected: %d"), (int32)MessageType), EN2CLogSeverity::Debug);
		
		if (MessageType == FJsonRpcUtils::EJsonRpcMessageType::Request)
		{
			FJsonRpcRequest Request(RequestObject);
			FJsonRpcResponse Response;
			
			if (ProcessRequest(Request, Response))
			{
				OutResponseBody = FJsonRpcUtils::SerializeResponse(Response);
				return true;
			}
			else
			{
				OutStatusCode = 500; // Internal Server Error
				return false;
			}
		}
		else if (MessageType == FJsonRpcUtils::EJsonRpcMessageType::Notification)
		{
			FJsonRpcNotification Notification(RequestObject);
			
			if (ProcessNotification(Notification))
			{
				// Notification - no response body
				OutResponseBody = TEXT("");
				OutStatusCode = 202; // Accepted
				return true;
			}
			else
			{
				OutStatusCode = 500; // Internal Server Error
				return false;
			}
		}
		else
		{
			// Invalid JSON-RPC message
			FN2CLogger::Get().LogWarning(TEXT("Invalid JSON-RPC message format"));
			
			FJsonRpcResponse ErrorResponse = FJsonRpcUtils::CreateErrorResponse(nullptr, JsonRpcErrorCodes::InvalidRequest, TEXT("Invalid Request"));
			OutResponseBody = FJsonRpcUtils::SerializeResponse(ErrorResponse);
			
			OutStatusCode = 400; // Bad Request
			return false;
		}
	}
	else
	{
		// Invalid JSON structure
		FN2CLogger::Get().LogWarning(TEXT("Invalid JSON-RPC request structure"));
		
		FJsonRpcResponse ErrorResponse = FJsonRpcUtils::CreateErrorResponse(nullptr, JsonRpcErrorCodes::InvalidRequest, TEXT("Invalid Request"));
		OutResponseBody = FJsonRpcUtils::SerializeResponse(ErrorResponse);
		
		OutStatusCode = 400; // Bad Request
		return false;
	}
}

bool FN2CMcpHttpRequestHandler::ParseJsonRpcMessage(const FString& JsonString, TSharedPtr<FJsonObject>& OutJsonObject)
{
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	TSharedPtr<FJsonValue> JsonValue;

	if (FJsonSerializer::Deserialize(Reader, JsonValue) && JsonValue.IsValid() && JsonValue->Type == EJson::Object)
	{
		OutJsonObject = JsonValue->AsObject();
		return true;
	}

	return false;
}

bool FN2CMcpHttpRequestHandler::ValidateJsonRpcMessage(const TSharedPtr<FJsonObject>& JsonObject)
{
	return FJsonRpcUtils::IsValidJsonRpcMessage(JsonObject);
}

bool FN2CMcpHttpRequestHandler::ProcessRequest(const FJsonRpcRequest& Request, FJsonRpcResponse& OutResponse)
{
	FN2CLogger::Get().Log(FString::Printf(TEXT("Processing JSON-RPC request: %s"), *Request.Method), EN2CLogSeverity::Debug);

	// Dispatch the method
	return DispatchMethod(Request.Method, Request.Params, Request.Id, OutResponse);
}

bool FN2CMcpHttpRequestHandler::ProcessNotification(const FJsonRpcNotification& Notification)
{
	FN2CLogger::Get().Log(FString::Printf(TEXT("Received JSON-RPC notification: %s"), *Notification.Method), EN2CLogSeverity::Info);
	
	// Handle specific notifications
	if (Notification.Method == TEXT("notifications/initialized"))
	{
		FN2CLogger::Get().Log(TEXT("MCP connection fully established - client sent initialized notification"), EN2CLogSeverity::Info);
		// TODO: Could trigger any post-initialization setup here if needed
		return true;
	}
	
	// For other notifications, just log them
	FN2CLogger::Get().Log(FString::Printf(TEXT("Unhandled notification: %s"), *Notification.Method), EN2CLogSeverity::Debug);
	return true;
}

bool FN2CMcpHttpRequestHandler::DispatchMethod(const FString& Method, const TSharedPtr<FJsonValue>& Params, const TSharedPtr<FJsonValue>& Id, FJsonRpcResponse& OutResponse)
{
	FN2CLogger::Get().Log(FString::Printf(TEXT("Dispatching JSON-RPC method: %s"), *Method), EN2CLogSeverity::Debug);

	// Handle method calls that expect responses
	if (Method == TEXT("initialize"))
	{
		return HandleInitialize(Params, Id, OutResponse);
	}
	else if (Method == TEXT("ping"))
	{
		// Simple ping response
		TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
		OutResponse = FJsonRpcUtils::CreateSuccessResponse(Id, MakeShareable(new FJsonValueObject(Result)));
		return true;
	}
	else if (Method == TEXT("tools/list"))
	{
		return HandleToolsList(Params, Id, OutResponse);
	}
	else if (Method == TEXT("tools/call"))
	{
		return HandleToolsCall(Params, Id, OutResponse);
	}
	else
	{
		// Method not found
		FN2CLogger::Get().LogWarning(FString::Printf(TEXT("JSON-RPC method not found: %s"), *Method));
		OutResponse = FJsonRpcUtils::CreateErrorResponse(Id, JsonRpcErrorCodes::MethodNotFound, FString::Printf(TEXT("Method '%s' not found"), *Method));
		return true;
	}
}

bool FN2CMcpHttpRequestHandler::HandleInitialize(const TSharedPtr<FJsonValue>& Params, const TSharedPtr<FJsonValue>& Id, FJsonRpcResponse& OutResponse)
{
	FN2CLogger::Get().Log(TEXT("Processing MCP initialize request"), EN2CLogSeverity::Info);

	// Parse InitializeClientRequestParams from the params
	if (!Params.IsValid() || Params->IsNull())
	{
		FN2CLogger::Get().LogWarning(TEXT("Initialize request missing or null params"));
		OutResponse = FJsonRpcUtils::CreateErrorResponse(Id, JsonRpcErrorCodes::InvalidParams, TEXT("Missing or null params for initialize"));
		return true;
	}
	
	if (Params->Type != EJson::Object)
	{
		FN2CLogger::Get().LogWarning(TEXT("Initialize request params is not an object"));
		OutResponse = FJsonRpcUtils::CreateErrorResponse(Id, JsonRpcErrorCodes::InvalidParams, TEXT("Params must be an object for initialize"));
		return true;
	}

	TSharedPtr<FJsonObject> ParamsObject = Params->AsObject();
	
	// Extract protocol version
	FString ClientProtocolVersion;
	if (!ParamsObject->TryGetStringField(TEXT("protocolVersion"), ClientProtocolVersion))
	{
		FN2CLogger::Get().LogWarning(TEXT("Initialize request missing protocolVersion"));
		OutResponse = FJsonRpcUtils::CreateErrorResponse(Id, JsonRpcErrorCodes::InvalidParams, TEXT("Missing required field: protocolVersion"));
		return true;
	}

	// Log client info if provided
	const TSharedPtr<FJsonObject>* ClientInfo = nullptr;
	if (ParamsObject->TryGetObjectField(TEXT("clientInfo"), ClientInfo) && ClientInfo->IsValid())
	{
		FString ClientName, ClientVersion;
		(*ClientInfo)->TryGetStringField(TEXT("name"), ClientName);
		(*ClientInfo)->TryGetStringField(TEXT("version"), ClientVersion);
		FN2CLogger::Get().Log(FString::Printf(TEXT("MCP Client: %s v%s"), *ClientName, *ClientVersion), EN2CLogSeverity::Info);
	}

	// Log client capabilities if provided
	const TSharedPtr<FJsonObject>* ClientCapabilities = nullptr;
	if (ParamsObject->TryGetObjectField(TEXT("capabilities"), ClientCapabilities) && ClientCapabilities->IsValid())
	{
		FN2CLogger::Get().Log(TEXT("Client capabilities received"), EN2CLogSeverity::Debug);
		// For MVP, we just acknowledge capabilities without specific handling
	}

	// Protocol version negotiation
	const FString SupportedProtocolVersion = TEXT("2025-03-26");
	FString NegotiatedProtocolVersion = SupportedProtocolVersion;

	// Check if we support the client's requested version
	if (ClientProtocolVersion != SupportedProtocolVersion)
	{
		FN2CLogger::Get().LogWarning(FString::Printf(TEXT("Client requested unsupported protocol version: %s. Server supports: %s"), 
			*ClientProtocolVersion, *SupportedProtocolVersion));
		
		// For MVP, we'll be strict about version matching
		OutResponse = FJsonRpcUtils::CreateErrorResponse(Id, JsonRpcErrorCodes::InvalidParams, 
			FString::Printf(TEXT("Unsupported protocol version: %s. Server supports: %s"), 
				*ClientProtocolVersion, *SupportedProtocolVersion));
		return true;
	}

	FN2CLogger::Get().Log(FString::Printf(TEXT("Protocol version negotiated: %s"), *NegotiatedProtocolVersion), EN2CLogSeverity::Info);

	// Build InitializeServerResult
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	
	// Set protocol version
	Result->SetStringField(TEXT("protocolVersion"), NegotiatedProtocolVersion);
	
	// Set server capabilities
	TSharedPtr<FJsonObject> ServerCapabilities = MakeShareable(new FJsonObject);
	
	// Tools capability
	TSharedPtr<FJsonObject> ToolsCapability = MakeShareable(new FJsonObject);
	ToolsCapability->SetBoolField(TEXT("listChanged"), true); // We support the tools/list_changed notification
	ServerCapabilities->SetObjectField(TEXT("tools"), ToolsCapability);
	
	// Logging capability (for future use)
	TSharedPtr<FJsonObject> LoggingCapability = MakeShareable(new FJsonObject);
	ServerCapabilities->SetObjectField(TEXT("logging"), LoggingCapability);
	
	Result->SetObjectField(TEXT("capabilities"), ServerCapabilities);
	
	// Set server info
	TSharedPtr<FJsonObject> ServerInfo = MakeShareable(new FJsonObject);
	ServerInfo->SetStringField(TEXT("name"), TEXT("NodeToCodeMCPServer"));
	
	// Get plugin version dynamically
	FString PluginVersion = TEXT("Unknown");
	if (TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("NodeToCode")))
	{
		PluginVersion = Plugin->GetDescriptor().VersionName;
	}
	ServerInfo->SetStringField(TEXT("version"), PluginVersion);
	Result->SetObjectField(TEXT("serverInfo"), ServerInfo);

	// Create success response
	OutResponse = FJsonRpcUtils::CreateSuccessResponse(Id, MakeShareable(new FJsonValueObject(Result)));
	
	FN2CLogger::Get().Log(TEXT("MCP connection initialized successfully"), EN2CLogSeverity::Info);
	
	return true;
}

bool FN2CMcpHttpRequestHandler::HandleToolsList(const TSharedPtr<FJsonValue>& Params, const TSharedPtr<FJsonValue>& Id, FJsonRpcResponse& OutResponse)
{
	FN2CLogger::Get().Log(TEXT("Processing MCP tools/list request"), EN2CLogSeverity::Info);

	// For MVP, we ignore pagination params (cursor) and return all tools
	// TODO: In the future, implement pagination if tool list becomes large

	// Get all registered tools from the manager
	TArray<FMcpToolDefinition> Tools = FN2CMcpToolManager::Get().GetAllToolDefinitions();

	// Build the result object
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);

	// Create tools array
	TArray<TSharedPtr<FJsonValue>> ToolsArray;
	for (const FMcpToolDefinition& Tool : Tools)
	{
		TSharedPtr<FJsonObject> ToolJson = Tool.ToJson();
		if (ToolJson.IsValid())
		{
			ToolsArray.Add(MakeShareable(new FJsonValueObject(ToolJson)));
		}
	}

	Result->SetArrayField(TEXT("tools"), ToolsArray);

	// For MVP, no pagination so no nextCursor field

	// Create success response
	OutResponse = FJsonRpcUtils::CreateSuccessResponse(Id, MakeShareable(new FJsonValueObject(Result)));

	FN2CLogger::Get().Log(FString::Printf(TEXT("Returned %d tools in tools/list response"), Tools.Num()), EN2CLogSeverity::Info);

	return true;
}

bool FN2CMcpHttpRequestHandler::HandleToolsCall(const TSharedPtr<FJsonValue>& Params, const TSharedPtr<FJsonValue>& Id, FJsonRpcResponse& OutResponse)
{
	FN2CLogger::Get().Log(TEXT("Processing MCP tools/call request"), EN2CLogSeverity::Info);

	// Validate params
	if (!Params.IsValid() || Params->IsNull())
	{
		FN2CLogger::Get().LogWarning(TEXT("tools/call request missing or null params"));
		OutResponse = FJsonRpcUtils::CreateErrorResponse(Id, JsonRpcErrorCodes::InvalidParams, TEXT("Missing or null params for tools/call"));
		return true;
	}

	if (Params->Type != EJson::Object)
	{
		FN2CLogger::Get().LogWarning(TEXT("tools/call request params is not an object"));
		OutResponse = FJsonRpcUtils::CreateErrorResponse(Id, JsonRpcErrorCodes::InvalidParams, TEXT("Params must be an object for tools/call"));
		return true;
	}

	TSharedPtr<FJsonObject> ParamsObject = Params->AsObject();

	// Extract tool name
	FString ToolName;
	if (!ParamsObject->TryGetStringField(TEXT("name"), ToolName))
	{
		FN2CLogger::Get().LogWarning(TEXT("tools/call request missing tool name"));
		OutResponse = FJsonRpcUtils::CreateErrorResponse(Id, JsonRpcErrorCodes::InvalidParams, TEXT("Missing required field: name"));
		return true;
	}

	// Extract arguments (optional, can be null)
	const TSharedPtr<FJsonObject>* ArgumentsObj = nullptr;
	TSharedPtr<FJsonObject> Arguments;
	if (ParamsObject->TryGetObjectField(TEXT("arguments"), ArgumentsObj) && ArgumentsObj->IsValid())
	{
		Arguments = *ArgumentsObj;
	}
	else
	{
		// If no arguments provided, use empty object
		Arguments = MakeShareable(new FJsonObject);
	}

	FN2CLogger::Get().Log(FString::Printf(TEXT("Calling tool: %s"), *ToolName), EN2CLogSeverity::Info);

	// Check if tool exists
	if (!FN2CMcpToolManager::Get().IsToolRegistered(ToolName))
	{
		FN2CLogger::Get().LogWarning(FString::Printf(TEXT("Tool not found: %s"), *ToolName));
		OutResponse = FJsonRpcUtils::CreateErrorResponse(Id, JsonRpcErrorCodes::MethodNotFound, FString::Printf(TEXT("Tool not found: %s"), *ToolName));
		return true;
	}

	// TODO: Validate arguments against tool's input schema (for MVP, skip validation)

	// Execute the tool
	FMcpToolCallResult ToolResult = FN2CMcpToolManager::Get().ExecuteTool(ToolName, Arguments);

	// Convert tool result to JSON
	TSharedPtr<FJsonObject> ResultJson = ToolResult.ToJson();

	// Create success response
	OutResponse = FJsonRpcUtils::CreateSuccessResponse(Id, MakeShareable(new FJsonValueObject(ResultJson)));

	return true;
}