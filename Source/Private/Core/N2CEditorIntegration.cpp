// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#include "Core/N2CEditorIntegration.h"

#include "BlueprintEditorModes.h"
#include "Core/N2CNodeCollector.h"
#include "BlueprintEditorModule.h"
#include "Code Editor/Models/N2CCodeLanguage.h"
#include "Core/N2CEditorWindow.h"
#include "Core/N2CNodeTranslator.h"
#include "Core/N2CSerializer.h"
#include "Core/N2CSettings.h"
#include "Core/N2CToolbarCommand.h"
#include "LLM/N2CLLMModule.h"
#include "LLM/N2CLLMTypes.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsPlatformApplicationMisc.h"
#endif

#if PLATFORM_MAC
#include "Mac/MacPlatformApplicationMisc.h"
#endif

FN2CEditorIntegration& FN2CEditorIntegration::Get()
{
    static FN2CEditorIntegration Instance;
    return Instance;
}

void FN2CEditorIntegration::StoreActiveBlueprintEditor(TWeakPtr<FBlueprintEditor> Editor)
{
    ActiveBlueprintEditor = Editor;
}

TSharedPtr<FBlueprintEditor> FN2CEditorIntegration::GetActiveBlueprintEditor() const
{
    return ActiveBlueprintEditor.Pin();
}

UEdGraph* FN2CEditorIntegration::GetFocusedGraphFromActiveEditor() const
{
    TSharedPtr<FBlueprintEditor> Editor = GetActiveBlueprintEditor();
    if (Editor.IsValid())
    {
        return Editor->GetFocusedGraph();
    }
    return nullptr;
}

bool FN2CEditorIntegration::CollectNodesFromGraph(UEdGraph* Graph, TArray<UK2Node*>& OutNodes) const
{
    if (!Graph)
    {
        FN2CLogger::Get().LogError(TEXT("CollectNodesFromGraph: Graph is null"));
        return false;
    }

    FN2CNodeCollector& Collector = FN2CNodeCollector::Get();
    bool bSuccess = Collector.CollectNodesFromGraph(Graph, OutNodes);
    
    if (bSuccess)
    {
        FN2CLogger::Get().Log(
            FString::Printf(TEXT("CollectNodesFromGraph: Successfully collected %d nodes"), OutNodes.Num()),
            EN2CLogSeverity::Info
        );
    }
    else
    {
        FN2CLogger::Get().LogError(TEXT("CollectNodesFromGraph: Failed to collect nodes"));
    }
    
    return bSuccess;
}

bool FN2CEditorIntegration::TranslateNodesToN2CBlueprint(const TArray<UK2Node*>& CollectedNodes, FN2CBlueprint& OutN2CBlueprint) const
{
    FN2CNodeTranslator& Translator = FN2CNodeTranslator::Get();
    
    if (Translator.GenerateN2CStruct(CollectedNodes))
    {
        // Get the translated blueprint structure
        OutN2CBlueprint = Translator.GetN2CBlueprint();
        
        // Validate the result
        bool bIsValid = OutN2CBlueprint.IsValid();
        
        if (bIsValid)
        {
            FN2CLogger::Get().Log(TEXT("TranslateNodesToN2CBlueprint: Translation validation successful"), EN2CLogSeverity::Info);
        }
        else
        {
            FN2CLogger::Get().LogError(TEXT("TranslateNodesToN2CBlueprint: Translation validation failed"));
        }
        
        return bIsValid;
    }
    
    FN2CLogger::Get().LogError(TEXT("TranslateNodesToN2CBlueprint: Failed to generate N2C structure"));
    return false;
}

