// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "MCP/Async/N2CToolAsyncTaskBase.h"
#include "CoreMinimal.h" // For FEvent

class FN2CTranslateBlueprintAsyncTask : public FN2CToolAsyncTaskBase
{
public:
    FN2CTranslateBlueprintAsyncTask(const FGuid& TaskId, const FString& ProgressToken, const TSharedPtr<FJsonObject>& Arguments);
    virtual void Execute() override;
};
