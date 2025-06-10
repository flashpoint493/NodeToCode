// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "MCP/Server/N2CMcpJsonRpcTypes.h"

/**
 * Tracks progress for long-running MCP operations and sends progress notifications.
 * This is especially important for Blueprint analysis and code generation tasks.
 */
class NODETOCODE_API FN2CMcpProgressTracker
{
public:
	/**
	 * Gets the singleton instance of the progress tracker.
	 */
	static FN2CMcpProgressTracker& Get();

	/**
	 * Begins tracking progress for a new operation.
	 * @param ProgressToken Unique token to identify this progress operation
	 * @param RequestId The original request ID that initiated this operation
	 */
	void BeginProgress(const FString& ProgressToken, const FString& RequestId);

	/**
	 * Updates the progress of an ongoing operation.
	 * @param ProgressToken The progress token from BeginProgress
	 * @param Progress Current progress value
	 * @param Total Total expected value (for percentage calculation)
	 * @param Message Optional human-readable status message
	 */
	void UpdateProgress(const FString& ProgressToken, float Progress, float Total, 
	                   const FString& Message = TEXT(""));

	/**
	 * Completes progress tracking for an operation.
	 * @param ProgressToken The progress token to complete
	 */
	void EndProgress(const FString& ProgressToken);

	/**
	 * Checks if a progress token is currently being tracked.
	 * @param ProgressToken The progress token to check
	 * @return true if the token is being tracked
	 */
	bool IsProgressActive(const FString& ProgressToken) const;

	/**
	 * Gets all active progress tokens.
	 * @return Array of active progress tokens
	 */
	TArray<FString> GetActiveProgressTokens() const;

private:
	// Private constructor for singleton
	FN2CMcpProgressTracker() = default;

	// Non-copyable
	FN2CMcpProgressTracker(const FN2CMcpProgressTracker&) = delete;
	FN2CMcpProgressTracker& operator=(const FN2CMcpProgressTracker&) = delete;

	/**
	 * Internal structure to track progress state.
	 */
	struct FProgressEntry
	{
		FString RequestId;
		float LastProgress;
		float Total;
		FDateTime LastUpdate;
		FString LastMessage;
	};

	/**
	 * Sends a progress notification to connected clients.
	 * @param ProgressToken The progress token
	 * @param Progress Current progress value
	 * @param Total Total expected value
	 * @param Message Optional status message
	 */
	void SendProgressNotification(const FString& ProgressToken, 
	                             float Progress, float Total, 
	                             const FString& Message);

	/** Map of progress tokens to their tracking entries */
	TMap<FString, FProgressEntry> ActiveProgress;

	/** Thread safety for progress tracking */
	mutable FCriticalSection ProgressLock;
};