// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/Server/N2CMcpJsonRpcTypes.h"

/**
 * Interface for notification channels in the MCP server.
 * Different transport mechanisms can implement this interface to receive notifications.
 */
class IN2CMcpNotificationChannel
{
public:
	virtual ~IN2CMcpNotificationChannel() = default;

	/**
	 * Sends a notification through this channel.
	 * @param Notification The JSON-RPC notification to send
	 * @return true if the notification was sent successfully
	 */
	virtual bool SendNotification(const FJsonRpcNotification& Notification) = 0;

	/**
	 * Checks if this channel is still active and can receive notifications.
	 * @return true if the channel is active
	 */
	virtual bool IsActive() const = 0;

	/**
	 * Gets the session ID associated with this channel.
	 * @return The session ID
	 */
	virtual FString GetSessionId() const = 0;
};