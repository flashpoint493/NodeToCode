// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintEditorModule.h"
#include "BlueprintEditor.h"
#include "Code Editor/Models/N2CCodeLanguage.h"
#include "Utils/N2CLogger.h"
#include "LLM/IN2CLLMService.h"
#include "Models/N2CBlueprint.h"

/**
 * @class FN2CEditorIntegration
 * @brief Handles integration with the Blueprint Editor
 *
 * Manages Blueprint Editor toolbar extensions and provides
 * access to the active Blueprint Editor instance.
 */
class FN2CEditorIntegration
{
public:
    static FN2CEditorIntegration& Get();

    /** Initialize integration with Blueprint Editor */
    void Initialize();

    /** Cleanup integration */
    void Shutdown();

    /** Get available themes for a language */
    TArray<FName> GetAvailableThemes(EN2CCodeLanguage Language) const;

    /** Get the default theme for a language */
    FName GetDefaultTheme(EN2CCodeLanguage Language) const;

    /** Called by HandleAssetEditorOpened to update the active editor */
    void StoreActiveBlueprintEditor(TWeakPtr<FBlueprintEditor> Editor);
    
    /** Retrieves the last stored active Blueprint editor */
    TSharedPtr<FBlueprintEditor> GetActiveBlueprintEditor() const;

    /** Retrieves the focused graph from the stored active editor */
    UEdGraph* GetFocusedGraphFromActiveEditor() const;
    
    /** Collects K2Nodes from a given UEdGraph */
    bool CollectNodesFromGraph(UEdGraph* Graph, TArray<UK2Node*>& OutNodes) const;

    /** Translates an array of K2Nodes into an FN2CBlueprint structure */
    bool TranslateNodesToN2CBlueprint(const TArray<UK2Node*>& CollectedNodes, FN2CBlueprint& OutN2CBlueprint) const;
    
    /** Translates an array of K2Nodes into an FN2CBlueprint structure and preserves ID maps */
    bool TranslateNodesToN2CBlueprintWithMaps(const TArray<UK2Node*>& CollectedNodes, FN2CBlueprint& OutN2CBlueprint,
        TMap<FGuid, FString>& OutNodeIDMap, TMap<FGuid, FString>& OutPinIDMap) const;
    
    /** Serializes an FN2CBlueprint structure to a JSON string */
    FString SerializeN2CBlueprintToJson(const FN2CBlueprint& Blueprint, bool bPrettyPrint) const;

    /** New high-level function to be called by the MCP tool handler
     * Returns N2CJSON string for the focused Blueprint, or an empty string on failure.
     * Populates OutErrorMsg if an error occurs.
     */
    FString GetFocusedBlueprintAsJson(bool bPrettyPrint, FString& OutErrorMsg);

private:
    /** Constructor */
    FN2CEditorIntegration() = default;

public:
    /** Get Blueprint Editor from active tab */
    TSharedPtr<FBlueprintEditor> GetBlueprintEditorFromTab() const;

    /** Register for Blueprint Editor callbacks */
    void RegisterBlueprintEditorCallback();

private:
    /** Map of Blueprint Editor instances to their command lists */
    TMap<TWeakPtr<FBlueprintEditor>, TSharedPtr<FUICommandList>> EditorCommandLists;

    /** The currently active Blueprint editor */
    TWeakPtr<FBlueprintEditor> ActiveBlueprintEditor;

    /** Register toolbar for a specific Blueprint Editor */
    void RegisterToolbarForEditor(TSharedPtr<FBlueprintEditor> InEditor);

    /** Execute collect nodes for a specific editor */
    void TranslateBlueprintNodesForEditor(TWeakPtr<FBlueprintEditor> InEditor);

    /** Execute copy blueprint JSON to clipboard for a specific editor */
    void ExecuteCopyJsonForEditor(TWeakPtr<FBlueprintEditor> InEditor);
    
    /** Handle asset editor opened callback */
    void HandleAssetEditorOpened(UObject* Asset, IAssetEditorInstance* EditorInstance);

};
