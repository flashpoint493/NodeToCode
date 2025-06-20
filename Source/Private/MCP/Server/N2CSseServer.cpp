// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#include "MCP/Server/N2CSseServer.h"
#include "Utils/N2CLogger.h"
#include "HAL/PlatformProcess.h"
#include "HAL/CriticalSection.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Guid.h"

// Include httplib.h here in the .cpp file
#if PLATFORM_WINDOWS
#pragma warning(push)
// Disable specific warnings httplib.h might trigger
#pragma warning(disable : 4267) // size_t to int truncation
#pragma warning(disable : 4244) // conversion from 'X' to 'Y', possible loss of data
#endif
#include "httplib.h"
#if PLATFORM_WINDOWS
#pragma warning(pop)
#endif

namespace NodeToCodeSseServer
{
    // Structure to hold connection-specific data for each SSE client
    struct SseClientConnection
    {
        std::queue<std::string> event_queue;
        std::mutex queue_mutex;
        std::condition_variable cv;
        std::atomic<bool> stop_requested{false};
        FGuid taskId;
    };

    // Map for active SSE clients
    TMap<FGuid, TSharedPtr<SseClientConnection>> GActiveSseClients;
    FCriticalSection GActiveSseClientsMutex; // UE's critical section

    // httplib server instance and thread
    TSharedPtr<httplib::Server> GSseHttpServer;
    std::unique_ptr<std::thread> GSseServerThreadPtr; // Use unique_ptr for better management
    std::atomic<bool> GbSseServerIsRunning{false};
    int32 GSseServerPort = -1;

    std::string FormatSseMessage(const FString& EventType, const FString& JsonData)
    {
        std::string sse_message;
        if (!EventType.IsEmpty())
        {
            sse_message += "event: " + std::string(TCHAR_TO_UTF8(*EventType)) + "\n";
        }
        
        TArray<FString> Lines;
        // Ensure JsonData is parsed correctly as lines, even if it contains newlines itself
        FString TempData = JsonData;
        TempData.Replace(TEXT("\r\n"), TEXT("\n")); // Normalize newlines
        TempData.ParseIntoArrayLines(Lines, false); // false to keep empty lines if any

        for (const FString& Line : Lines)
        {
            sse_message += "data: " + std::string(TCHAR_TO_UTF8(*Line)) + "\n";
        }
        sse_message += "\n"; // Double newline to end the event
        return sse_message;
    }

    void PushFormattedSseEventToClient(const FGuid& TaskId, const std::string& SseMessage)
    {
        TSharedPtr<SseClientConnection> ClientConn;
        {
            FScopeLock Lock(&GActiveSseClientsMutex);
            TSharedPtr<SseClientConnection>* FoundConn = GActiveSseClients.Find(TaskId);
            if (FoundConn)
            {
                ClientConn = *FoundConn;
            }
        }

        if (ClientConn)
        {
            {
                std::lock_guard<std::mutex> q_lock(ClientConn->queue_mutex);
                if (ClientConn->stop_requested.load()) {
                     FN2CLogger::Get().Log(FString::Printf(TEXT("SSE: Client %s is stopping, not queuing event."), *TaskId.ToString()), EN2CLogSeverity::Debug);
                    return;
                }
                ClientConn->event_queue.push(SseMessage);
            }
            ClientConn->cv.notify_one();
            FN2CLogger::Get().Log(FString::Printf(TEXT("SSE: Queued event for TaskId %s. Message: %s"), *TaskId.ToString(), UTF8_TO_TCHAR(SseMessage.substr(0, 100).c_str())), EN2CLogSeverity::Debug);
        }
        else
        {
            FN2CLogger::Get().LogWarning(FString::Printf(TEXT("SSE: No active client connection found for TaskId %s to push event."), *TaskId.ToString()));
        }
    }

    void SignalSseClientCompletion(const FGuid& TaskId)
    {
        TSharedPtr<SseClientConnection> ClientConn;
        {
            FScopeLock Lock(&GActiveSseClientsMutex);
            TSharedPtr<SseClientConnection>* FoundConn = GActiveSseClients.Find(TaskId);
            if (FoundConn)
            {
                ClientConn = *FoundConn;
            }
        }

        if (ClientConn)
        {
            {
                std::lock_guard<std::mutex> q_lock(ClientConn->queue_mutex);
                ClientConn->stop_requested.store(true);
            }
            ClientConn->cv.notify_one();
            FN2CLogger::Get().Log(FString::Printf(TEXT("SSE: Signaled completion for TaskId %s."), *TaskId.ToString()), EN2CLogSeverity::Info);
        }
         else
        {
             FN2CLogger::Get().LogWarning(FString::Printf(TEXT("SSE: No active client connection found for TaskId %s to signal completion."), *TaskId.ToString()));
        }
    }