bool FN2CEditorIntegration::TranslateNodesToN2CBlueprintWithMaps(const TArray<UK2Node*>& CollectedNodes, FN2CBlueprint& OutN2CBlueprint, 
    TMap<FGuid, FString>& OutNodeIDMap, TMap<FGuid, FString>& OutPinIDMap) const
{
    FN2CNodeTranslator& Translator = FN2CNodeTranslator::Get();
    
    if (Translator.GenerateN2CStruct(CollectedNodes))
    {
        // Get the translated blueprint structure
        OutN2CBlueprint = Translator.GetN2CBlueprint();
        
        // IMPORTANT: Preserve the ID maps immediately after translation
        Translator.PreserveIDMaps(OutNodeIDMap, OutPinIDMap);
        
        // Validate the result
        bool bIsValid = OutN2CBlueprint.IsValid();
        
        if (bIsValid)
        {
            FN2CLogger::Get().Log(TEXT("TranslateNodesToN2CBlueprintWithMaps: Translation validation successful"), EN2CLogSeverity::Info);
            FN2CLogger::Get().Log(FString::Printf(TEXT("TranslateNodesToN2CBlueprintWithMaps: Preserved %d node IDs and %d pin IDs"), 
                OutNodeIDMap.Num(), OutPinIDMap.Num()), EN2CLogSeverity::Info);
        }
        else
        {
            FN2CLogger::Get().LogError(TEXT("TranslateNodesToN2CBlueprintWithMaps: Translation validation failed"));
        }
        
        return bIsValid;
    }
    
    FN2CLogger::Get().LogError(TEXT("TranslateNodesToN2CBlueprintWithMaps: Failed to generate N2C structure"));
    return false;
}

FString FN2CEditorIntegration::SerializeN2CBlueprintToJson(const FN2CBlueprint& Blueprint, bool bPrettyPrint) const
{
    FN2CSerializer::SetPrettyPrint(bPrettyPrint);
    return FN2CSerializer::ToJson(Blueprint);
}

FString FN2CEditorIntegration::GetFocusedBlueprintAsJson(bool bPrettyPrint, FString& OutErrorMsg)
{
    // Get the active Blueprint editor
    TSharedPtr<FBlueprintEditor> Editor = GetActiveBlueprintEditor();
    if (!Editor.IsValid())
    {
        OutErrorMsg = TEXT("No active Blueprint Editor.");
        return FString();
    }
    
    // Get the focused graph
    UEdGraph* FocusedGraph = GetFocusedGraphFromActiveEditor();
    if (!FocusedGraph)
    {
        OutErrorMsg = TEXT("No focused graph in the active Blueprint Editor.");
        return FString();
    }
    
    // Collect nodes from the graph
    TArray<UK2Node*> CollectedNodes;
    if (!CollectNodesFromGraph(FocusedGraph, CollectedNodes) || CollectedNodes.IsEmpty())
    {
        OutErrorMsg = TEXT("Failed to collect nodes or no nodes found in the focused graph.");
        return FString();
    }
    
    // Translate nodes to N2CBlueprint structure
    FN2CBlueprint N2CBlueprintData;
    if (!TranslateNodesToN2CBlueprint(CollectedNodes, N2CBlueprintData))
    {
        OutErrorMsg = TEXT("Failed to translate collected nodes into N2CBlueprint structure.");
        return FString();
    }
    
    // Serialize to JSON
    FString JsonOutput = SerializeN2CBlueprintToJson(N2CBlueprintData, bPrettyPrint);
    if (JsonOutput.IsEmpty())
    {
        OutErrorMsg = TEXT("Failed to serialize N2CBlueprint to JSON.");
        return FString();
    }
    
    OutErrorMsg = TEXT("Success.");
    return JsonOutput;
}

