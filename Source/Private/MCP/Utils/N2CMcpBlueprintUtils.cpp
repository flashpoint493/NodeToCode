// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#include "MCP/Utils/N2CMcpBlueprintUtils.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Core/N2CEditorIntegration.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "BlueprintEditor.h" // Required for TSharedPtr<FBlueprintEditor>

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
