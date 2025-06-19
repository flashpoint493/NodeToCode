// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "N2CMcpJsonRpcTypes.h"

/**
 * Handles processing of MCP JSON-RPC requests received over HTTP.
 * Responsible for parsing, validating, and routing JSON-RPC messages.
 */
class NODETOCODE_API FN2CMcpHttpRequestHandler
{
public:
	/**
	 * Processes an MCP request body and generates a response.
	 * @param RequestBody The raw HTTP request body containing JSON-RPC message(s)
	 * @param OutResponseBody The generated JSON-RPC response body
	 * @param OutStatusCode The HTTP status code to return
	 * @param OutWasInitializeCall Set to true if the request was an 'initialize' method call
	 * @return true if the request was processed successfully, false otherwise
	 */
	static bool ProcessMcpRequest(const FString& RequestBody, FString& OutResponseBody, int32& OutStatusCode, bool& OutWasInitializeCall);

private:
	/**
	 * Parses a JSON-RPC message from a string.
	 * @param JsonString The JSON string to parse
	 * @param OutJsonObject The parsed JSON object
	 * @return true if parsing was successful, false otherwise
	 */
	static bool ParseJsonRpcMessage(const FString& JsonString, TSharedPtr<FJsonObject>& OutJsonObject);

	/**
	 * Validates that a JSON object is a valid JSON-RPC 2.0 message.
	 * @param JsonObject The JSON object to validate
	 * @return true if the object is a valid JSON-RPC message, false otherwise
	 */
	static bool ValidateJsonRpcMessage(const TSharedPtr<FJsonObject>& JsonObject);

	/**
	 * Processes a JSON-RPC request and generates a response.
	 * @param Request The JSON-RPC request
	 * @param OutResponse The generated JSON-RPC response
	 * @return true if the request was processed successfully, false otherwise
	 */
	static bool ProcessRequest(const FJsonRpcRequest& Request, FJsonRpcResponse& OutResponse);

	/**
	 * Processes a JSON-RPC notification.
	 * @param Notification The JSON-RPC notification
	 * @return true if the notification was processed successfully, false otherwise
	 */
	static bool ProcessNotification(const FJsonRpcNotification& Notification);

	/**
	 * Dispatches a JSON-RPC method call to the appropriate handler.
	 * @param Method The method name to dispatch
	 * @param Params The method parameters (can be null)
	 * @param Id The request ID (can be null for notifications)
	 * @param OutResponse The generated response
	 * @return true if the method was handled successfully, false otherwise
	 */
	static bool DispatchMethod(const FString& Method, const TSharedPtr<FJsonValue>& Params, const TSharedPtr<FJsonValue>& Id, FJsonRpcResponse& OutResponse);

	/**
	 * Handles the MCP initialize request.
	 * @param Params The request parameters
	 * @param Id The request ID
	 * @param OutResponse The generated response
	 * @return true if handled successfully
	 */
	static bool HandleInitialize(const TSharedPtr<FJsonValue>& Params, const TSharedPtr<FJsonValue>& Id, FJsonRpcResponse& OutResponse);

	/**
	 * Handles the MCP tools/list request.
	 * @param Params The request parameters
	 * @param Id The request ID
	 * @param OutResponse The generated response
	 * @return true if handled successfully
	 */
	static bool HandleToolsList(const TSharedPtr<FJsonValue>& Params, const TSharedPtr<FJsonValue>& Id, FJsonRpcResponse& OutResponse);

	/**
	 * Handles the MCP tools/call request.
	 * @param Params The request parameters
	 * @param Id The request ID
	 * @param OutResponse The generated response
	 * @return true if handled successfully
	 */
	static bool HandleToolsCall(const TSharedPtr<FJsonValue>& Params, const TSharedPtr<FJsonValue>& Id, FJsonRpcResponse& OutResponse);

	/**
	 * Handles the MCP resources/list request.
	 * @param Params The request parameters
	 * @param Id The request ID
	 * @param OutResponse The generated response
	 * @return true if handled successfully
	 */
	static bool HandleResourcesList(const TSharedPtr<FJsonValue>& Params, const TSharedPtr<FJsonValue>& Id, FJsonRpcResponse& OutResponse);

	/**
	 * Handles the MCP resources/read request.
	 * @param Params The request parameters
	 * @param Id The request ID
	 * @param OutResponse The generated response
	 * @return true if handled successfully
	 */
	static bool HandleResourcesRead(const TSharedPtr<FJsonValue>& Params, const TSharedPtr<FJsonValue>& Id, FJsonRpcResponse& OutResponse);

	/**
	 * Handles the MCP prompts/list request.
	 * @param Params The request parameters
	 * @param Id The request ID
	 * @param OutResponse The generated response
	 * @return true if handled successfully
	 */
	static bool HandlePromptsList(const TSharedPtr<FJsonValue>& Params, const TSharedPtr<FJsonValue>& Id, FJsonRpcResponse& OutResponse);

	/**
	 * Handles the MCP prompts/get request.
	 * @param Params The request parameters
	 * @param Id The request ID
	 * @param OutResponse The generated response
	 * @return true if handled successfully
	 */
	static bool HandlePromptsGet(const TSharedPtr<FJsonValue>& Params, const TSharedPtr<FJsonValue>& Id, FJsonRpcResponse& OutResponse);

	/**
	 * Handles the nodetocode/cancelTask request.
	 * @param Params The request parameters
	 * @param Id The request ID
	 * @param OutResponse The generated response
	 * @return true if handled successfully
	 */
	static bool HandleCancelTask(const TSharedPtr<FJsonValue>& Params, const TSharedPtr<FJsonValue>& Id, FJsonRpcResponse& OutResponse);

	/**
	 * Processes a batch of JSON-RPC requests.
	 * @param BatchArray The array of JSON-RPC requests
	 * @param OutResponseBody The generated response body (empty for all-notifications)
	 * @param OutStatusCode The HTTP status code to return
	 * @return true if the batch was processed successfully
	 */
	static bool ProcessBatchRequest(const TArray<TSharedPtr<FJsonValue>>& BatchArray, FString& OutResponseBody, int32& OutStatusCode);

private:
	// Supported protocol versions in order of preference (newest first)
	static const TArray<FString> SUPPORTED_PROTOCOL_VERSIONS;
	
	// These methods are now handled by FJsonRpcUtils
};