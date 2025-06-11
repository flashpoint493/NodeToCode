// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/Prompts/N2CMcpPromptTypes.h"
#include "Async/Async.h"

/**
 * MCP Prompt for generating code from the current Blueprint
 */
class NODETOCODE_API FN2CMcpCodeGenerationPrompt : public IN2CMcpPrompt
{
public:
	virtual FMcpPromptDefinition GetDefinition() const override;
	virtual FMcpPromptResult GetPrompt(const FMcpPromptArguments& Arguments) override;
	virtual bool RequiresGameThread() const override { return true; }

protected:
	/**
	 * Helper to execute code on the game thread
	 */
	template<typename ResultType>
	static ResultType ExecuteOnGameThread(TFunction<ResultType()> Function)
	{
		if (IsInGameThread())
		{
			return Function();
		}
		
		TPromise<ResultType> Promise;
		TFuture<ResultType> Future = Promise.GetFuture();
		
		AsyncTask(ENamedThreads::GameThread, [Function = MoveTemp(Function), Promise = MoveTemp(Promise)]() mutable
		{
			Promise.SetValue(Function());
		});
		
		return Future.Get();
	}
};

/**
 * MCP Prompt for analyzing a Blueprint
 */
class NODETOCODE_API FN2CMcpBlueprintAnalysisPrompt : public IN2CMcpPrompt
{
public:
	virtual FMcpPromptDefinition GetDefinition() const override;
	virtual FMcpPromptResult GetPrompt(const FMcpPromptArguments& Arguments) override;
	virtual bool RequiresGameThread() const override { return true; }

protected:
	template<typename ResultType>
	static ResultType ExecuteOnGameThread(TFunction<ResultType()> Function)
	{
		if (IsInGameThread())
		{
			return Function();
		}
		
		TPromise<ResultType> Promise;
		TFuture<ResultType> Future = Promise.GetFuture();
		
		AsyncTask(ENamedThreads::GameThread, [Function = MoveTemp(Function), Promise = MoveTemp(Promise)]() mutable
		{
			Promise.SetValue(Function());
		});
		
		return Future.Get();
	}
};

/**
 * MCP Prompt for refactoring Blueprint code
 */
class NODETOCODE_API FN2CMcpRefactorPrompt : public IN2CMcpPrompt
{
public:
	virtual FMcpPromptDefinition GetDefinition() const override;
	virtual FMcpPromptResult GetPrompt(const FMcpPromptArguments& Arguments) override;
	virtual bool RequiresGameThread() const override { return true; }

protected:
	template<typename ResultType>
	static ResultType ExecuteOnGameThread(TFunction<ResultType()> Function)
	{
		if (IsInGameThread())
		{
			return Function();
		}
		
		TPromise<ResultType> Promise;
		TFuture<ResultType> Future = Promise.GetFuture();
		
		AsyncTask(ENamedThreads::GameThread, [Function = MoveTemp(Function), Promise = MoveTemp(Promise)]() mutable
		{
			Promise.SetValue(Function());
		});
		
		return Future.Get();
	}
};