void FN2CEditorIntegration::ExecuteCopyJsonForEditor(TWeakPtr<FBlueprintEditor> InEditor)
{
    FN2CLogger::Get().Log(TEXT("ExecuteCopyJsonForEditor called"), EN2CLogSeverity::Debug);

    // Store the editor as active
    StoreActiveBlueprintEditor(InEditor);

    // Use the new helper method to get JSON with pretty printing for clipboard
    FString ErrorMsg;
    FString JsonOutput = GetFocusedBlueprintAsJson(true /* pretty print for clipboard */, ErrorMsg);

    if (JsonOutput.IsEmpty())
    {
        FN2CLogger::Get().LogError(FString::Printf(TEXT("Failed to get Blueprint JSON: %s"), *ErrorMsg));
        return;
    }

    // Copy JSON to clipboard
    FPlatformApplicationMisc::ClipboardCopy(*JsonOutput);

    // Show notification
    FNotificationInfo Info(NSLOCTEXT("NodeToCode", "BlueprintJsonCopied", "Blueprint JSON copied to clipboard"));
    Info.bFireAndForget = true;
    Info.FadeInDuration = 0.2f;
    Info.FadeOutDuration = 0.5f;
    Info.ExpireDuration = 2.0f;
    FSlateNotificationManager::Get().AddNotification(Info);

    FN2CLogger::Get().Log(TEXT("Blueprint JSON copied to clipboard successfully"), EN2CLogSeverity::Info);
}

void FN2CEditorIntegration::Initialize()
{
    // Register commands
    FN2CToolbarCommand::Register();

    // Register tab spawner
    SN2CEditorWindow::RegisterTabSpawner();

    // Subscribe to asset editor opened events
    if (GEditor)
    {
        UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
        if (ensure(AssetEditorSubsystem))
        {
            AssetEditorSubsystem->OnAssetEditorOpened().AddLambda([this](UObject* Asset)
            {
                if (IAssetEditorInstance* EditorInstance = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->FindEditorForAsset(Asset, false))
                {
                    HandleAssetEditorOpened(Asset, EditorInstance);
                }
            });
            FN2CLogger::Get().Log(TEXT("N2C Editor Integration: Subscribed to OnAssetEditorOpened via AssetEditorSubsystem"), EN2CLogSeverity::Info);
        }
    }

    FN2CLogger::Get().Log(TEXT("N2C Editor Integration initialized"), EN2CLogSeverity::Info);
}

void FN2CEditorIntegration::Shutdown()
{
    // Unregister tab spawner
    SN2CEditorWindow::UnregisterTabSpawner();

    // Clear editor command lists
    EditorCommandLists.Empty();

    // Unsubscribe from asset editor events
    if (GEditor)
    {
        UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
        if (AssetEditorSubsystem)
        {
            AssetEditorSubsystem->OnAssetEditorOpened().RemoveAll(this);
        }
    }

    FN2CLogger::Get().Log(TEXT("N2C Editor Integration shutdown"), EN2CLogSeverity::Info);
}


TSharedPtr<FBlueprintEditor> FN2CEditorIntegration::GetBlueprintEditorFromTab() const
{
    // Mark as deprecated
    FN2CLogger::Get().LogWarning(TEXT("GetBlueprintEditorFromTab is deprecated - editors should be accessed directly"));
    return nullptr;
}

void FN2CEditorIntegration::HandleAssetEditorOpened(UObject* Asset, IAssetEditorInstance* EditorInstance)
{
    if (!Asset || !EditorInstance)
    {
        return;
    }

    // Check if the asset is a Blueprint or a child class of Blueprint
    UBlueprint* OpenedBlueprint = Cast<UBlueprint>(Asset);
    if (!OpenedBlueprint)
    {
        return; // Not a Blueprint, so ignore
    }

    // Convert the EditorInstance to the correct type
    FBlueprintEditor* BlueprintEditorPtr = static_cast<FBlueprintEditor*>(EditorInstance);
    if (!BlueprintEditorPtr)
    {
        return;
    }

    // Convert to SharedPtr so it matches our existing RegisterToolbarForEditor() signature
    TSharedPtr<FBlueprintEditor> BlueprintEditorShared = StaticCastSharedRef<FBlueprintEditor>(BlueprintEditorPtr->AsShared());
    if (BlueprintEditorShared.IsValid())
    {
        // Store the active Blueprint editor
        StoreActiveBlueprintEditor(BlueprintEditorShared);
        
        // Check if we already have this editor registered
        TWeakPtr<FBlueprintEditor> WeakEditor(BlueprintEditorShared);
        if (!EditorCommandLists.Contains(WeakEditor))
        {
            FString BlueprintPath = OpenedBlueprint->GetPathName();
            
            FN2CLogger::Get().Log(
                FString::Printf(TEXT("Registering toolbar for Blueprint Editor: %s"), 
                *BlueprintPath), 
                EN2CLogSeverity::Info
            );
            
            RegisterToolbarForEditor(BlueprintEditorShared);
        }
        else
        {
            FN2CLogger::Get().Log(
                TEXT("Blueprint Editor already registered"), 
                EN2CLogSeverity::Debug
            );
        }
    }
}


