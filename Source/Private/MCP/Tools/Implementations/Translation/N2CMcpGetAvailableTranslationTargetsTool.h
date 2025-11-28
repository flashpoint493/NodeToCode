// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/Tools/N2CMcpToolBase.h"
#include "Code Editor/Models/N2CCodeLanguage.h"

/**
 * MCP tool that returns the list of available programming languages that NodeToCode can translate Blueprints into.
 * This tool queries the EN2CCodeLanguage enum values and returns them in a format suitable for AI assistants.
 */
class FN2CMcpGetAvailableTranslationTargetsTool : public FN2CMcpToolBase
{
public:
    // IN2CMcpTool interface
    virtual FMcpToolDefinition GetDefinition() const override;
    virtual FMcpToolCallResult Execute(const TSharedPtr<FJsonObject>& Arguments) override;
    
    // This tool doesn't require Game Thread since it only accesses static enum data and settings
    virtual bool RequiresGameThread() const override { return false; }
    
private:
    /**
     * Get a human-readable description for a language
     * @param Language The language enum value
     * @return Description string
     */
    static FString GetLanguageDescription(EN2CCodeLanguage Language);
    
    /**
     * Get the category for a language (compiled, scripted, etc.)
     * @param Language The language enum value
     * @return Category string
     */
    static FString GetLanguageCategory(EN2CCodeLanguage Language);
    
    /**
     * Get the file extensions associated with a language
     * @param Language The language enum value
     * @return Array of file extensions (e.g., [".h", ".cpp"])
     */
    static TArray<FString> GetLanguageFileExtensions(EN2CCodeLanguage Language);
    
    /**
     * Get additional features or notes about a language
     * @param Language The language enum value
     * @return Features/notes string
     */
    static FString GetLanguageFeatures(EN2CCodeLanguage Language);
};