// Copyright (c) 2024 Protospatial. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/Tools/N2CMcpToolBase.h"
#include "Engine/Blueprint.h"

/**
 * MCP tool for compiling Blueprints
 * Provides the same functionality as the Compile button in the Blueprint editor
 */
class FN2CMcpCompileBlueprint : public FN2CMcpToolBase
{
public:
    virtual FMcpToolDefinition GetDefinition() const override;
    virtual FMcpToolCallResult Execute(const TSharedPtr<FJsonObject>& Arguments) override;
    virtual bool RequiresGameThread() const override { return true; }
    
private:
    /**
     * Converts Blueprint status enum to string representation
     * @param Status The Blueprint status
     * @return String representation of the status
     */
    FString GetStatusString(EBlueprintStatus Status);
};