    bool StartSseServer(int32 Port)
    {
        if (GbSseServerIsRunning.load()) {
            FN2CLogger::Get().LogWarning(FString::Printf(TEXT("SSE: Server already running on port %d."), GSseServerPort));
            return true;
        }

        GSseServerPort = Port;
        GSseHttpServer = MakeShareable(new httplib::Server());

        if (!GSseHttpServer.IsValid()) {
            FN2CLogger::Get().LogError(TEXT("SSE: Failed to create httplib::Server instance."));
            return false;
        }
        
        // Route for SSE events: /mcp/events/{taskIdGUID}
        // Regex: TaskId is a GUID (xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx)
        // Example: /mcp/events/123e4567-e89b-12d3-a456-426614174000
        GSseHttpServer->Get(R"(/mcp/events/([0-9a-fA-F]{8}-(?:[0-9a-fA-F]{4}-){3}[0-9a-fA-F]{12}))",
            [](const httplib::Request& req, httplib::Response& res) {
            
            FGuid TaskId;
            FString TaskIdStr = UTF8_TO_TCHAR(req.matches[1].str().c_str());
            
            if (!FGuid::Parse(TaskIdStr, TaskId) || !TaskId.IsValid())
            {
                res.status = 400; // Bad Request
                res.set_content("Invalid Task ID format in URL path.", "text/plain");
                FN2CLogger::Get().LogError(FString::Printf(TEXT("SSE: Received request with invalid TaskId URL format: %s"), *TaskIdStr));
                return;
            }

            FN2CLogger::Get().Log(FString::Printf(TEXT("SSE: Client connected for TaskId: %s"), *TaskIdStr), EN2CLogSeverity::Info);

            res.set_header("Content-Type", "text/event-stream");
            res.set_header("Cache-Control", "no-cache");
            res.set_header("Connection", "keep-alive");
            res.set_header("Access-Control-Allow-Origin", "*"); 

            auto ClientConn = MakeShared<SseClientConnection>();
            ClientConn->taskId = TaskId;

            {
                FScopeLock Lock(&GActiveSseClientsMutex);
                if (GActiveSseClients.Contains(TaskId))
                {
                    // Handle scenario where a client reconnects for an existing task.
                    // Option: Reject (only one SSE stream per task)
                    FN2CLogger::Get().LogWarning(FString::Printf(TEXT("SSE: TaskId %s already has an active SSE connection. Rejecting new connection."), *TaskIdStr));
                    res.status = 409; // Conflict
                    res.set_content("Task ID already has an active SSE stream.", "text/plain");
                    return;
                }
                GActiveSseClients.Add(TaskId, ClientConn);
            }
            
            // Send an initial comment to establish the connection immediately.
            std::string InitialComment = FormatSseMessage(TEXT(""), TEXT(": SSE connection established for task ") + TaskIdStr);
            // We push this to the queue, the content_provider will send it.
            {
                 std::lock_guard<std::mutex> q_lock(ClientConn->queue_mutex);
                 ClientConn->event_queue.push(InitialComment);
            }
            // No cv.notify_one() here, the content_provider loop will pick it up on its first run.

            res.set_content_provider(
                "text/event-stream", 
                [ClientConn](size_t offset, httplib::DataSink &sink) -> bool {
                    std::unique_lock<std::mutex> q_lock(ClientConn->queue_mutex);
                    
                    // Wait until there's something in the queue or stop is requested
                    ClientConn->cv.wait(q_lock, [&ClientConn] {
                        return !ClientConn->event_queue.empty() || ClientConn->stop_requested.load();
                    });

                    bool bStopAfterThisWrite = false;
                    if (ClientConn->stop_requested.load() && ClientConn->event_queue.empty()) {
                        q_lock.unlock(); // Release before calling sink.done()
                        sink.done(); // Signal end of stream
                        FN2CLogger::Get().Log(FString::Printf(TEXT("SSE Provider: TaskId %s stop_requested and queue empty. Sink done."), *ClientConn->taskId.ToString()), EN2CLogSeverity::Debug);
                        return false; // Stop the provider loop
                    }

                    if (!ClientConn->event_queue.empty()) {
                        std::string event_data_to_send = ClientConn->event_queue.front();
                        ClientConn->event_queue.pop();

                        // Check again if this is the last event after popping
                        if (ClientConn->stop_requested.load() && ClientConn->event_queue.empty()) {
                             bStopAfterThisWrite = true;
                        }
                        q_lock.unlock(); // Release mutex before writing to sink
                        
                        if (!sink.write(event_data_to_send.c_str(), event_data_to_send.length())) {
                            FN2CLogger::Get().LogError(FString::Printf(TEXT("SSE Provider: sink.write failed for TaskId %s."), *ClientConn->taskId.ToString()));
                            ClientConn->stop_requested.store(true); // Ensure cleanup on write failure
                            // Do not call sink.done() here, let the releaser handle it or httplib closes it
                            return false; // Stop the provider loop on write failure
                        }

                        if (bStopAfterThisWrite) {
                            sink.done();
                            FN2CLogger::Get().Log(FString::Printf(TEXT("SSE Provider: TaskId %s sent last event and called sink.done()."), *ClientConn->taskId.ToString()), EN2CLogSeverity::Debug);
                            return false; // Stop after sending the final event and calling done
                        }
                    } else {
                         q_lock.unlock(); // Should not happen if stop_requested is false due to cv.wait condition
                    }
                    
                    return true; // Continue providing data
                },
                [TaskId, TaskIdStr](bool success) { // Releaser lambda
                    FScopeLock Lock(&GActiveSseClientsMutex);
                    GActiveSseClients.Remove(TaskId);
                    FN2CLogger::Get().Log(FString::Printf(TEXT("SSE: Connection for TaskId %s (Path: %s) released by httplib. Success: %d"), *TaskId.ToString(), *TaskIdStr, success), EN2CLogSeverity::Info);
                }
            );
        });
        
        GbSseServerIsRunning.store(false); // Will be set to true after listen starts
        GSseServerThreadPtr = std::make_unique<std::thread>([Port]() {
            FN2CLogger::Get().Log(FString::Printf(TEXT("SSE: httplib server thread starting, attempting to listen on 0.0.0.0:%d"), Port), EN2CLogSeverity::Info);
            GbSseServerIsRunning.store(true); // Server is now considered running
            bool bListenSuccess = GSseHttpServer->listen("0.0.0.0", Port);
            GbSseServerIsRunning.store(false); // Server stopped listening

            if (!bListenSuccess) {
                FN2CLogger::Get().LogError(FString::Printf(TEXT("SSE: httplib server failed to listen on port %d or stopped unexpectedly."), Port));
            } else {
                 FN2CLogger::Get().Log(TEXT("SSE: httplib server finished listening."), EN2CLogSeverity::Info);
            }
        });
        
        // Brief sleep to allow the server thread to start listening
        FPlatformProcess::Sleep(0.1f); 
        if (!GbSseServerIsRunning.load() && GSseHttpServer->is_running()) { // Double check after sleep
             GbSseServerIsRunning.store(true);
        }

        return GbSseServerIsRunning.load();
    }

