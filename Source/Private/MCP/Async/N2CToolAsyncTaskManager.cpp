// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#include "MCP/Async/N2CToolAsyncTaskManager.h"
#include "MCP/Async/N2CTranslateBlueprintAsyncTask.h"
#include "MCP/Server/N2CMcpHttpServerManager.h"
#include "MCP/Server/N2CMcpJsonRpcTypes.h"
#include "MCP/Server/N2CSseServer.h"
#include "MCP/Tools/N2CMcpToolManager.h"
#include "Utils/N2CLogger.h"
#include "Async/TaskGraphInterfaces.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

FN2CToolAsyncTaskManager& FN2CToolAsyncTaskManager::Get()
{
	static FN2CToolAsyncTaskManager Instance;
	return Instance;
}

FGuid FN2CToolAsyncTaskManager::LaunchTask(const FGuid& InTaskId, const FString& ToolName, const TSharedPtr<FJsonObject>& Arguments,
	const FString& ProgressToken, const FString& SessionId, const TSharedPtr<FJsonValue>& OriginalRequestId)
{
	FGuid TaskId = InTaskId; // USE THE PASSED-IN TASK ID
	
	// Create the async task
	TSharedPtr<IN2CToolAsyncTask> AsyncTask = CreateAsyncTask(ToolName, TaskId, ProgressToken, Arguments);
	if (!AsyncTask.IsValid())
	{
		FN2CLogger::Get().LogError(FString::Printf(TEXT("Failed to create async task for tool: %s"), *ToolName));
		return FGuid();
	}

	// Create task context
	auto TaskContext = MakeShared<FN2CAsyncTaskContext>();
	TaskContext->TaskId = TaskId;
	TaskContext->ProgressToken = ProgressToken;
	TaskContext->SessionId = SessionId;
	TaskContext->OriginalRequestId = OriginalRequestId;
	TaskContext->ToolName = ToolName;
	TaskContext->Arguments = Arguments;
	TaskContext->Task = AsyncTask;

	// Set up progress callback with lambda to capture TaskId
	AsyncTask->SetProgressDelegate(FN2CAsyncTaskProgressDelegate::CreateLambda(
		[this, TaskId](float Progress, const FString& Message)
		{
			this->OnTaskProgress(TaskId, Progress, Message);
		}));

	// Set up completion callback with lambda to capture TaskId
	AsyncTask->SetCompleteDelegate(FN2CAsyncTaskCompleteDelegate::CreateLambda(
		[this, TaskId](const FMcpToolCallResult& Result)
		{
			this->OnTaskCompleted(TaskId, Result);
		}));

	// Launch the task on a background thread
	TaskContext->TaskFuture = Async(EAsyncExecution::ThreadPool, [AsyncTask]()
	{
		AsyncTask->Execute();
	});

	// Store the task context
	{
		FScopeLock Lock(&TaskMapLock);
		RunningTasks.Add(TaskId, TaskContext);
		ProgressTokenToTaskId.Add(ProgressToken, TaskId);
	}

	FN2CLogger::Get().Log(FString::Printf(TEXT("Launched async task %s for tool: %s (ProgressToken: %s)"), 
		*TaskId.ToString(), *ToolName, *ProgressToken), EN2CLogSeverity::Info);

	return TaskId;
}

bool FN2CToolAsyncTaskManager::CancelTask(const FGuid& TaskId)
{
	FScopeLock Lock(&TaskMapLock);
	
	auto TaskContext = RunningTasks.Find(TaskId);
	if (!TaskContext || !TaskContext->IsValid())
	{
		return false;
	}

	if ((*TaskContext)->Task.IsValid())
	{
		(*TaskContext)->Task->RequestCancel();
		FN2CLogger::Get().Log(FString::Printf(TEXT("Cancellation requested for task %s"), *TaskId.ToString()), EN2CLogSeverity::Info);
		return true;
	}

	return false;
}

