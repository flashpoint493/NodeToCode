// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "HttpServerModule.h"
#include "IHttpRouter.h"

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
};