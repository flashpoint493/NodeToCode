// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#include "Core/N2CUserSecrets.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Utils/N2CLogger.h"

UN2CUserSecrets::UN2CUserSecrets()
{
    // Load secrets when the object is created
    LoadSecrets();
}

FString UN2CUserSecrets::GetSecretsFilePath()
{
    return FPaths::ConvertRelativePathToFull(
        FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NodeToCode"), TEXT("User"), TEXT("secrets.json")));
}

void UN2CUserSecrets::EnsureSecretsDirectoryExists()
{
    FString SecretsDir = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NodeToCode"), TEXT("User")));

    if (!IFileManager::Get().DirectoryExists(*SecretsDir))
    {
        IFileManager::Get().MakeDirectory(*SecretsDir, true);
        FN2CLogger::Get().Log(
            FString::Printf(TEXT("Created secrets directory: %s"), *SecretsDir),
            EN2CLogSeverity::Info);
    }
}

FString UN2CUserSecrets::GetProviderKeyName(EN2COAuthProvider ProviderId)
{
    switch (ProviderId)
    {
    case EN2COAuthProvider::Anthropic:
        return TEXT("Anthropic");
    case EN2COAuthProvider::Google:
        return TEXT("Google");
    default:
        return TEXT("Unknown");
    }
}

void UN2CUserSecrets::LoadSecrets()
{
    FString SecretsFilePath = GetSecretsFilePath();

    // Check if the file exists
    if (!FPaths::FileExists(SecretsFilePath))
    {
        FN2CLogger::Get().Log(
            FString::Printf(TEXT("Secrets file not found at: %s"), *SecretsFilePath),
            EN2CLogSeverity::Info);
        return;
    }

    // Load the file content
    FString JsonString;
    if (!FFileHelper::LoadFileToString(JsonString, *SecretsFilePath))
    {
        FN2CLogger::Get().LogError(
            FString::Printf(TEXT("Failed to load secrets from: %s"), *SecretsFilePath));
        return;
    }

    // Parse the JSON
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        FN2CLogger::Get().LogError(
            FString::Printf(TEXT("Failed to parse secrets JSON from: %s"), *SecretsFilePath));
        return;
    }

    // Extract the API keys
    OpenAI_API_Key = JsonObject->GetStringField(TEXT("OpenAI_API_Key"));
    Anthropic_API_Key = JsonObject->GetStringField(TEXT("Anthropic_API_Key"));
    Gemini_API_Key = JsonObject->GetStringField(TEXT("Gemini_API_Key"));
    DeepSeek_API_Key = JsonObject->GetStringField(TEXT("DeepSeek_API_Key"));

    // Load OAuth tokens (handles migration from legacy format)
    LoadOAuthTokensFromJson(JsonObject);

    FN2CLogger::Get().Log(
        FString::Printf(TEXT("Successfully loaded secrets from: %s"), *SecretsFilePath),
        EN2CLogSeverity::Info);
}

