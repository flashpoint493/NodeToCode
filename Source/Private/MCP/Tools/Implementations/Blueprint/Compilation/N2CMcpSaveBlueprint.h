// Copyright (c) 2025 Protospatial. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/Tools/N2CMcpToolBase.h"

class UBlueprint;

/**
 * MCP tool for saving Blueprints to disk
 * Provides functionality to save a Blueprint asset, marking the package as saved
 */
class FN2CMcpSaveBlueprint : public FN2CMcpToolBase
{
public:
    virtual FMcpToolDefinition GetDefinition() const override;
    virtual FMcpToolCallResult Execute(const TSharedPtr<FJsonObject>& Arguments) override;
    virtual bool RequiresGameThread() const override { return true; }
    
private:
    /**
     * Saves the Blueprint asset to disk
     * @param Blueprint The Blueprint to save
     * @param OutError Error message if save fails
     * @return True if save succeeded, false otherwise
     */
    bool SaveBlueprintAsset(UBlueprint* Blueprint, FString& OutError);
    
    /**
     * Builds the save result JSON object
     * @param Blueprint The saved Blueprint
     * @param bSuccess Whether the save succeeded
     * @param Message Status message
     * @return JSON object with save results
     */
    TSharedPtr<FJsonObject> BuildSaveResult(
        UBlueprint* Blueprint,
        bool bSuccess,
        const FString& Message
    );
};