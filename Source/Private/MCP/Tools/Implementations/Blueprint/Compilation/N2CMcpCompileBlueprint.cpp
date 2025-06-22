// Copyright (c) 2024 Protospatial. All Rights Reserved.

#include "N2CMcpCompileBlueprint.h"
#include "MCP/Tools/N2CMcpToolRegistry.h"
#include "MCP/Utils/N2CMcpBlueprintUtils.h"
#include "MCP/Utils/N2CMcpArgumentParser.h"
#include "Utils/N2CLogger.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// Auto-register this tool
REGISTER_MCP_TOOL(FN2CMcpCompileBlueprint)

FMcpToolDefinition FN2CMcpCompileBlueprint::GetDefinition() const
{
    FMcpToolDefinition Definition(
        TEXT("compile-blueprint"),
        TEXT("Compile a Blueprint and return compilation results including errors and warnings")
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
        double StartTime = FPlatformTime::Seconds();
        
        // Create compiler results log
        FCompilerResultsLog CompilerResults;
        CompilerResults.bSilentMode = true; // We'll handle output ourselves
        CompilerResults.bAnnotateMentionedNodes = true;
        CompilerResults.SetSourcePath(Blueprint->GetPathName());
        
        // Begin compilation event
        CompilerResults.BeginEvent(TEXT("MCP Compile"));
        
        // Set compilation flags
        EBlueprintCompileOptions CompileFlags = bSkipGarbageCollection 
            ? EBlueprintCompileOptions::SkipGarbageCollection 
            : EBlueprintCompileOptions::None;
        
        try
        {
            // Compile the Blueprint
            FKismetEditorUtilities::CompileBlueprint(Blueprint, CompileFlags, &CompilerResults);
        }
        catch (...)
        {
            FN2CLogger::Get().LogError(TEXT("FN2CMcpCompileBlueprint"), 
                TEXT("Exception during Blueprint compilation"));
            return FMcpToolCallResult::CreateErrorResult(
                TEXT("COMPILATION_FAILED: Critical error during compilation"));
        }
        
        // End compilation event
        CompilerResults.EndEvent();
        
        // Calculate compilation time
        float CompilationTime = static_cast<float>(FPlatformTime::Seconds() - StartTime);
        
        // Refresh Blueprint action database
        FN2CMcpBlueprintUtils::RefreshBlueprintActionDatabase();
        
        // Build and return result
        TSharedPtr<FJsonObject> Result = BuildCompilationResult(
            Blueprint, CompilerResults, PreCompileStatus, CompilationTime);
        
        // Convert JSON object to string for text result
        FString OutputString;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
        FJsonSerializer::Serialize(Result.ToSharedRef(), Writer);
        
        return FMcpToolCallResult::CreateTextResult(OutputString);
    });
}

TSharedPtr<FJsonObject> FN2CMcpCompileBlueprint::BuildCompilationResult(
    UBlueprint* Blueprint,
    const FCompilerResultsLog& CompilerResults,
    EBlueprintStatus PreCompileStatus,
    float CompilationTime)
{
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    
    // Basic info
    Result->SetBoolField(TEXT("success"), CompilerResults.NumErrors == 0);
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
    ResultsObj->SetNumberField(TEXT("errorCount"), CompilerResults.NumErrors);
    ResultsObj->SetNumberField(TEXT("warningCount"), CompilerResults.NumWarnings);
    
    // Count notes (info messages)
    int32 NoteCount = 0;
    for (const TSharedRef<FTokenizedMessage>& Message : CompilerResults.Messages)
    {
        if (Message->GetSeverity() == EMessageSeverity::Info)
        {
            NoteCount++;
        }
    }
    ResultsObj->SetNumberField(TEXT("noteCount"), NoteCount);
    
    // Extract messages
    ResultsObj->SetArrayField(TEXT("messages"), ExtractCompilerMessages(CompilerResults));
    Result->SetObjectField(TEXT("results"), ResultsObj);
    
    // Summary message
    FString SummaryMessage;
    if (CompilerResults.NumErrors > 0)
    {
        SummaryMessage = FString::Printf(TEXT("Blueprint compilation failed with %d error(s) and %d warning(s)"),
            CompilerResults.NumErrors, CompilerResults.NumWarnings);
    }
    else if (CompilerResults.NumWarnings > 0)
    {
        SummaryMessage = FString::Printf(TEXT("Blueprint compiled successfully with %d warning(s)"),
            CompilerResults.NumWarnings);
    }
    else
    {
        SummaryMessage = TEXT("Blueprint compiled successfully");
    }
    Result->SetStringField(TEXT("message"), SummaryMessage);
    
    return Result;
}

TArray<TSharedPtr<FJsonValue>> FN2CMcpCompileBlueprint::ExtractCompilerMessages(
    const FCompilerResultsLog& CompilerResults)
{
    TArray<TSharedPtr<FJsonValue>> Messages;
    
    for (const TSharedRef<FTokenizedMessage>& Message : CompilerResults.Messages)
    {
        TSharedPtr<FJsonObject> MessageObj = MakeShareable(new FJsonObject);
        
        // Severity
        FString Severity;
        switch (Message->GetSeverity())
        {
            case EMessageSeverity::Error:
                Severity = TEXT("Error");
                break;
            case EMessageSeverity::Warning:
                Severity = TEXT("Warning");
                break;
            case EMessageSeverity::Info:
                Severity = TEXT("Note");
                break;
            default:
                Severity = TEXT("Unknown");
                break;
        }
        MessageObj->SetStringField(TEXT("severity"), Severity);
        
        // Message text
        MessageObj->SetStringField(TEXT("message"), Message->ToText().ToString());
        
        // TODO: In a future enhancement, we could parse the tokenized message
        // to extract node references and other structured data
        
        Messages.Add(MakeShareable(new FJsonValueObject(MessageObj)));
    }
    
    return Messages;
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