bool FN2CToolAsyncTaskManager::CancelTaskByProgressToken(const FString& ProgressToken)
{
	FScopeLock Lock(&TaskMapLock);
	
	FGuid* TaskId = ProgressTokenToTaskId.Find(ProgressToken);
	if (!TaskId)
	{
		return false;
	}

	// Use the non-locked version since we already have the lock
	auto TaskContext = RunningTasks.Find(*TaskId);
	if (!TaskContext || !TaskContext->IsValid())
	{
		return false;
	}

	if ((*TaskContext)->Task.IsValid())
	{
		(*TaskContext)->Task->RequestCancel();
		FN2CLogger::Get().Log(FString::Printf(TEXT("Cancellation requested for task %s via progress token %s"), 
			*TaskId->ToString(), *ProgressToken), EN2CLogSeverity::Info);
		return true;
	}

	return false;
}

TSharedPtr<FN2CAsyncTaskContext> FN2CToolAsyncTaskManager::GetTaskContext(const FGuid& TaskId) const
{
	FScopeLock Lock(&TaskMapLock);
	
	auto TaskContext = RunningTasks.Find(TaskId);
	if (TaskContext && TaskContext->IsValid())
	{
		return *TaskContext;
	}
	
	return nullptr;
}

TSharedPtr<FN2CAsyncTaskContext> FN2CToolAsyncTaskManager::GetTaskContextByProgressToken(const FString& ProgressToken) const
{
	FScopeLock Lock(&TaskMapLock);
	
	const FGuid* TaskId = ProgressTokenToTaskId.Find(ProgressToken);
	if (!TaskId)
	{
		return nullptr;
	}

	auto TaskContext = RunningTasks.Find(*TaskId);
	if (TaskContext && TaskContext->IsValid())
	{
		return *TaskContext;
	}
	
	return nullptr;
}

bool FN2CToolAsyncTaskManager::IsTaskRunning(const FGuid& TaskId) const
{
	FScopeLock Lock(&TaskMapLock);
	
	auto TaskContext = RunningTasks.Find(TaskId);
	if (!TaskContext || !TaskContext->IsValid())
	{
		return false;
	}

	// Check if the future is still valid and not completed
	return (*TaskContext)->TaskFuture.IsValid() && !(*TaskContext)->TaskFuture.IsReady();
}

TArray<FGuid> FN2CToolAsyncTaskManager::GetRunningTaskIds() const
{
	FScopeLock Lock(&TaskMapLock);
	
	TArray<FGuid> RunningIds;
	for (const auto& Pair : RunningTasks)
	{
		if (Pair.Value.IsValid() && Pair.Value->TaskFuture.IsValid() && !Pair.Value->TaskFuture.IsReady())
		{
			RunningIds.Add(Pair.Key);
		}
	}
	
	return RunningIds;
}

void FN2CToolAsyncTaskManager::CleanupCompletedTasks()
{
	FScopeLock Lock(&TaskMapLock);
	
	TArray<FGuid> TasksToRemove;
	
	for (const auto& Pair : RunningTasks)
	{
		if (!Pair.Value.IsValid() || !Pair.Value->TaskFuture.IsValid() || Pair.Value->TaskFuture.IsReady())
		{
			TasksToRemove.Add(Pair.Key);
		}
	}

	for (const FGuid& TaskIdToRemove : TasksToRemove) // Renamed TaskId to TaskIdToRemove for clarity
	{
		auto TaskContextPtr = RunningTasks.Find(TaskIdToRemove);
		if (TaskContextPtr && TaskContextPtr->IsValid())
		{
			// Call the new cleanup function in NodeToCodeSseServer
			NodeToCodeSseServer::CleanupStreamForCompletedTask((*TaskContextPtr)->TaskId);
			ProgressTokenToTaskId.Remove((*TaskContextPtr)->ProgressToken);
		}
		RunningTasks.Remove(TaskIdToRemove);
	}

	if (TasksToRemove.Num() > 0)
	{
		FN2CLogger::Get().Log(FString::Printf(TEXT("Cleaned up %d completed async tasks"), TasksToRemove.Num()), EN2CLogSeverity::Debug);
	}
}

void FN2CToolAsyncTaskManager::CancelAllTasks()
{
	FScopeLock Lock(&TaskMapLock);
	
	for (auto& Pair : RunningTasks)
	{
		if (Pair.Value.IsValid() && Pair.Value->Task.IsValid())
		{
			Pair.Value->Task->RequestCancel();
		}
	}

	FN2CLogger::Get().Log(FString::Printf(TEXT("Requested cancellation for all %d running tasks"), RunningTasks.Num()), EN2CLogSeverity::Info);
}

