// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#include "MCP/Server/N2CMcpHttpRequestHandler.h"
#include "Utils/N2CLogger.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

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
	
	// For now, just log notifications - specific handlers can be added later
	return true;
}

bool FN2CMcpHttpRequestHandler::DispatchMethod(const FString& Method, const TSharedPtr<FJsonValue>& Params, const TSharedPtr<FJsonValue>& Id, FJsonRpcResponse& OutResponse)
{
	FN2CLogger::Get().Log(FString::Printf(TEXT("Dispatching JSON-RPC method: %s"), *Method), EN2CLogSeverity::Debug);

	// Handle method calls that expect responses
	if (Method == TEXT("ping"))
	{
		// Simple ping response
		TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
		OutResponse = FJsonRpcUtils::CreateSuccessResponse(Id, MakeShareable(new FJsonValueObject(Result)));
		return true;
	}
	else
	{
		// Method not found
		FN2CLogger::Get().LogWarning(FString::Printf(TEXT("JSON-RPC method not found: %s"), *Method));
		OutResponse = FJsonRpcUtils::CreateErrorResponse(Id, JsonRpcErrorCodes::MethodNotFound, FString::Printf(TEXT("Method '%s' not found"), *Method));
		return true;
	}
}