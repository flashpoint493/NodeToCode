// Copyright (c) 2024 Protospatial. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/Tools/N2CMcpToolBase.h"
#include "Engine/Blueprint.h"

class FCompilerResultsLog;
enum EBlueprintStatus : int;

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
     * Builds the compilation result JSON object
     * @param Blueprint The compiled Blueprint
     * @param CompilerResults The compilation results log
     * @param PreCompileStatus The Blueprint status before compilation
     * @param CompilationTime Time taken to compile in seconds
     * @return JSON object with compilation results
     */
    TSharedPtr<FJsonObject> BuildCompilationResult(
        UBlueprint* Blueprint,
        const FCompilerResultsLog& CompilerResults,
        EBlueprintStatus PreCompileStatus,
        float CompilationTime
    );
    
    /**
     * Extracts compiler messages from the results log
     * @param CompilerResults The compilation results log
     * @return Array of JSON values representing messages
     */
    TArray<TSharedPtr<FJsonValue>> ExtractCompilerMessages(
        const FCompilerResultsLog& CompilerResults
    );
    
    /**
     * Converts Blueprint status enum to string representation
     * @param Status The Blueprint status
     * @return String representation of the status
     */
    FString GetStatusString(EBlueprintStatus Status);
};