TSharedPtr<IN2CToolAsyncTask> FN2CToolAsyncTaskManager::CreateAsyncTask(const FString& ToolName, const FGuid& TaskId, 
	const FString& ProgressToken, const TSharedPtr<FJsonObject>& Arguments)
{
	
	if (ToolName == TEXT("translate-focused-blueprint"))
	{
		return MakeShared<FN2CTranslateBlueprintAsyncTask>(TaskId, ProgressToken, Arguments);
	}
	
	FN2CLogger::Get().LogWarning(FString::Printf(TEXT("No async task implementation found for tool: %s. TaskId: %s"), *ToolName, *TaskId.ToString()));
	return nullptr;
}

void FN2CToolAsyncTaskManager::OnTaskCompleted(const FGuid& TaskId, const FMcpToolCallResult& Result)
{
	TSharedPtr<FN2CAsyncTaskContext> TaskContext = GetTaskContext(TaskId);
	if (!TaskContext.IsValid())
	{
		FN2CLogger::Get().LogWarning(FString::Printf(TEXT("Task completed but context not found: %s"), *TaskId.ToString()));
		return;
	}

	// Send the final JSON-RPC response via the SSE stream
	FN2CLogger::Get().Log(FString::Printf(TEXT("Task %s completed, sending final response"), *TaskId.ToString()), EN2CLogSeverity::Info);

	// Create the JSON-RPC response
	FJsonRpcResponse Response;
	Response.Id = TaskContext->OriginalRequestId;
	// Convert the tool result to JSON Value
	Response.Result = MakeShared<FJsonValueObject>(Result.ToJson());

	// Serialize to JSON string
	FString ResponseJson = FJsonRpcUtils::SerializeResponse(Response);
	if (!ResponseJson.IsEmpty())
	{
		// Push the final response to the SSE stream
		std::string SseMessage = NodeToCodeSseServer::FormatSseMessage(TEXT("response"), ResponseJson);
		NodeToCodeSseServer::PushFormattedSseEventToClient(TaskId, SseMessage);
	}
	else
	{
		FN2CLogger::Get().LogError(FString::Printf(TEXT("Failed to serialize response for task %s"), *TaskId.ToString()));
	}

	// Signal the SSE connection to close after sending the response
	NodeToCodeSseServer::SignalSseClientCompletion(TaskId);
}

void FN2CToolAsyncTaskManager::OnTaskProgress(const FGuid& TaskId, float Progress, const FString& Message)
{
	TSharedPtr<FN2CAsyncTaskContext> TaskContext = GetTaskContext(TaskId);
	if (!TaskContext.IsValid())
	{
		FN2CLogger::Get().LogWarning(FString::Printf(TEXT("Task progress reported but context not found: %s"), *TaskId.ToString()));
		return;
	}

	// Create MCP progress notification
	FJsonRpcNotification ProgressNotification;
	ProgressNotification.Method = TEXT("notifications/progress");
	
	auto ParamsObject = MakeShared<FJsonObject>();
	ParamsObject->SetStringField(TEXT("progressToken"), TaskContext->ProgressToken);
	ParamsObject->SetNumberField(TEXT("progress"), Progress);
	if (!Message.IsEmpty())
	{
		ParamsObject->SetStringField(TEXT("message"), Message);
	}
	
	ProgressNotification.Params = MakeShared<FJsonValueObject>(ParamsObject);

	// Send the progress notification via the SSE stream
	FN2CLogger::Get().Log(FString::Printf(TEXT("Task %s progress: %.1f%% - %s"), 
		*TaskId.ToString(), Progress * 100.0f, *Message), EN2CLogSeverity::Debug);

	// Serialize the notification to JSON
	FString NotificationJson = FJsonRpcUtils::SerializeNotification(ProgressNotification);
	if (!NotificationJson.IsEmpty())
	{
		// Push the progress notification to the SSE stream
		std::string SseMessage = NodeToCodeSseServer::FormatSseMessage(TEXT("progress"), NotificationJson);
		NodeToCodeSseServer::PushFormattedSseEventToClient(TaskId, SseMessage);
	}
	else
	{
		FN2CLogger::Get().LogError(FString::Printf(TEXT("Failed to serialize progress notification for task %s"), *TaskId.ToString()));
	}
}
