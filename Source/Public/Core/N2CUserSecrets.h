// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Auth/N2COAuthTypes.h"
#include "N2CUserSecrets.generated.h"

/**
 * @class UN2CUserSecrets
 * @brief Stores sensitive configuration data like API keys
 *
 * Uses a custom JSON storage system instead of Unreal's config system
 * to ensure consistent behavior across engine versions.
 *
 * OAuth tokens are stored in a nested structure for easy provider extension:
 * {
 *     "OpenAI_API_Key": "...",
 *     "OAuth": {
 *         "Anthropic": { "AccessToken": "...", "RefreshToken": "...", ... },
 *         "Google": { "AccessToken": "...", "RefreshToken": "...", ... }
 *     }
 * }
 */

UCLASS()
class NODETOCODE_API UN2CUserSecrets : public UObject
{
    GENERATED_BODY()
public:
    UN2CUserSecrets();

    /** Load API keys from storage */
    void LoadSecrets();

    /** Save API keys to storage */
    void SaveSecrets();

    /** Get the path to the secrets file */
    static FString GetSecretsFilePath();

    /** OpenAI API Key */
    UPROPERTY(EditAnywhere, Category = "Node to Code | API Keys")
    FString OpenAI_API_Key;

    /** Anthropic API Key */
    UPROPERTY(EditAnywhere, Category = "Node to Code | API Keys")
    FString Anthropic_API_Key;

    /** Gemini API Key */
    UPROPERTY(EditAnywhere, Category = "Node to Code | API Keys")
    FString Gemini_API_Key;

    /** DeepSeek API Key */
    UPROPERTY(EditAnywhere, Category = "Node to Code | API Keys")
    FString DeepSeek_API_Key;

    // ============================================
    // Unified OAuth Token Storage API
    // ============================================

    /**
     * Set OAuth tokens for a specific provider
     * @param ProviderId - The OAuth provider identifier
     * @param Tokens - The tokens to store
     */
    void SetOAuthTokensForProvider(EN2COAuthProvider ProviderId, const FN2COAuthTokens& Tokens);

    /**
     * Get OAuth tokens for a specific provider
     * @param ProviderId - The OAuth provider identifier
     * @param OutTokens - Output token struct
     * @return true if tokens are present
     */
    bool GetOAuthTokensForProvider(EN2COAuthProvider ProviderId, FN2COAuthTokens& OutTokens) const;

    /**
     * Clear OAuth tokens for a specific provider
     * @param ProviderId - The OAuth provider identifier
     */
    void ClearOAuthTokensForProvider(EN2COAuthProvider ProviderId);

    /**
     * Check if OAuth tokens are present for a specific provider
     * @param ProviderId - The OAuth provider identifier
     * @return true if tokens are present
     */
    bool HasOAuthTokensForProvider(EN2COAuthProvider ProviderId) const;

    // ============================================
    // Legacy OAuth API (Anthropic/Claude)
    // These methods delegate to the unified API
    // ============================================

    /**
     * Set OAuth tokens from a token exchange response (Claude/Anthropic)
     * @param AccessToken - The access token
     * @param RefreshToken - The refresh token
     * @param ExpiresAt - ISO 8601 expiration timestamp
     * @param Scope - Granted scopes
     */
    void SetOAuthTokens(const FString& AccessToken, const FString& RefreshToken,
                        const FString& ExpiresAt, const FString& Scope);

    /**
     * Get OAuth tokens as a struct (Claude/Anthropic)
     * @param OutTokens - Output token struct
     * @return true if tokens are present
     */
    bool GetOAuthTokens(struct FN2COAuthTokens& OutTokens) const;

    /** Clear all OAuth tokens (Claude/Anthropic) */
    void ClearOAuthTokens();

    /** Check if OAuth tokens are present (Claude/Anthropic) */
    bool HasOAuthTokens() const;

    // ============================================
    // Legacy OAuth API (Google/Gemini)
    // These methods delegate to the unified API
    // ============================================

    /**
     * Set Google OAuth tokens from a token exchange response
     * @param AccessToken - The access token
     * @param RefreshToken - The refresh token
     * @param ExpiresAt - ISO 8601 expiration timestamp
     * @param Scope - Granted scopes
     */
    void SetGoogleOAuthTokens(const FString& AccessToken, const FString& RefreshToken,
                              const FString& ExpiresAt, const FString& Scope);

    /**
     * Get Google OAuth tokens as a struct
     * @param OutTokens - Output token struct
     * @return true if tokens are present
     */
    bool GetGoogleOAuthTokens(struct FN2COAuthTokens& OutTokens) const;

    /** Clear all Google OAuth tokens */
    void ClearGoogleOAuthTokens();

    /** Check if Google OAuth tokens are present */
    bool HasGoogleOAuthTokens() const;

private:
    /** Ensure the secrets directory exists */
    static void EnsureSecretsDirectoryExists();

    /** Get the JSON key name for a provider */
    static FString GetProviderKeyName(EN2COAuthProvider ProviderId);

    /** OAuth tokens storage map (provider -> tokens) */
    TMap<EN2COAuthProvider, FN2COAuthTokens> OAuthTokensMap;

    /** Load OAuth tokens from a JSON object (handles both legacy and new formats) */
    void LoadOAuthTokensFromJson(const TSharedPtr<FJsonObject>& JsonObject);

    /** Save OAuth tokens to a JSON object */
    void SaveOAuthTokensToJson(TSharedPtr<FJsonObject>& JsonObject) const;
};
