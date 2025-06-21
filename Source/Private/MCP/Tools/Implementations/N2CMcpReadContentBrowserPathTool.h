// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/Tools/N2CMcpToolBase.h"

/**
 * @class FN2CMcpReadContentBrowserPathTool
 * @brief MCP tool for reading content browser paths and returning assets/folders
 * 
 * This tool provides agents with visibility into the project's asset structure,
 * supporting pagination, filtering by type and name, and optional browser sync.
 */
class FN2CMcpReadContentBrowserPathTool : public FN2CMcpToolBase
{
public:
	virtual FMcpToolDefinition GetDefinition() const override;
	virtual FMcpToolCallResult Execute(const TSharedPtr<FJsonObject>& Arguments) override;
	virtual bool RequiresGameThread() const override { return true; }
	
private:
	/**
	 * Parse and validate tool arguments
	 */
	bool ParseArguments(const TSharedPtr<FJsonObject>& Arguments,
		FString& OutPath,
		int32& OutPage,
		int32& OutPageSize,
		FString& OutFilterType,
		FString& OutFilterName,
		bool& OutSyncBrowser) const;
	
	/**
	 * Build the result JSON object
	 */
	TSharedPtr<FJsonObject> BuildResultJson(
		const TArray<struct FContentBrowserItem>& Items,
		int32 StartIndex,
		int32 EndIndex,
		int32 TotalCount,
		int32 Page,
		int32 PageSize,
		bool bHasMore) const;
};