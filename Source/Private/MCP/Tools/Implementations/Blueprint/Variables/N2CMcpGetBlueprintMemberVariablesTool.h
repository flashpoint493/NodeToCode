// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/Tools/N2CMcpToolBase.h"

/**
 * @class FN2CMcpGetBlueprintMemberVariablesTool
 * @brief MCP tool for retrieving all member variables from the currently focused Blueprint
 * 
 * This tool provides functionality to extract information about all member variables
 * defined in a Blueprint, including their types, categories, properties, and metadata.
 */
class FN2CMcpGetBlueprintMemberVariablesTool : public FN2CMcpToolBase
{
public:
    virtual FMcpToolDefinition GetDefinition() const override;
    virtual FMcpToolCallResult Execute(const TSharedPtr<FJsonObject>& Arguments) override;
    virtual bool RequiresGameThread() const override { return true; }

private:
    /**
     * Builds a JSON object containing information about a Blueprint variable
     * @param VarDesc The variable description from the Blueprint
     * @param VariableName The name of the variable
     * @return JSON object with variable details
     */
    TSharedPtr<FJsonObject> BuildVariableInfo(const struct FBPVariableDescription& VarDesc, FName VariableName) const;
    
    /**
     * Extracts metadata values for a variable
     * @param Blueprint The Blueprint containing the variable
     * @param VariableName The name of the variable
     * @return JSON object containing metadata key-value pairs
     */
    TSharedPtr<FJsonObject> ExtractVariableMetadata(const class UBlueprint* Blueprint, FName VariableName) const;
    
    /**
     * Converts pin type information to a JSON object
     * @param PinType The pin type to convert
     * @return JSON object with type information
     */
    TSharedPtr<FJsonObject> PinTypeToJson(const struct FEdGraphPinType& PinType) const;
};