// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#include "MCP/Tools/N2CMcpToolBase.h"
#include "Utils/N2CLogger.h"
#include "Async/Async.h"
#include "Async/TaskGraphInterfaces.h"
#include "Dom/JsonObject.h"

FMcpToolCallResult FN2CMcpToolBase::ExecuteOnGameThread(TFunction<FMcpToolCallResult()> Logic, float TimeoutSeconds)
{
	// Check if we're already on the Game Thread
	if (IsInGameThread())
	{
		FN2CLogger::Get().Log(TEXT("MCP Tool: Already on Game Thread, executing directly"), EN2CLogSeverity::Debug);
		return Logic();
	}
	
	// We're on a worker thread, need to dispatch to Game Thread
	FN2CLogger::Get().Log(TEXT("MCP Tool: On worker thread, dispatching to Game Thread"), EN2CLogSeverity::Debug);
	
	// Use a simple event for synchronization
	FEvent* TaskEvent = FPlatformProcess::GetSynchEventFromPool();
	FMcpToolCallResult Result;
	
	// Create a task to run on the Game Thread
	FGraphEventRef Task = FFunctionGraphTask::CreateAndDispatchWhenReady([&Result, &Logic, TaskEvent]()
	{
		FN2CLogger::Get().Log(TEXT("MCP Tool: Game Thread task executing"), EN2CLogSeverity::Debug);
		
		Result = Logic();
		
		FN2CLogger::Get().Log(FString::Printf(TEXT("MCP Tool: Game Thread task completed. Success: %s"), 
			Result.bIsError ? TEXT("No") : TEXT("Yes")), EN2CLogSeverity::Debug);
		
		// Signal completion
		TaskEvent->Trigger();
	}, TStatId(), nullptr, ENamedThreads::GameThread);
	
	// Wait for completion with timeout
	const uint32 TimeoutMs = static_cast<uint32>(TimeoutSeconds * 1000.0f);
	bool bCompleted = TaskEvent->Wait(TimeoutMs);
	
	// Return event to pool
	FPlatformProcess::ReturnSynchEventToPool(TaskEvent);
	
	if (!bCompleted)
	{
		FN2CLogger::Get().LogError(TEXT("MCP Tool timed out waiting for Game Thread"));
		return FMcpToolCallResult::CreateErrorResult(TEXT("Timeout waiting for Game Thread execution."));
	}
	
	return Result;
}

TSharedPtr<FJsonObject> FN2CMcpToolBase::BuildInputSchema(
	const TMap<FString, FString>& Properties,
	const TArray<FString>& Required)
{
	TSharedPtr<FJsonObject> Schema = MakeShareable(new FJsonObject);
	Schema->SetStringField(TEXT("type"), TEXT("object"));
	
	// Build properties object
	TSharedPtr<FJsonObject> PropertiesObj = MakeShareable(new FJsonObject);
	for (const auto& Prop : Properties)
	{
		TSharedPtr<FJsonObject> PropSchema = MakeShareable(new FJsonObject);
		PropSchema->SetStringField(TEXT("type"), Prop.Value);
		PropertiesObj->SetObjectField(Prop.Key, PropSchema);
	}
	Schema->SetObjectField(TEXT("properties"), PropertiesObj);
	
	// Add required array if needed
	if (Required.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> RequiredArray;
		for (const FString& ReqProp : Required)
		{
			RequiredArray.Add(MakeShareable(new FJsonValueString(ReqProp)));
		}
		Schema->SetArrayField(TEXT("required"), RequiredArray);
	}
	
	return Schema;
}

TSharedPtr<FJsonObject> FN2CMcpToolBase::BuildEmptyObjectSchema()
{
	TSharedPtr<FJsonObject> Schema = MakeShareable(new FJsonObject);
	Schema->SetStringField(TEXT("type"), TEXT("object"));
	
	// Empty properties object
	TSharedPtr<FJsonObject> Properties = MakeShareable(new FJsonObject);
	Schema->SetObjectField(TEXT("properties"), Properties);
	
	// Empty required array
	TArray<TSharedPtr<FJsonValue>> RequiredArray;
	Schema->SetArrayField(TEXT("required"), RequiredArray);
	
	return Schema;
}

void FN2CMcpToolBase::AddReadOnlyAnnotation(FMcpToolDefinition& Definition)
{
	if (!Definition.Annotations.IsValid())
	{
		Definition.Annotations = MakeShareable(new FJsonObject);
	}
	Definition.Annotations->SetBoolField(TEXT("readOnlyHint"), true);
}