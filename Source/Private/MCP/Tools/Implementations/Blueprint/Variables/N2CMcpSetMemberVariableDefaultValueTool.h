// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/Tools/N2CMcpToolBase.h"

/**
 * MCP tool for setting the default value of a member variable in a Blueprint
 * This modifies the FBPVariableDescription's DefaultValue property, similar to
 * editing the default value in the Details panel of the Blueprint editor.
 */
class FN2CMcpSetMemberVariableDefaultValueTool : public FN2CMcpToolBase
{
public:
    virtual FMcpToolDefinition GetDefinition() const override;
    virtual FMcpToolCallResult Execute(const TSharedPtr<FJsonObject>& Arguments) override;
    virtual bool RequiresGameThread() const override { return true; }
    
private:
    /**
     * Finds a variable by name in the Blueprint
     * @param Blueprint The Blueprint to search in
     * @param VariableName The name of the variable to find
     * @return Pointer to the variable description, or nullptr if not found
     */
    FBPVariableDescription* FindVariable(UBlueprint* Blueprint, const FString& VariableName) const;
    
    /**
     * Validates that the provided default value is compatible with the variable's type
     * @param VarDesc The variable description
     * @param DefaultValue The default value to validate
     * @param OutError Error message if validation fails
     * @return true if the default value is valid, false otherwise
     */
    bool ValidateDefaultValue(const FBPVariableDescription& VarDesc, const FString& DefaultValue, FString& OutError) const;
    
    /**
     * Applies the new default value to the variable
     * @param Blueprint The Blueprint containing the variable
     * @param VarDesc The variable description to modify
     * @param DefaultValue The new default value
     * @return true if successful, false otherwise
     */
    bool ApplyDefaultValue(UBlueprint* Blueprint, FBPVariableDescription& VarDesc, const FString& DefaultValue) const;
    
    /**
     * Builds the success result JSON
     * @param Blueprint The Blueprint that was modified
     * @param VariableName The name of the variable that was modified
     * @param VarDesc The variable description
     * @param OldDefaultValue The previous default value
     * @param NewDefaultValue The new default value
     * @return JSON object with result data
     */
    TSharedPtr<FJsonObject> BuildSuccessResult(
        const UBlueprint* Blueprint,
        const FString& VariableName,
        const FBPVariableDescription& VarDesc,
        const FString& OldDefaultValue,
        const FString& NewDefaultValue) const;
};