void FN2CEditorIntegration::RegisterToolbarForEditor(TSharedPtr<FBlueprintEditor> InEditor)
{
    FN2CLogger::Get().Log(TEXT("Starting toolbar registration for editor"), EN2CLogSeverity::Info);

    if (!InEditor.IsValid())
    {
        FN2CLogger::Get().LogError(TEXT("Invalid editor pointer provided to RegisterToolbarForEditor"));
        return;
    }

    // Get Blueprint name for context
    FString BlueprintName = TEXT("Unknown");
    if (InEditor->GetBlueprintObj())
    {
        BlueprintName = InEditor->GetBlueprintObj()->GetName();
    }

    // Check if we already have a command list for this editor
    TWeakPtr<FBlueprintEditor> WeakEditor(InEditor);
    if (EditorCommandLists.Contains(WeakEditor))
    {
        FN2CLogger::Get().Log(
            FString::Printf(TEXT("Editor already has command list registered: %s"), *BlueprintName),
            EN2CLogSeverity::Warning
        );
        return;
    }

    // Create command list for this editor
    TSharedPtr<FUICommandList> CommandList = MakeShareable(new FUICommandList);
    
    FN2CLogger::Get().Log(
        FString::Printf(TEXT("Created command list for Blueprint: %s"), *BlueprintName), 
        EN2CLogSeverity::Info
    );

    // Map the Open Window command
    CommandList->MapAction(
        FN2CToolbarCommand::Get().OpenWindowCommand,
        FExecuteAction::CreateLambda([this]()
        {
            FGlobalTabmanager::Get()->TryInvokeTab(SN2CEditorWindow::TabId);
            FN2CLogger::Get().Log(TEXT("Node to Code window opened"), EN2CLogSeverity::Info);
        }),
        FCanExecuteAction::CreateLambda([]() { return true; })
    );

    // Map the Collect Nodes command
    CommandList->MapAction(
        FN2CToolbarCommand::Get().CollectNodesCommand,
        FExecuteAction::CreateLambda([this, WeakEditor, BlueprintName]()
        {
            FN2CLogger::Get().Log(
                FString::Printf(TEXT("Node to Code collection triggered for Blueprint: %s"), *BlueprintName),
                EN2CLogSeverity::Info
            );
            TranslateBlueprintNodesForEditor(WeakEditor);
        }),
        FCanExecuteAction::CreateLambda([WeakEditor]()
        {
            TSharedPtr<FBlueprintEditor> Editor = WeakEditor.Pin();
            if (!Editor.IsValid())
            {
                return false;
            }
            return Editor->GetCurrentMode() == FBlueprintEditorApplicationModes::StandardBlueprintEditorMode;
        })
    );
    
    // Map the Copy JSON command
    CommandList->MapAction(
        FN2CToolbarCommand::Get().CopyJsonCommand,
        FExecuteAction::CreateLambda([this, WeakEditor, BlueprintName]()
        {
            FN2CLogger::Get().Log(
                FString::Printf(TEXT("Copy Blueprint JSON triggered for Blueprint: %s"), *BlueprintName),
                EN2CLogSeverity::Info
            );
            ExecuteCopyJsonForEditor(WeakEditor);
        }),
        FCanExecuteAction::CreateLambda([WeakEditor]()
        {
            TSharedPtr<FBlueprintEditor> Editor = WeakEditor.Pin();
            if (!Editor.IsValid())
            {
                return false;
            }
            return Editor->GetCurrentMode() == FBlueprintEditorApplicationModes::StandardBlueprintEditorMode;
        })
    );

    // Store in our map
    EditorCommandLists.Add(WeakEditor, CommandList);
    FN2CLogger::Get().Log(
        FString::Printf(TEXT("Added command list to map for Blueprint: %s"), *BlueprintName),
        EN2CLogSeverity::Info
    );

    // Add toolbar extension
    TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);
    ToolbarExtender->AddToolBarExtension(
        "Asset",
        EExtensionHook::After,
        CommandList,
        FToolBarExtensionDelegate::CreateLambda([CommandList](FToolBarBuilder& Builder)
        {
            Builder.BeginSection("NodeToCode");
            
            // Add dropdown button
            Builder.AddComboButton(
                FUIAction(),
                FOnGetContent::CreateLambda([CommandList]() -> TSharedRef<SWidget>
                {
                    FMenuBuilder MenuBuilder(true, CommandList);
                    
                    MenuBuilder.AddMenuEntry(FN2CToolbarCommand::Get().OpenWindowCommand);
                    MenuBuilder.AddMenuEntry(FN2CToolbarCommand::Get().CollectNodesCommand);
                    MenuBuilder.AddMenuEntry(FN2CToolbarCommand::Get().CopyJsonCommand);

                    return MenuBuilder.MakeWidget();
                }),
                NSLOCTEXT("NodeToCode", "NodeToCodeActions", "Node to Code"),
                NSLOCTEXT("NodeToCode", "NodeToCodeTooltip", "Node to Code Actions"),
                FSlateIcon("NodeToCodeStyle", "NodeToCode.ToolbarButton")
            );
            
            Builder.EndSection();
        })
    );

    // Add the extender to this specific editor
    InEditor->AddToolbarExtender(ToolbarExtender);

    InEditor->RegenerateMenusAndToolbars();
    
    FN2CLogger::Get().Log(
        FString::Printf(TEXT("Completed toolbar registration for Blueprint: %s"), *BlueprintName), 
        EN2CLogSeverity::Info
    );
}