void UN2CUserSecrets::LoadOAuthTokensFromJson(const TSharedPtr<FJsonObject>& JsonObject)
{
    // Clear existing tokens
    OAuthTokensMap.Empty();

    // Check for new nested OAuth format first
    if (JsonObject->HasField(TEXT("OAuth")))
    {
        const TSharedPtr<FJsonObject>* OAuthObject;
        if (JsonObject->TryGetObjectField(TEXT("OAuth"), OAuthObject))
        {
            // Load Anthropic tokens
            const TSharedPtr<FJsonObject>* AnthropicObject;
            if ((*OAuthObject)->TryGetObjectField(TEXT("Anthropic"), AnthropicObject))
            {
                FN2COAuthTokens AnthropicTokens;
                AnthropicTokens.AccessToken = (*AnthropicObject)->GetStringField(TEXT("AccessToken"));
                AnthropicTokens.RefreshToken = (*AnthropicObject)->GetStringField(TEXT("RefreshToken"));
                AnthropicTokens.ExpiresAt = (*AnthropicObject)->GetStringField(TEXT("ExpiresAt"));
                AnthropicTokens.Scope = (*AnthropicObject)->GetStringField(TEXT("Scope"));

                if (AnthropicTokens.HasTokens())
                {
                    OAuthTokensMap.Add(EN2COAuthProvider::Anthropic, AnthropicTokens);
                }
            }

            // Load Google tokens
            const TSharedPtr<FJsonObject>* GoogleObject;
            if ((*OAuthObject)->TryGetObjectField(TEXT("Google"), GoogleObject))
            {
                FN2COAuthTokens GoogleTokens;
                GoogleTokens.AccessToken = (*GoogleObject)->GetStringField(TEXT("AccessToken"));
                GoogleTokens.RefreshToken = (*GoogleObject)->GetStringField(TEXT("RefreshToken"));
                GoogleTokens.ExpiresAt = (*GoogleObject)->GetStringField(TEXT("ExpiresAt"));
                GoogleTokens.Scope = (*GoogleObject)->GetStringField(TEXT("Scope"));

                if (GoogleTokens.HasTokens())
                {
                    OAuthTokensMap.Add(EN2COAuthProvider::Google, GoogleTokens);
                }
            }

            FN2CLogger::Get().Log(TEXT("Loaded OAuth tokens from new nested format"), EN2CLogSeverity::Debug);
            return;
        }
    }

    // Fall back to legacy flat format (migration)
    bool bMigratedLegacy = false;

    // Check for legacy Claude tokens
    FString LegacyClaudeAccessToken = JsonObject->GetStringField(TEXT("Claude_OAuth_AccessToken"));
    FString LegacyClaudeRefreshToken = JsonObject->GetStringField(TEXT("Claude_OAuth_RefreshToken"));
    if (!LegacyClaudeAccessToken.IsEmpty() || !LegacyClaudeRefreshToken.IsEmpty())
    {
        FN2COAuthTokens AnthropicTokens;
        AnthropicTokens.AccessToken = LegacyClaudeAccessToken;
        AnthropicTokens.RefreshToken = LegacyClaudeRefreshToken;
        AnthropicTokens.ExpiresAt = JsonObject->GetStringField(TEXT("Claude_OAuth_ExpiresAt"));
        AnthropicTokens.Scope = JsonObject->GetStringField(TEXT("Claude_OAuth_Scope"));

        if (AnthropicTokens.HasTokens())
        {
            OAuthTokensMap.Add(EN2COAuthProvider::Anthropic, AnthropicTokens);
            bMigratedLegacy = true;
        }
    }

    // Check for legacy Google tokens
    FString LegacyGoogleAccessToken = JsonObject->GetStringField(TEXT("Google_OAuth_AccessToken"));
    FString LegacyGoogleRefreshToken = JsonObject->GetStringField(TEXT("Google_OAuth_RefreshToken"));
    if (!LegacyGoogleAccessToken.IsEmpty() || !LegacyGoogleRefreshToken.IsEmpty())
    {
        FN2COAuthTokens GoogleTokens;
        GoogleTokens.AccessToken = LegacyGoogleAccessToken;
        GoogleTokens.RefreshToken = LegacyGoogleRefreshToken;
        GoogleTokens.ExpiresAt = JsonObject->GetStringField(TEXT("Google_OAuth_ExpiresAt"));
        GoogleTokens.Scope = JsonObject->GetStringField(TEXT("Google_OAuth_Scope"));

        if (GoogleTokens.HasTokens())
        {
            OAuthTokensMap.Add(EN2COAuthProvider::Google, GoogleTokens);
            bMigratedLegacy = true;
        }
    }

    if (bMigratedLegacy)
    {
        FN2CLogger::Get().Log(TEXT("Migrated OAuth tokens from legacy flat format to nested format"), EN2CLogSeverity::Info);
        // Save immediately to migrate the file to the new format
        // Note: We call SaveSecrets() after LoadSecrets() completes to avoid re-entrancy
    }
}

void UN2CUserSecrets::SaveOAuthTokensToJson(TSharedPtr<FJsonObject>& JsonObject) const
{
    // Create OAuth container object
    TSharedPtr<FJsonObject> OAuthObject = MakeShareable(new FJsonObject);

    // Save each provider's tokens
    for (const auto& Pair : OAuthTokensMap)
    {
        const FString ProviderKey = GetProviderKeyName(Pair.Key);
        const FN2COAuthTokens& Tokens = Pair.Value;

        TSharedPtr<FJsonObject> ProviderObject = MakeShareable(new FJsonObject);
        ProviderObject->SetStringField(TEXT("AccessToken"), Tokens.AccessToken);
        ProviderObject->SetStringField(TEXT("RefreshToken"), Tokens.RefreshToken);
        ProviderObject->SetStringField(TEXT("ExpiresAt"), Tokens.ExpiresAt);
        ProviderObject->SetStringField(TEXT("Scope"), Tokens.Scope);

        OAuthObject->SetObjectField(ProviderKey, ProviderObject);
    }

    JsonObject->SetObjectField(TEXT("OAuth"), OAuthObject);
}

