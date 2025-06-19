// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#include "MCP/Async/N2CTranslateBlueprintAsyncTask.h"
#include "Core/N2CEditorIntegration.h"
#include "MCP/Utils/N2CMcpArgumentParser.h"
#include "Utils/N2CLogger.h"
#include "Async/Async.h" // For AsyncTask TPromise/TFuture
#include "HAL/PlatformProcess.h" // For FEvent

FN2CTranslateBlueprintAsyncTask::FN2CTranslateBlueprintAsyncTask(const FGuid& TaskId, const FString& ProgressToken, const TSharedPtr<FJsonObject>& Arguments)
    : FN2CToolAsyncTaskBase(TaskId, ProgressToken, TEXT("translate-focused-blueprint"), Arguments)
{
}

void FN2CTranslateBlueprintAsyncTask::Execute()
{
    if (CheckCancellationAndReport()) return;
    ReportProgress(0.05f, TEXT("Preparing Blueprint data..."));

    // FN2CEditorIntegration::TranslateFocusedBlueprintAsync handles its own GameThread dispatches for Blueprint data collection.
    // So, we can call it directly from this worker thread. The OnComplete lambda will be called back on whatever thread
    // the LLM response is processed on (likely an HTTP thread or another worker thread).

    FN2CMcpArgumentParser ArgParser(Arguments);
    FString ProviderId = ArgParser.GetOptionalString(TEXT("provider"));
    FString ModelId = ArgParser.GetOptionalString(TEXT("model"));
    FString LanguageId = ArgParser.GetOptionalString(TEXT("language"));

    FEvent* LLMCompleteEvent = FPlatformProcess::GetSynchEventFromPool(false); // false for auto-reset
    FMcpToolCallResult FinalLLMResult;
    bool bLLMOperationCompleted = false;

    ReportProgress(0.1f, TEXT("Sending translation request to LLM..."));

    FN2CEditorIntegration::Get().TranslateFocusedBlueprintAsync(
        ProviderId, ModelId, LanguageId,
        FOnLLMResponseReceived::CreateLambda([&FinalLLMResult, LLMCompleteEvent, &bLLMOperationCompleted](const FString& LLMResponse)
        {
            if (LLMResponse.StartsWith(TEXT("{\"error\"")))
            {
                FinalLLMResult = FMcpToolCallResult::CreateErrorResult(LLMResponse);
            }
            else
            {
                FinalLLMResult = FMcpToolCallResult::CreateTextResult(LLMResponse);
            }
            bLLMOperationCompleted = true;
            LLMCompleteEvent->Trigger();
        })
    );

    // Wait for LLM completion or cancellation
    float WaitProgress = 0.1f; // Initial progress after BP data prep
    const float MaxWaitTimeSeconds = 3600.0f; // Max wait time (e.g., 1 hour)
    const float StartTime = FPlatformTime::Seconds();
    const float ProgressUpdateIntervalSeconds = 0.2f; // How often we check/update progress
    const float TotalProgressSpanForWait = 0.85f; // Progress from 0.1 (initial) to 0.95 (before completion)

    // Calculate increment per cycle to make progress span MaxWaitTimeSeconds
    // Ensure CyclesInMaxWait is not zero to prevent division by zero, though MaxWaitTimeSeconds is large.
    const float CyclesInMaxWait = (MaxWaitTimeSeconds > 0 && ProgressUpdateIntervalSeconds > 0) ? (MaxWaitTimeSeconds / ProgressUpdateIntervalSeconds) : 1.0f;
    const float IncrementPerCycle = TotalProgressSpanForWait / CyclesInMaxWait;

    while (!bLLMOperationCompleted)
    {
        if (CheckCancellationAndReport())
        {
            FPlatformProcess::ReturnSynchEventToPool(LLMCompleteEvent);
            return;
        }

        if ((FPlatformTime::Seconds() - StartTime) > MaxWaitTimeSeconds)
        {
            FinalLLMResult = FMcpToolCallResult::CreateErrorResult(TEXT("LLM request timed out."));
            ReportComplete(FinalLLMResult);
            FPlatformProcess::ReturnSynchEventToPool(LLMCompleteEvent);
            return;
        }
        
        if (LLMCompleteEvent->Wait(FTimespan::FromSeconds(ProgressUpdateIntervalSeconds))) // Check with defined interval
        {
            // Event was triggered, bLLMOperationCompleted should be true. Loop will exit.
        }

        // Increment progress based on the calculated per-cycle increment
        WaitProgress = FMath::Min(WaitProgress + IncrementPerCycle, 0.95f); 
        ReportProgress(WaitProgress, TEXT("Waiting for LLM response..."));
    }
    
    FPlatformProcess::ReturnSynchEventToPool(LLMCompleteEvent);

    ReportProgress(1.0f, TEXT("Translation received."));
    ReportComplete(FinalLLMResult);
}
