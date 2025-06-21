// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#include "MCP/Utils/N2CMcpBlueprintUtils.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Core/N2CEditorIntegration.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "BlueprintEditor.h" // Required for TSharedPtr<FBlueprintEditor>
#include "Subsystems/AssetEditorSubsystem.h"
#include "BlueprintEditorModule.h"
#include "Framework/Application/SlateApplication.h"
#include "Utils/N2CLogger.h"
#include "BlueprintActionDatabase.h" // For FBlueprintActionDatabase

UBlueprint* FN2CMcpBlueprintUtils::ResolveBlueprint(const FString& OptionalBlueprintPath, FString& OutErrorMsg)
{
    OutErrorMsg.Empty();
    UBlueprint* ResolvedBlueprint = nullptr;

    if (!OptionalBlueprintPath.IsEmpty())
    {
        FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
        IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
        FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(OptionalBlueprintPath));

        if (AssetData.IsValid())
        {
            ResolvedBlueprint = Cast<UBlueprint>(AssetData.GetAsset());
        }
        
        if (!ResolvedBlueprint)
        {
            // Try alternate loading method if AssetRegistry fails (e.g. for unsaved assets or direct paths)
            ResolvedBlueprint = LoadObject<UBlueprint>(nullptr, *OptionalBlueprintPath);
        }

        if (!ResolvedBlueprint)
        {
            OutErrorMsg = FString::Printf(TEXT("ASSET_NOT_FOUND: Blueprint not found at path: %s"), *OptionalBlueprintPath);
            return nullptr;
        }
        return ResolvedBlueprint;
    }
    
    // If path is empty, try to get the focused Blueprint
    TSharedPtr<FBlueprintEditor> Editor = FN2CEditorIntegration::Get().GetActiveBlueprintEditor();
    if (Editor.IsValid())
    {
        ResolvedBlueprint = Editor->GetBlueprintObj();
    }

    if (!ResolvedBlueprint)
    {
        OutErrorMsg = TEXT("NO_ACTIVE_BLUEPRINT: No blueprint path provided and no focused editor found.");
        return nullptr;
    }

    return ResolvedBlueprint;
}

bool FN2CMcpBlueprintUtils::GetFocusedEditorGraph(UBlueprint*& OutBlueprint, UEdGraph*& OutGraph, FString& OutErrorMsg)
{
    OutErrorMsg.Empty();
    OutBlueprint = nullptr;
    OutGraph = nullptr;

    OutGraph = FN2CEditorIntegration::Get().GetFocusedGraphFromActiveEditor();
    if (!OutGraph)
    {
        OutErrorMsg = TEXT("NO_FOCUSED_GRAPH: No graph is currently focused in the active Blueprint editor.");
        return false;
    }

    OutBlueprint = FBlueprintEditorUtils::FindBlueprintForGraph(OutGraph);
    if (!OutBlueprint)
    {
        // This case should ideally not happen if a graph is focused.
        OutErrorMsg = FString::Printf(TEXT("INTERNAL_ERROR: Could not find owning Blueprint for focused graph: %s"), *OutGraph->GetName());
        return false;
    }
    
    return true;
}

bool FN2CMcpBlueprintUtils::OpenBlueprintEditor(UBlueprint* Blueprint, TSharedPtr<IBlueprintEditor>& OutEditor, FString& OutErrorMsg)
{
    OutErrorMsg.Empty();
    OutEditor.Reset();
    
    if (!Blueprint)
    {
        OutErrorMsg = TEXT("INVALID_BLUEPRINT: Blueprint is null");
        return false;
    }
    
    // Get the asset editor subsystem
    UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
    if (!AssetEditorSubsystem)
    {
        OutErrorMsg = TEXT("EDITOR_SUBSYSTEM_ERROR: Could not get AssetEditorSubsystem");
        return false;
    }
    
    // First check if there's already an editor open for this Blueprint
    IAssetEditorInstance* ExistingEditor = AssetEditorSubsystem->FindEditorForAsset(Blueprint, true); // true = focus if open
    
    if (ExistingEditor)
    {
        // An editor is already open, we just focused it
        // Give the editor focus callback time to update
        FSlateApplication::Get().ProcessApplicationActivationEvent(true);
        
        // Try to get the active Blueprint editor from our integration
        OutEditor = FN2CEditorIntegration::Get().GetActiveBlueprintEditor();
        
        if (OutEditor.IsValid())
        {
            FN2CLogger::Get().Log(FString::Printf(TEXT("Using existing editor for Blueprint: %s"), *Blueprint->GetName()), EN2CLogSeverity::Debug);
            return true;
        }
    }
    
    // If we get here, either there was no existing editor or we couldn't get a valid reference to it
    // Use the asset editor subsystem to open the editor, which will reuse existing ones
    bool bOpened = AssetEditorSubsystem->OpenEditorForAsset(Blueprint);
    if (!bOpened)
    {
        OutErrorMsg = TEXT("OPEN_EDITOR_FAILED: Failed to open editor for Blueprint");
        return false;
    }
    
    // After opening, we should be able to get the active Blueprint editor
    OutEditor = FN2CEditorIntegration::Get().GetActiveBlueprintEditor();
    
    if (!OutEditor.IsValid())
    {
        // This shouldn't happen, but as a fallback we can try to create a new editor
        FN2CLogger::Get().LogWarning(TEXT("Could not get active Blueprint editor after opening, creating new instance"));
        
        FBlueprintEditorModule& BlueprintEditorModule = FModuleManager::LoadModuleChecked<FBlueprintEditorModule>("Kismet");
        TSharedRef<IBlueprintEditor> NewEditor = BlueprintEditorModule.CreateBlueprintEditor(
            EToolkitMode::Standalone, 
            TSharedPtr<IToolkitHost>(), 
            Blueprint
        );
        OutEditor = NewEditor;
        
        // Make sure to update the active editor
        if (TSharedPtr<FBlueprintEditor> BPEditor = StaticCastSharedPtr<FBlueprintEditor>(OutEditor))
        {
            FN2CEditorIntegration::Get().StoreActiveBlueprintEditor(BPEditor);
        }
    }
    
    if (!OutEditor.IsValid())
    {
        OutErrorMsg = TEXT("EDITOR_CREATION_FAILED: Could not create or get Blueprint editor");
        return false;
    }
    
    return true;
}

void FN2CMcpBlueprintUtils::RefreshBlueprintActionDatabase()
{
    if (FBlueprintActionDatabase* ActionDB = FBlueprintActionDatabase::TryGet())
    {
        FN2CLogger::Get().Log(TEXT("Refreshing BlueprintActionDatabase via utility function."), EN2CLogSeverity::Debug);
        ActionDB->RefreshAll();
        FN2CLogger::Get().Log(TEXT("BlueprintActionDatabase refreshed successfully via utility."), EN2CLogSeverity::Debug);
    }
    else
    {
        FN2CLogger::Get().LogWarning(TEXT("FBlueprintActionDatabase not available for refresh via utility. Context menu issues might persist."));
    }
}
