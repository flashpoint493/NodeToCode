// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "N2CMcpToolTypes.generated.h"

/**
 * MCP Tool Definition
 * Represents metadata for a single MCP tool
 */
USTRUCT()
struct NODETOCODE_API FMcpToolDefinition
{
	GENERATED_BODY()

	/** The unique name of the tool */
	UPROPERTY()
	FString Name;

	/** Optional description of what the tool does */
	UPROPERTY()
	FString Description;

	/** The category this tool belongs to */
	UPROPERTY()
	FString Category;

	/** JSON Schema defining the tool's input parameters */
	TSharedPtr<FJsonObject> InputSchema;

	/** Optional annotations for the tool (e.g., readOnlyHint) */
	TSharedPtr<FJsonObject> Annotations;

	/** Whether this tool is long-running and should use async execution */
	UPROPERTY()
	bool bIsLongRunning = false;

	FMcpToolDefinition()
	{
	}

	FMcpToolDefinition(const FString& InName, const FString& InDescription = TEXT(""), const FString& InCategory = TEXT(""))
		: Name(InName)
		, Description(InDescription)
		, Category(InCategory)
	{
	}

	/** Convert to JSON object for MCP protocol */
	TSharedPtr<FJsonObject> ToJson() const;

	/** Create from JSON object */
	static FMcpToolDefinition FromJson(const TSharedPtr<FJsonObject>& JsonObject);
};

/**
 * MCP Tool Call Result
 * Represents the result of invoking an MCP tool
 */
USTRUCT()
struct NODETOCODE_API FMcpToolCallResult
{
	GENERATED_BODY()

	/** Array of content items (for now, just text content) */
	TArray<TSharedPtr<FJsonObject>> Content;

	/** Whether this result indicates an error */
	UPROPERTY()
	bool bIsError = false;

	FMcpToolCallResult()
	{
	}

	/** Create a success result with text content */
	static FMcpToolCallResult CreateTextResult(const FString& Text);

	/** Create an error result with error message */
	static FMcpToolCallResult CreateErrorResult(const FString& ErrorMessage);

	/** Convert to JSON object for MCP protocol */
	TSharedPtr<FJsonObject> ToJson() const;
};

/**
 * Delegate type for MCP tool handlers
 */
DECLARE_DELEGATE_RetVal_OneParam(FMcpToolCallResult, FMcpToolHandlerDelegate, const TSharedPtr<FJsonObject>& /* Args */);