    void StopSseServer()
    {
        if (!GSseHttpServer.IsValid() && !GbSseServerIsRunning.load()) {
            FN2CLogger::Get().Log(TEXT("SSE: Server already stopped or not initialized."), EN2CLogSeverity::Debug);
            return;
        }

        FN2CLogger::Get().Log(TEXT("SSE: Initiating server stop..."), EN2CLogSeverity::Info);
        
        // Signal all active client connections to stop
        {
            FScopeLock Lock(&GActiveSseClientsMutex);
            for (auto& Pair : GActiveSseClients)
            {
                if (Pair.Value)
                {
                    std::lock_guard<std::mutex> q_lock(Pair.Value->queue_mutex);
                    Pair.Value->stop_requested.store(true);
                    Pair.Value->cv.notify_all(); 
                }
            }
            // Don't clear GActiveSseClients here, let the releaser lambdas do it
        }

        if (GSseHttpServer.IsValid()) {
            GSseHttpServer->stop(); // This is thread-safe and signals listen() to return
        }
        
        if (GSseServerThreadPtr && GSseServerThreadPtr->joinable())
        {
            FN2CLogger::Get().Log(TEXT("SSE: Waiting for server thread to join..."), EN2CLogSeverity::Debug);
            GSseServerThreadPtr->join();
            GSseServerThreadPtr.reset();
            FN2CLogger::Get().Log(TEXT("SSE: Server thread joined."), EN2CLogSeverity::Info);
        }
        
        GSseHttpServer.Reset(); // Release the server instance
        GbSseServerIsRunning.store(false);
        GSseServerPort = -1;
        
        // Final cleanup of any remaining clients, just in case
        {
             FScopeLock Lock(&GActiveSseClientsMutex);
             if(GActiveSseClients.Num() > 0)
             {
                FN2CLogger::Get().LogWarning(FString::Printf(TEXT("SSE: %d client connections remained after server stop. Clearing now."), GActiveSseClients.Num()));
                GActiveSseClients.Empty();
             }
        }
        FN2CLogger::Get().Log(TEXT("SSE: Server fully stopped."), EN2CLogSeverity::Info);
    }

    bool IsSseServerRunning()
    {
        return GbSseServerIsRunning.load() && GSseHttpServer.IsValid() && GSseHttpServer->is_running();
    }

    int32 GetSseServerPort()
    {
        return IsSseServerRunning() ? GSseServerPort : -1;
    }

} // namespace NodeToCodeSseServer