void UN2CUserSecrets::SaveSecrets()
{
    // Ensure the directory exists
    EnsureSecretsDirectoryExists();

    // Create a JSON object
    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);

    // Save API keys
    JsonObject->SetStringField(TEXT("OpenAI_API_Key"), OpenAI_API_Key);
    JsonObject->SetStringField(TEXT("Anthropic_API_Key"), Anthropic_API_Key);
    JsonObject->SetStringField(TEXT("Gemini_API_Key"), Gemini_API_Key);
    JsonObject->SetStringField(TEXT("DeepSeek_API_Key"), DeepSeek_API_Key);

    // Save OAuth tokens in nested format
    SaveOAuthTokensToJson(JsonObject);

    // Serialize to string
    FString JsonString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
    if (!FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer))
    {
        FN2CLogger::Get().LogError(TEXT("Failed to serialize secrets to JSON"));
        return;
    }

    // Save to file
    FString SecretsFilePath = GetSecretsFilePath();
    if (!FFileHelper::SaveStringToFile(JsonString, *SecretsFilePath))
    {
        FN2CLogger::Get().LogError(
            FString::Printf(TEXT("Failed to save secrets to: %s"), *SecretsFilePath));
        return;
    }

    FN2CLogger::Get().Log(
        FString::Printf(TEXT("Successfully saved secrets to: %s"), *SecretsFilePath),
        EN2CLogSeverity::Info);
}

// ============================================
// Unified OAuth Token Storage API
// ============================================

void UN2CUserSecrets::SetOAuthTokensForProvider(EN2COAuthProvider ProviderId, const FN2COAuthTokens& Tokens)
{
    OAuthTokensMap.Add(ProviderId, Tokens);
    SaveSecrets();

    FN2CLogger::Get().Log(
        FString::Printf(TEXT("%s OAuth tokens saved successfully"), *GetProviderKeyName(ProviderId)),
        EN2CLogSeverity::Info);
}

bool UN2CUserSecrets::GetOAuthTokensForProvider(EN2COAuthProvider ProviderId, FN2COAuthTokens& OutTokens) const
{
    const FN2COAuthTokens* FoundTokens = OAuthTokensMap.Find(ProviderId);
    if (FoundTokens)
    {
        OutTokens = *FoundTokens;
        return OutTokens.HasTokens();
    }

    OutTokens = FN2COAuthTokens();
    return false;
}

void UN2CUserSecrets::ClearOAuthTokensForProvider(EN2COAuthProvider ProviderId)
{
    OAuthTokensMap.Remove(ProviderId);
    SaveSecrets();

    FN2CLogger::Get().Log(
        FString::Printf(TEXT("%s OAuth tokens cleared"), *GetProviderKeyName(ProviderId)),
        EN2CLogSeverity::Info);
}

bool UN2CUserSecrets::HasOAuthTokensForProvider(EN2COAuthProvider ProviderId) const
{
    const FN2COAuthTokens* FoundTokens = OAuthTokensMap.Find(ProviderId);
    return FoundTokens && FoundTokens->HasTokens();
}

// ============================================
// Legacy OAuth API (Anthropic/Claude)
// ============================================

void UN2CUserSecrets::SetOAuthTokens(const FString& AccessToken, const FString& RefreshToken,
                                      const FString& ExpiresAt, const FString& Scope)
{
    FN2COAuthTokens Tokens;
    Tokens.AccessToken = AccessToken;
    Tokens.RefreshToken = RefreshToken;
    Tokens.ExpiresAt = ExpiresAt;
    Tokens.Scope = Scope;

    SetOAuthTokensForProvider(EN2COAuthProvider::Anthropic, Tokens);
}

bool UN2CUserSecrets::GetOAuthTokens(FN2COAuthTokens& OutTokens) const
{
    return GetOAuthTokensForProvider(EN2COAuthProvider::Anthropic, OutTokens);
}

void UN2CUserSecrets::ClearOAuthTokens()
{
    ClearOAuthTokensForProvider(EN2COAuthProvider::Anthropic);
}

bool UN2CUserSecrets::HasOAuthTokens() const
{
    return HasOAuthTokensForProvider(EN2COAuthProvider::Anthropic);
}

// ============================================
// Legacy OAuth API (Google/Gemini)
// ============================================

void UN2CUserSecrets::SetGoogleOAuthTokens(const FString& AccessToken, const FString& RefreshToken,
                                            const FString& ExpiresAt, const FString& Scope)
{
    FN2COAuthTokens Tokens;
    Tokens.AccessToken = AccessToken;
    Tokens.RefreshToken = RefreshToken;
    Tokens.ExpiresAt = ExpiresAt;
    Tokens.Scope = Scope;

    SetOAuthTokensForProvider(EN2COAuthProvider::Google, Tokens);
}

bool UN2CUserSecrets::GetGoogleOAuthTokens(FN2COAuthTokens& OutTokens) const
{
    return GetOAuthTokensForProvider(EN2COAuthProvider::Google, OutTokens);
}

void UN2CUserSecrets::ClearGoogleOAuthTokens()
{
    ClearOAuthTokensForProvider(EN2COAuthProvider::Google);
}

bool UN2CUserSecrets::HasGoogleOAuthTokens() const
{
    return HasOAuthTokensForProvider(EN2COAuthProvider::Google);
}
