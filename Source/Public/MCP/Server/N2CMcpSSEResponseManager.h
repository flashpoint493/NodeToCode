// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HttpServerResponse.h"
#include "HttpServerRequest.h"
#include "MCP/Server/N2CMcpJsonRpcTypes.h"
#include "HAL/CriticalSection.h"

/**
 * Manages Server-Sent Events (SSE) responses for long-running MCP tool calls
 * Handles streaming responses back to the MCP connector
 */
class NODETOCODE_API FN2CMcpSSEResponseManager
{
public:
	/**
	 * SSE connection context
	 */
	struct FSSEConnection
	{
		/** The HTTP request that initiated this SSE stream */
		TSharedPtr<FHttpServerRequest> Request;

		/** The streaming response object */
		TSharedPtr<FHttpServerResponse> Response;

		/** Session ID for this connection */
		FString SessionId;

		/** Original request ID from the MCP request */
		TSharedPtr<FJsonValue> OriginalRequestId;

		/** Progress token for this connection */
		FString ProgressToken;

		/** Task ID associated with this connection */
		FGuid TaskId;

		/** Whether the connection is still active */
		bool bIsActive = true;

		/** Timestamp when the connection was established */
		FDateTime ConnectionTime;
	};

public:
	/**
	 * Gets the singleton instance
	 */
	static FN2CMcpSSEResponseManager& Get();

	/**
	 * Creates a new SSE connection for an async tool call
	 * @param Request The HTTP request
	 * @param SessionId The MCP session ID
	 * @param OriginalRequestId The original request ID from the MCP request
	 * @param ProgressToken The progress token
	 * @param TaskId The async task ID
	 * @return The connection ID, or empty string if failed
	 */
	FString CreateSSEConnection(const TSharedPtr<FHttpServerRequest>& Request, const FString& SessionId,
		const TSharedPtr<FJsonValue>& OriginalRequestId, const FString& ProgressToken, const FGuid& TaskId);

	/**
	 * Sends a progress notification over an SSE connection
	 * @param ConnectionId The connection ID
	 * @param ProgressNotification The progress notification to send
	 * @return true if sent successfully
	 */
	bool SendProgressNotification(const FString& ConnectionId, const FJsonRpcNotification& ProgressNotification);

	/**
	 * Sends the final response and closes the SSE connection
	 * @param ConnectionId The connection ID
	 * @param Response The final JSON-RPC response
	 * @return true if sent successfully
	 */
	bool SendFinalResponse(const FString& ConnectionId, const FJsonRpcResponse& Response);

	/**
	 * Finds a connection by task ID
	 * @param TaskId The task ID
	 * @return The connection ID, or empty string if not found
	 */
	FString FindConnectionByTaskId(const FGuid& TaskId) const;

	/**
	 * Finds a connection by progress token
	 * @param ProgressToken The progress token
	 * @return The connection ID, or empty string if not found
	 */
	FString FindConnectionByProgressToken(const FString& ProgressToken) const;

	/**
	 * Closes an SSE connection
	 * @param ConnectionId The connection ID
	 */
	void CloseConnection(const FString& ConnectionId);

	/**
	 * Closes all active connections
	 */
	void CloseAllConnections();

	/**
	 * Gets the number of active connections
	 * @return The count of active connections
	 */
	int32 GetActiveConnectionCount() const;

	/**
	 * Cleans up inactive or timed-out connections
	 */
	void CleanupInactiveConnections();

private:
	// Private constructor for singleton
	FN2CMcpSSEResponseManager() = default;

	// Non-copyable
	FN2CMcpSSEResponseManager(const FN2CMcpSSEResponseManager&) = delete;
	FN2CMcpSSEResponseManager& operator=(const FN2CMcpSSEResponseManager&) = delete;

	/**
	 * Writes an SSE event to the response stream
	 * @param Connection The SSE connection
	 * @param EventType The event type (optional)
	 * @param Data The event data
	 * @return true if written successfully
	 */
	bool WriteSSEEvent(const TSharedPtr<FSSEConnection>& Connection, const FString& EventType, const FString& Data);

	/**
	 * Formats data as an SSE event
	 * @param EventType The event type (optional)
	 * @param Data The event data
	 * @return The formatted SSE event string
	 */
	FString FormatSSEEvent(const FString& EventType, const FString& Data) const;

private:
	/** Map of connection IDs to SSE connections */
	TMap<FString, TSharedPtr<FSSEConnection>> ActiveConnections;

	/** Thread safety for connection map access */
	mutable FCriticalSection ConnectionMapLock;

	/** Connection timeout in seconds */
	static constexpr float CONNECTION_TIMEOUT_SECONDS = 300.0f; // 5 minutes
};