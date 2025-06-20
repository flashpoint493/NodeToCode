// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include <memory>
#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>

// Forward declare httplib types to avoid including httplib.h in this header
namespace httplib {
    class Server;
    struct Request;
    struct Response;
    class DataSink;
}

namespace NodeToCodeSseServer
{
    struct SseClientConnection; // Forward declaration

    /**
     * Starts the SSE HTTP server on a separate thread.
     * @param Port The port number for the SSE server to listen on.
     * @return True if the server started successfully, false otherwise.
     */
    bool StartSseServer(int32 Port);

    /**
     * Stops the SSE HTTP server.
     */
    void StopSseServer();

    /**
     * Checks if the SSE server is currently running.
     * @return True if the server is running.
     */
    bool IsSseServerRunning();

    /**
     * Gets the port the SSE server is running on.
     * @return The port number, or -1 if not running.
     */
    int32 GetSseServerPort();

    /**
     * Pushes an SSE-formatted event string to a specific client's queue.
     * @param TaskId The Task ID identifying the client connection.
     * @param SseMessage The fully formatted SSE message string (including "event:", "data:", and newlines).
     */
    void PushFormattedSseEventToClient(const FGuid& TaskId, const std::string& SseMessage);

    /**
     * Signals an SSE client connection that its task is complete and the stream should be closed.
     * @param TaskId The Task ID identifying the client connection.
     */
    void SignalSseClientCompletion(const FGuid& TaskId);
    
    /**
    * Formats an event type and JSON data into an SSE message string.
    * This can be called from UE code to prepare messages for PushFormattedSseEventToClient.
    * @param EventType The type of the SSE event (e.g., "progress", "response"). Can be empty for default "message" events or comments.
    * @param JsonData The JSON string data for the event.
    * @return A fully formatted SSE message string.
    */
    std::string FormatSseMessage(const FString& EventType, const FString& JsonData);

} // namespace NodeToCodeSseServer