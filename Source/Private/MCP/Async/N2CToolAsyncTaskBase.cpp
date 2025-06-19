// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#include "MCP/Async/N2CToolAsyncTaskBase.h"
#include "Utils/N2CLogger.h"

FN2CToolAsyncTaskBase::FN2CToolAsyncTaskBase(const FGuid& InTaskId, const FString& InProgressToken, const FString& InToolName, const TSharedPtr<FJsonObject>& InArguments)
	: Arguments(InArguments)
	, TaskId(InTaskId)
	, ProgressToken(InProgressToken)
	, ToolName(InToolName)
	, bCancellationRequested(false)
	, bHasCompleted(false)
{
}

void FN2CToolAsyncTaskBase::RequestCancel()
{
	bCancellationRequested = true;
	FN2CLogger::Get().Log(FString::Printf(TEXT("Cancellation requested for async task %s (Tool: %s)"), *TaskId.ToString(), *ToolName), EN2CLogSeverity::Info);
}

bool FN2CToolAsyncTaskBase::IsCancellationRequested() const
{
	return bCancellationRequested;
}

void FN2CToolAsyncTaskBase::SetProgressDelegate(const FN2CAsyncTaskProgressDelegate& InDelegate)
{
	OnProgress = InDelegate;
}

void FN2CToolAsyncTaskBase::SetCompleteDelegate(const FN2CAsyncTaskCompleteDelegate& InDelegate)
{
	OnComplete = InDelegate;
}

void FN2CToolAsyncTaskBase::ReportProgress(float Progress, const FString& Message)
{
	if (OnProgress.IsBound())
	{
		// Execute on game thread if needed
		if (IsInGameThread())
		{
			OnProgress.Execute(Progress, Message);
		}
		else
		{
			AsyncTask(ENamedThreads::GameThread, [this, Progress, Message]()
			{
				if (OnProgress.IsBound())
				{
					OnProgress.Execute(Progress, Message);
				}
			});
		}
	}
}

void FN2CToolAsyncTaskBase::ReportComplete(const FMcpToolCallResult& Result)
{
	// Ensure we only complete once
	if (bHasCompleted.AtomicSet(true))
	{
		FN2CLogger::Get().LogWarning(FString::Printf(TEXT("Async task %s attempted to complete multiple times"), *TaskId.ToString()));
		return;
	}

	if (OnComplete.IsBound())
	{
		// Execute on game thread if needed
		if (IsInGameThread())
		{
			OnComplete.Execute(Result);
		}
		else
		{
			AsyncTask(ENamedThreads::GameThread, [this, Result]()
			{
				if (OnComplete.IsBound())
				{
					OnComplete.Execute(Result);
				}
			});
		}
	}

	FN2CLogger::Get().Log(FString::Printf(TEXT("Async task %s completed (Tool: %s, IsError: %s)"), 
		*TaskId.ToString(), *ToolName, Result.bIsError ? TEXT("Yes") : TEXT("No")), EN2CLogSeverity::Info);
}

bool FN2CToolAsyncTaskBase::CheckCancellationAndReport()
{
	if (IsCancellationRequested())
	{
		FMcpToolCallResult CancelledResult = FMcpToolCallResult::CreateErrorResult(TEXT("Task was cancelled"));
		ReportComplete(CancelledResult);
		return true;
	}
	return false;
}