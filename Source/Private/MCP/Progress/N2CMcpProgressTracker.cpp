// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#include "MCP/Progress/N2CMcpProgressTracker.h"
#include "MCP/Server/N2CMcpHttpServerManager.h"
#include "Utils/N2CLogger.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

FN2CMcpProgressTracker& FN2CMcpProgressTracker::Get()
{
	static FN2CMcpProgressTracker Instance;
	return Instance;
}

void FN2CMcpProgressTracker::BeginProgress(const FString& ProgressToken, const FString& RequestId)
{
	FScopeLock Lock(&ProgressLock);
	
	if (ActiveProgress.Contains(ProgressToken))
	{
		FN2CLogger::Get().LogWarning(FString::Printf(TEXT("Progress token already exists: %s"), *ProgressToken));
		return;
	}
	
	FProgressEntry Entry;
	Entry.RequestId = RequestId;
	Entry.LastProgress = 0.0f;
	Entry.Total = 100.0f;  // Default to percentage
	Entry.LastUpdate = FDateTime::Now();
	Entry.LastMessage = TEXT("Operation started");
	
	ActiveProgress.Add(ProgressToken, Entry);
	
	FN2CLogger::Get().Log(FString::Printf(TEXT("Started progress tracking: %s for request: %s"), 
		*ProgressToken, *RequestId), EN2CLogSeverity::Debug);
	
	// Send initial progress notification
	SendProgressNotification(ProgressToken, 0.0f, Entry.Total, Entry.LastMessage);
}

void FN2CMcpProgressTracker::UpdateProgress(const FString& ProgressToken, float Progress, float Total, 
                                           const FString& Message)
{
	FScopeLock Lock(&ProgressLock);
	
	FProgressEntry* Entry = ActiveProgress.Find(ProgressToken);
	if (!Entry)
	{
		FN2CLogger::Get().LogWarning(FString::Printf(TEXT("Progress token not found: %s"), *ProgressToken));
		return;
	}
	
	// Update entry
	Entry->LastProgress = Progress;
	Entry->Total = Total;
	Entry->LastUpdate = FDateTime::Now();
	if (!Message.IsEmpty())
	{
		Entry->LastMessage = Message;
	}
	
	// Send progress notification
	SendProgressNotification(ProgressToken, Progress, Total, Entry->LastMessage);
}

void FN2CMcpProgressTracker::EndProgress(const FString& ProgressToken)
{
	FScopeLock Lock(&ProgressLock);
	
	FProgressEntry* Entry = ActiveProgress.Find(ProgressToken);
	if (!Entry)
	{
		FN2CLogger::Get().LogWarning(FString::Printf(TEXT("Progress token not found: %s"), *ProgressToken));
		return;
	}
	
	// Send final progress notification (100% complete)
	SendProgressNotification(ProgressToken, Entry->Total, Entry->Total, TEXT("Operation completed"));
	
	// Remove from tracking
	ActiveProgress.Remove(ProgressToken);
	
	FN2CLogger::Get().Log(FString::Printf(TEXT("Ended progress tracking: %s"), *ProgressToken), EN2CLogSeverity::Debug);
}

bool FN2CMcpProgressTracker::IsProgressActive(const FString& ProgressToken) const
{
	FScopeLock Lock(&ProgressLock);
	return ActiveProgress.Contains(ProgressToken);
}

TArray<FString> FN2CMcpProgressTracker::GetActiveProgressTokens() const
{
	FScopeLock Lock(&ProgressLock);
	
	TArray<FString> Tokens;
	ActiveProgress.GetKeys(Tokens);
	return Tokens;
}

void FN2CMcpProgressTracker::SendProgressNotification(const FString& ProgressToken, 
                                                      float Progress, float Total, 
                                                      const FString& Message)
{
	// Build progress notification per MCP spec
	FJsonRpcNotification Notification;
	Notification.JsonRpc = TEXT("2.0");
	Notification.Method = TEXT("notifications/progress");
	
	// Build params object
	TSharedPtr<FJsonObject> ParamsObject = MakeShareable(new FJsonObject);
	ParamsObject->SetStringField(TEXT("progressToken"), ProgressToken);
	
	// Calculate percentage
	float Percentage = (Total > 0) ? (Progress / Total) * 100.0f : 0.0f;
	ParamsObject->SetNumberField(TEXT("progress"), Percentage);
	
	if (!Message.IsEmpty())
	{
		ParamsObject->SetStringField(TEXT("message"), Message);
	}
	
	// Add timestamp
	ParamsObject->SetStringField(TEXT("timestamp"), FDateTime::Now().ToIso8601());
	
	Notification.Params = MakeShareable(new FJsonValueObject(ParamsObject));
	
	// Broadcast the progress notification to all connected clients
	FN2CMcpHttpServerManager::Get().BroadcastNotification(Notification);
	
	FN2CLogger::Get().Log(FString::Printf(TEXT("Progress: %s - %.1f%% - %s"), 
		*ProgressToken, Percentage, *Message), EN2CLogSeverity::Debug);
}