TArray<FName> FN2CEditorIntegration::GetAvailableThemes(EN2CCodeLanguage Language) const
{
    TArray<FName> ThemeNames;
    const UN2CSettings* Settings = GetDefault<UN2CSettings>();
    
    switch (Language)
    {
        case EN2CCodeLanguage::Cpp:
            Settings->CPPThemes.Themes.GetKeys(ThemeNames);
            break;
        case EN2CCodeLanguage::Python:
            Settings->PythonThemes.Themes.GetKeys(ThemeNames);
            break;
        case EN2CCodeLanguage::JavaScript:
            Settings->JavaScriptThemes.Themes.GetKeys(ThemeNames);
            break;
        case EN2CCodeLanguage::CSharp:
            Settings->CSharpThemes.Themes.GetKeys(ThemeNames);
            break;
        case EN2CCodeLanguage::Swift:
            Settings->SwiftThemes.Themes.GetKeys(ThemeNames);
            break;
    }
    
    return ThemeNames;
}

FName FN2CEditorIntegration::GetDefaultTheme(EN2CCodeLanguage Language) const
{
    return TEXT("Unreal Engine");
}

void FN2CEditorIntegration::TranslateBlueprintNodesForEditor(TWeakPtr<FBlueprintEditor> InEditor)
{
    // Check if translation is already in progress
    UN2CLLMModule* LLMModule = UN2CLLMModule::Get();
    if (LLMModule && LLMModule->GetSystemStatus() == EN2CSystemStatus::Processing)
    {
        FN2CLogger::Get().LogWarning(TEXT("Translation already in progress, please wait"));
        return;
    }

    FN2CLogger::Get().Log(TEXT("ExecuteCollectNodesForEditor called"), EN2CLogSeverity::Debug);

    // Show the window as a tab
    FGlobalTabmanager::Get()->TryInvokeTab(SN2CEditorWindow::TabId);
    FN2CLogger::Get().Log(TEXT("Node to Code window shown"), EN2CLogSeverity::Debug);

    // Store the editor as active
    StoreActiveBlueprintEditor(InEditor);

    // Use the new helper method to get JSON
    FString ErrorMsg;
    FString JsonOutput = GetFocusedBlueprintAsJson(false /* no pretty print */, ErrorMsg);

    if (JsonOutput.IsEmpty())
    {
        FN2CLogger::Get().LogError(FString::Printf(TEXT("Failed to get Blueprint JSON: %s"), *ErrorMsg));
        return;
    }

    // Log the JSON output
    FN2CLogger::Get().Log(TEXT("JSON Output:"), EN2CLogSeverity::Debug);
    FN2CLogger::Get().Log(JsonOutput, EN2CLogSeverity::Debug);

    if (LLMModule->Initialize())
    {
        // Send JSON to LLM service
        LLMModule->ProcessN2CJson(JsonOutput, FOnLLMResponseReceived::CreateLambda(
            [](const FString& Response)
            {
                FN2CLogger::Get().Log(FString::Printf(TEXT("LLM Response:\n\n%s"), *Response), EN2CLogSeverity::Debug);
            
                // Create translation response struct
                FN2CTranslationResponse TranslationResponse;
            
                // Get active service's response parser
                TScriptInterface<IN2CLLMService> ActiveService = UN2CLLMModule::Get()->GetActiveService();
                if (ActiveService.GetInterface())
                {
                    UN2CResponseParserBase* Parser = ActiveService->GetResponseParser();
                    if (Parser)
                    {
                        if (Parser->ParseLLMResponse(Response, TranslationResponse))
                        {
                            // Log successful parsing
                            FN2CLogger::Get().Log(TEXT("Successfully parsed LLM response"), EN2CLogSeverity::Info);
                        }
                        else
                        {
                            FN2CLogger::Get().LogError(TEXT("Failed to parse LLM response"));
                        }
                    }
                    else
                    {
                        FN2CLogger::Get().LogError(TEXT("No response parser available"));
                    }
                }
                else
                {
                    FN2CLogger::Get().LogError(TEXT("No active LLM service"));
                }
            }));
    }
    else
    {
        FN2CLogger::Get().LogError(TEXT("Failed to initialize LLM Module"));
    }
}

