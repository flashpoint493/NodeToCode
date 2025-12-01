// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintEditorModule.h"
#include "BlueprintEditor.h"
#include "Code Editor/Models/N2CCodeLanguage.h"
#include "Utils/N2CLogger.h"
#include "LLM/IN2CLLMService.h"
#include "Models/N2CBlueprint.h"

// Forward declarations
class SN2CGraphEditorWrapper;
class SDockTab;
class SN2CGraphOverlay;

/**
 * Delegate for when translation state changes globally
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnN2CTranslationStateChanged, bool /* bIsTranslating */);

/**
 * Delegate for when a graph overlay requests translation
 * The main window can subscribe to show progress modal
 */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnN2COverlayTranslationRequested, const FGuid& /* GraphGuid */, const FString& /* GraphName */, const FString& /* BlueprintPath */);

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

    /** Simple wrapper function that translates the focused Blueprint graph using the latest active editor */
    void TranslateFocusedBlueprintGraph();

    /**
     * Asynchronously translates the focused Blueprint graph using specified or default LLM settings.
     * This method is designed to be called from a background thread.
     * @param ProviderIdOverride Optional override for the LLM provider ID.
     * @param ModelIdOverride Optional override for the LLM model ID.
     * @param LanguageIdOverride Optional override for the target language ID.
     * @param OnComplete Delegate called with the translation result (JSON string) or an error message.
     */
    void TranslateFocusedBlueprintAsync(
        const FString& ProviderIdOverride,
        const FString& ModelIdOverride,
        const FString& LanguageIdOverride,
        FOnLLMResponseReceived OnComplete
    );

    /**
     * Wrap a graph tab with the NodeToCode overlay if not already wrapped
     * @param Tab The dock tab to wrap
     * @param Graph The graph being displayed in the tab
     * @param Editor The Blueprint editor instance
     */
    void WrapGraphTabIfNeeded(TSharedPtr<SDockTab> Tab, UEdGraph* Graph, TWeakPtr<FBlueprintEditor> Editor);

    /**
     * Try to wrap the currently focused graph tab in the given editor
     * @param Editor The Blueprint editor to check
     */
    void TryWrapFocusedGraphTab(TWeakPtr<FBlueprintEditor> Editor);

    /**
     * Clean up any stale wrapper references
     */
    void CleanupStaleWrappers();

    // ==================== Global Translation State ====================

    /**
     * Check if any translation is currently in progress
     * This includes both single graph translations and batch translations
     */
    bool IsAnyTranslationInProgress() const { return bIsAnyTranslationInProgress; }

    /**
     * Set the global translation state
     * Called by overlays, main window, etc. when starting/completing translations
     */
    void SetTranslationInProgress(bool bInProgress);

    /**
     * Request a translation from a graph overlay
     * This broadcasts to the main window to show progress modal
     */
    void RequestOverlayTranslation(const FGuid& GraphGuid, const FString& GraphName, const FString& BlueprintPath);

    /** Delegate fired when global translation state changes */
    FOnN2CTranslationStateChanged OnTranslationStateChanged;

    /** Delegate fired when a graph overlay requests translation */
    FOnN2COverlayTranslationRequested OnOverlayTranslationRequested;

private:
    /** Constructor */
    FN2CEditorIntegration() = default;

public:
    /** Get Blueprint Editor from active tab */
    TSharedPtr<FBlueprintEditor> GetBlueprintEditorFromTab() const;

    /** Register for Blueprint Editor callbacks */
    void RegisterBlueprintEditorCallback();

private:
    /** The currently active Blueprint editor */
    TWeakPtr<FBlueprintEditor> ActiveBlueprintEditor;

    /** Map of wrapped tabs to their wrapper widgets (legacy, kept for compatibility) */
    TMap<TWeakPtr<SDockTab>, TSharedPtr<SN2CGraphEditorWrapper>> WrappedTabs;

    /** Set of graph GUIDs that have had overlays injected */
    TSet<FGuid> InjectedGraphOverlays;

    /** Timer handle for deferred graph tab wrapping */
    FTimerHandle GraphTabWrapTimerHandle;

    /** Delegate handle for tab change subscription */
    FDelegateHandle OnActiveTabChangedHandle;

    /** Global flag tracking if any translation is in progress */
    bool bIsAnyTranslationInProgress = false;

    /** Handle tab activation to wrap new graph tabs */
    void OnActiveTabChanged(TSharedPtr<SDockTab> PreviouslyActive, TSharedPtr<SDockTab> NewlyActivated);

    /** Execute collect nodes for a specific editor */
    void TranslateBlueprintNodesForEditor(TWeakPtr<FBlueprintEditor> InEditor);

    /** Execute copy blueprint JSON to clipboard for a specific editor */
    void ExecuteCopyJsonForEditor(TWeakPtr<FBlueprintEditor> InEditor);
    
    /** Handle asset editor opened callback */
    void HandleAssetEditorOpened(UObject* Asset, IAssetEditorInstance* EditorInstance);

};
