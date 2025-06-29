// Copyright (c) 2024 Protospatial. All Rights Reserved.

#include "N2CMcpCompileBlueprint.h"
#include "MCP/Tools/N2CMcpToolRegistry.h"
#include "MCP/Utils/N2CMcpBlueprintUtils.h"
#include "MCP/Utils/N2CMcpArgumentParser.h"
#include "Utils/N2CLogger.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// Auto-register this tool
REGISTER_MCP_TOOL(FN2CMcpCompileBlueprint)

FMcpToolDefinition FN2CMcpCompileBlueprint::GetDefinition() const
{
    FMcpToolDefinition Definition(
        TEXT("compile-blueprint"),
        TEXT("Compile a Blueprint and return compilation results including errors and warnings"),
        TEXT("Blueprint Compilation")
    );
    
    // Define input schema
    TMap<FString, FString> Properties;
    Properties.Add(TEXT("blueprintPath"), TEXT("string"));
    Properties.Add(TEXT("skipGarbageCollection"), TEXT("boolean"));
    
    TArray<FString> Required; // No required fields - uses focused blueprint if path not provided
    
    Definition.InputSchema = BuildInputSchema(Properties, Required);
    
    return Definition;
}

FMcpToolCallResult FN2CMcpCompileBlueprint::Execute(const TSharedPtr<FJsonObject>& Arguments)
{
    return ExecuteOnGameThread([this, Arguments]() -> FMcpToolCallResult
    {
        // Parse arguments
        FN2CMcpArgumentParser Parser(Arguments);
        FString BlueprintPath = Parser.GetOptionalString(TEXT("blueprintPath"));
        bool bSkipGarbageCollection = Parser.GetOptionalBool(TEXT("skipGarbageCollection"), true); // Default to true for performance
        
        // Resolve Blueprint
        FString ErrorMessage;
        UBlueprint* Blueprint = FN2CMcpBlueprintUtils::ResolveBlueprint(BlueprintPath, ErrorMessage);
        
        if (!Blueprint)
        {
            return FMcpToolCallResult::CreateErrorResult(ErrorMessage);
        }
        
        // Record pre-compilation state
        EBlueprintStatus PreCompileStatus = Blueprint->Status;
        
        // Use the utility method to compile with detailed messages
        int32 ErrorCount = 0;
        int32 WarningCount = 0;
        float CompilationTime = 0.0f;
        TArray<TSharedPtr<FN2CCompilerMessage>> CompilerMessages;
        bool bCompileSuccess = FN2CMcpBlueprintUtils::CompileBlueprint(
            Blueprint, bSkipGarbageCollection, ErrorCount, WarningCount, CompilationTime, &CompilerMessages);
        
        // Build result JSON
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
        
        // Basic info
        Result->SetBoolField(TEXT("success"), bCompileSuccess);
        Result->SetStringField(TEXT("blueprintName"), Blueprint->GetName());
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        
        // Compilation status
        TSharedPtr<FJsonObject> StatusObj = MakeShareable(new FJsonObject);
        StatusObj->SetStringField(TEXT("previousStatus"), GetStatusString(PreCompileStatus));
        StatusObj->SetStringField(TEXT("currentStatus"), GetStatusString(Blueprint->Status));
        StatusObj->SetNumberField(TEXT("statusCode"), static_cast<int32>(Blueprint->Status));
        Result->SetObjectField(TEXT("compilationStatus"), StatusObj);
        
        // Compilation time
        Result->SetNumberField(TEXT("compilationTime"), CompilationTime);
        
        // Compilation results
        TSharedPtr<FJsonObject> ResultsObj = MakeShareable(new FJsonObject);
        ResultsObj->SetNumberField(TEXT("errorCount"), ErrorCount);
        ResultsObj->SetNumberField(TEXT("warningCount"), WarningCount);
        
        // Count notes (info messages)
        int32 NoteCount = 0;
        for (const TSharedPtr<FN2CCompilerMessage>& Message : CompilerMessages)
        {
            if (Message->Severity == TEXT("Note"))
            {
                NoteCount++;
            }
        }
        ResultsObj->SetNumberField(TEXT("noteCount"), NoteCount);
        
        // Add detailed messages
        TArray<TSharedPtr<FJsonValue>> MessagesArray;
        for (const TSharedPtr<FN2CCompilerMessage>& Message : CompilerMessages)
        {
            TSharedPtr<FJsonObject> MessageObj = MakeShareable(new FJsonObject);
            MessageObj->SetStringField(TEXT("severity"), Message->Severity);
            MessageObj->SetStringField(TEXT("message"), Message->Message);
            MessagesArray.Add(MakeShareable(new FJsonValueObject(MessageObj)));
        }
        ResultsObj->SetArrayField(TEXT("messages"), MessagesArray);
        Result->SetObjectField(TEXT("results"), ResultsObj);
        
        // Summary message
        FString SummaryMessage;
        if (ErrorCount > 0)
        {
            SummaryMessage = FString::Printf(TEXT("Blueprint compilation failed with %d error(s) and %d warning(s)"),
                ErrorCount, WarningCount);
        }
        else if (WarningCount > 0)
        {
            SummaryMessage = FString::Printf(TEXT("Blueprint compiled successfully with %d warning(s)"),
                WarningCount);
        }
        else
        {
            SummaryMessage = TEXT("Blueprint compiled successfully");
        }
        Result->SetStringField(TEXT("message"), SummaryMessage);
        
        // Convert JSON object to string for text result
        FString OutputString;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
        FJsonSerializer::Serialize(Result.ToSharedRef(), Writer);
        
        return FMcpToolCallResult::CreateTextResult(OutputString);
    });
}

FString FN2CMcpCompileBlueprint::GetStatusString(EBlueprintStatus Status)
{
    switch (Status)
    {
        case BS_Unknown:
            return TEXT("Unknown");
        case BS_Dirty:
            return TEXT("Dirty");
        case BS_Error:
            return TEXT("Error");
        case BS_UpToDate:
            return TEXT("UpToDate");
        case BS_BeingCreated:
            return TEXT("BeingCreated");
        case BS_UpToDateWithWarnings:
            return TEXT("UpToDateWithWarnings");
        default:
            return TEXT("Unknown");
    }
}