void FN2CEditorIntegration::TranslateFocusedBlueprintGraph()
{
    // Get the latest active blueprint editor
    TSharedPtr<FBlueprintEditor> ActiveEditor = GetActiveBlueprintEditor();
    
    if (!ActiveEditor.IsValid())
    {
        FN2CLogger::Get().LogError(TEXT("TranslateFocusedBlueprintGraph: No active Blueprint editor found"));
        return;
    }
    
    // Call the existing translation function with the active editor
    TranslateBlueprintNodesForEditor(ActiveEditor);
}

void FN2CEditorIntegration::TranslateFocusedBlueprintAsync(
    const FString& ProviderIdOverride,
    const FString& ModelIdOverride,
    const FString& LanguageIdOverride,
    FOnLLMResponseReceived OnComplete)
{
    // This initial part MUST run on GameThread if GetFocusedBlueprintAsJson is not thread-safe
    // GetFocusedBlueprintAsJson itself handles some GameThread dispatching for its internal calls.
    // However, the overall call to TranslateFocusedBlueprintAsync is expected from a worker thread.
    // So, we'll dispatch the JSON retrieval to the game thread and wait.
    
    TPromise<FString> JsonPromise;
    TFuture<FString> JsonFuture = JsonPromise.GetFuture();

    AsyncTask(ENamedThreads::GameThread, [this, &JsonPromise]()
    {
        FString ErrorMsg;
        FString JsonInput = GetFocusedBlueprintAsJson(false /*bPrettyPrint*/, ErrorMsg);
        if (JsonInput.IsEmpty())
        {
            JsonPromise.SetValue(FString::Printf(TEXT("{\"error\":\"Failed to get Blueprint JSON: %s\"}"), *ErrorMsg));
        }
        else
        {
            JsonPromise.SetValue(JsonInput);
        }
    });

    JsonFuture.Wait(); // Wait for the game thread task to complete
    FString JsonInput = JsonFuture.Get();

    if (JsonInput.StartsWith(TEXT("{\"error\":")))
    {
        OnComplete.ExecuteIfBound(JsonInput);
        return;
    }

    UN2CLLMModule* LLMModule = UN2CLLMModule::Get();
    if (!LLMModule || !LLMModule->IsInitialized())
    {
        // Attempt to initialize if not already
        if (!LLMModule || !LLMModule->Initialize())
        {
             OnComplete.ExecuteIfBound(TEXT("{\"error\":\"LLMModule failed to initialize.\"}"));
             return;
        }
    }
    
    const UN2CSettings* Settings = GetDefault<UN2CSettings>();
    
    FN2CLLMConfig RequestConfig = LLMModule->GetConfig(); // Start with default config from module

    EN2CLLMProvider FinalProvider = RequestConfig.Provider;
    if (!ProviderIdOverride.IsEmpty())
    {
        const UEnum* ProviderEnum = StaticEnum<EN2CLLMProvider>();
        int64 EnumValue = ProviderEnum->GetValueByNameString(ProviderIdOverride, EGetByNameFlags::CaseSensitive);
        if (EnumValue == INDEX_NONE) // Try with "EN2CLLMProvider::" prefix
        {
            EnumValue = ProviderEnum->GetValueByNameString(FString(TEXT("EN2CLLMProvider::")) + ProviderIdOverride, EGetByNameFlags::CaseSensitive);
        }

        if (EnumValue != INDEX_NONE)
        {
            FinalProvider = static_cast<EN2CLLMProvider>(EnumValue);
        }
        else
        {
            FN2CLogger::Get().LogWarning(FString::Printf(TEXT("Invalid ProviderId '%s' provided. Using default: %s"), 
                *ProviderIdOverride, *UEnum::GetValueAsString(FinalProvider)));
        }
    }
    RequestConfig.Provider = FinalProvider;
    RequestConfig.ApiKey = Settings->GetActiveApiKeyForProvider(FinalProvider); // Get API key for the potentially overridden provider

    if (!ModelIdOverride.IsEmpty())
    {
        RequestConfig.Model = ModelIdOverride;
    }
    else
    {
        // If provider was overridden (or not), get the default model for the final provider
        RequestConfig.Model = Settings->GetActiveModelForProvider(FinalProvider);
    }
    
    EN2CCodeLanguage TargetLanguage = Settings->TargetLanguage; // Default from settings
    if (!LanguageIdOverride.IsEmpty())
    {
        const UEnum* LangEnum = StaticEnum<EN2CCodeLanguage>();
        FString SearchLangId = LanguageIdOverride;
        int64 EnumValue = LangEnum->GetValueByNameString(SearchLangId, EGetByNameFlags::CaseSensitive);
        if (EnumValue == INDEX_NONE) // Try with "EN2CCodeLanguage::" prefix
        {
            EnumValue = LangEnum->GetValueByNameString(FString(TEXT("EN2CCodeLanguage::")) + SearchLangId, EGetByNameFlags::CaseSensitive);
        }

        if (EnumValue != INDEX_NONE)
        {
            TargetLanguage = static_cast<EN2CCodeLanguage>(EnumValue);
        }
        else
        {
             FN2CLogger::Get().LogWarning(FString::Printf(TEXT("Invalid LanguageId '%s' provided. Using default: %s"), 
                *LanguageIdOverride, *UEnum::GetValueAsString(TargetLanguage)));
        }
    }

    // Now call the LLMModule method that handles overrides
    LLMModule->ProcessN2CJsonWithOverrides(JsonInput, RequestConfig, TargetLanguage, OnComplete);
}
