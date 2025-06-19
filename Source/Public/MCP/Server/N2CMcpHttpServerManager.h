// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "HttpServerModule.h"
#include "IHttpRouter.h"
#include "HAL/CriticalSection.h"
#include "MCP/Server/IN2CMcpNotificationChannel.h"
#include "MCP/Server/N2CMcpJsonRpcTypes.h"
#include "MCP/Tools/N2CMcpToolTypes.h"

/**
 * Manages the HTTP server for Model Context Protocol (MCP) communication.
 * Provides a lightweight HTTP server that listens on localhost for MCP JSON-RPC messages.
 */
class NODETOCODE_API FN2CMcpHttpServerManager
{
public:
	/**
	 * Gets the singleton instance of the MCP HTTP server manager.
	 */
	static FN2CMcpHttpServerManager& Get();

	/**
	 * Sends an async task progress notification to the appropriate SSE stream
	 * @param SessionId The session ID
	 * @param ProgressNotification The progress notification to send
	 */
	void SendAsyncTaskProgress(const FString& SessionId, const FJsonRpcNotification& ProgressNotification);

	/**
	 * Sends the final async task response and closes the SSE stream
	 * @param TaskId The ID of the completed task
	 * @param OriginalRequestId The original request ID from the MCP client
	 * @param Result The task result
	 */
	void SendAsyncTaskResponse(const FGuid& TaskId, const TSharedPtr<FJsonValue>& OriginalRequestId, const FMcpToolCallResult& Result);

	/**
	 * Initializes and starts the HTTP server on the specified port.
	 * @param Port The port to listen on (default: 27000)
	 * @return true if the server started successfully, false otherwise
	 */
	bool StartServer(int32 Port = 27000);

	/**
	 * Stops the HTTP server and cleans up resources.
	 */
	void StopServer();

	/**
	 * Checks if the server is currently running.
	 * @return true if the server is running, false otherwise
	 */
	bool IsServerRunning() const { return bIsServerRunning; }

	/**
	 * Gets the port the server is listening on.
	 * @return The server port, or -1 if not running
	 */
	int32 GetServerPort() const { return ServerPort; }

	/**
	 * Registers a client channel for receiving notifications.
	 * @param SessionId The session ID to associate with this channel
	 * @param Channel The notification channel implementation
	 */
	void RegisterClient(const FString& SessionId, TSharedPtr<IN2CMcpNotificationChannel> Channel);

	/**
	 * Unregisters a client channel.
	 * @param SessionId The session ID to unregister
	 */
	void UnregisterClient(const FString& SessionId);

	/**
	 * Broadcasts a notification to all connected clients.
	 * @param Notification The notification to broadcast
	 */
	void BroadcastNotification(const FJsonRpcNotification& Notification);

	/**
	 * Sends a notification to a specific client.
	 * @param SessionId The session ID of the client
	 * @param Notification The notification to send
	 */
	void SendNotificationToClient(const FString& SessionId, const FJsonRpcNotification& Notification);

	/**
	 * Gets the current session ID for the active connection.
	 * @return The current session ID, or empty string if none
	 */
	FString GetCurrentSessionId() const { return CurrentSessionId; }

	/**
	 * Sets the protocol version for a session.
	 * @param SessionId The session ID
	 * @param ProtocolVersion The negotiated protocol version
	 */
	void SetSessionProtocolVersion(const FString& SessionId, const FString& ProtocolVersion);

private:
	// Private constructor for singleton
	FN2CMcpHttpServerManager() = default;

	// Non-copyable
	FN2CMcpHttpServerManager(const FN2CMcpHttpServerManager&) = delete;
	FN2CMcpHttpServerManager& operator=(const FN2CMcpHttpServerManager&) = delete;

	/**
	 * Handles incoming HTTP requests to the MCP endpoint.
	 */
	bool HandleMcpRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);

	/**
	 * Handles health check requests.
	 */
	bool HandleHealthRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);

	/**
	 * Registers MCP tools with the tool manager.
	 */
	void RegisterMcpTools();

	/**
	 * Registers MCP resources with the resource manager.
	 */
	void RegisterMcpResources();

	/**
	 * Registers MCP prompts with the prompt manager.
	 */
	void RegisterMcpPrompts();

	/** Whether the server is currently running */
	bool bIsServerRunning = false;

	/** The port the server is listening on */
	int32 ServerPort = -1;

	/** HTTP server module reference */
	FHttpServerModule* HttpServerModule = nullptr;

	/** HTTP router for handling requests */
	TSharedPtr<IHttpRouter> HttpRouter;

	/** Current session ID for the MCP connection */
	FString CurrentSessionId;

	/** Map of session IDs to notification channels */
	TMap<FString, TSharedPtr<IN2CMcpNotificationChannel>> ClientChannels;

	/** Thread safety for client channel access */
	mutable FCriticalSection ClientChannelLock;

	/** Map of session IDs to their negotiated protocol versions */
	TMap<FString, FString> SessionProtocolVersions;
};
