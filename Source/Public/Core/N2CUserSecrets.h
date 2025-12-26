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
    // OAuth Tokens (Claude Pro/Max)
    // ============================================

    /** OAuth access token for Claude Pro/Max authentication */
    UPROPERTY()
    FString Claude_OAuth_AccessToken;

    /** OAuth refresh token for obtaining new access tokens */
    UPROPERTY()
    FString Claude_OAuth_RefreshToken;

    /** ISO 8601 timestamp when the access token expires */
    UPROPERTY()
    FString Claude_OAuth_ExpiresAt;

    /** OAuth scopes granted by the authorization */
    UPROPERTY()
    FString Claude_OAuth_Scope;

    /**
     * Set OAuth tokens from a token exchange response
     * @param AccessToken - The access token
     * @param RefreshToken - The refresh token
     * @param ExpiresAt - ISO 8601 expiration timestamp
     * @param Scope - Granted scopes
     */
    void SetOAuthTokens(const FString& AccessToken, const FString& RefreshToken,
                        const FString& ExpiresAt, const FString& Scope);

    /**
     * Get OAuth tokens as a struct
     * @param OutTokens - Output token struct
     * @return true if tokens are present
     */
    bool GetOAuthTokens(struct FN2COAuthTokens& OutTokens) const;

    /** Clear all OAuth tokens */
    void ClearOAuthTokens();

    /** Check if OAuth tokens are present */
    bool HasOAuthTokens() const;

    // ============================================
    // OAuth Tokens (Google/Gemini)
    // ============================================

    /** OAuth access token for Google/Gemini authentication */
    UPROPERTY()
    FString Google_OAuth_AccessToken;

    /** OAuth refresh token for obtaining new access tokens */
    UPROPERTY()
    FString Google_OAuth_RefreshToken;

    /** ISO 8601 timestamp when the access token expires */
    UPROPERTY()
    FString Google_OAuth_ExpiresAt;

    /** OAuth scopes granted by the authorization */
    UPROPERTY()
    FString Google_OAuth_Scope;

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
};
