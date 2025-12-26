// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Auth/N2COAuthTokenManagerBase.h"
#include "N2CGoogleOAuthTokenManager.generated.h"

/**
 * @class UN2CGoogleOAuthTokenManager
 * @brief OAuth manager for Google/Gemini Code Assist authentication
 *
 * This singleton class implements the Google-specific OAuth 2.0 PKCE flow:
 * - Form-encoded token request bodies
 * - access_type=offline parameter for refresh tokens
 * - 1-hour default token expiry
 * - Code Assist session initialization after authentication
 */
UCLASS()
class NODETOCODE_API UN2CGoogleOAuthTokenManager : public UN2COAuthTokenManagerBase
{
	GENERATED_BODY()

public:
	/**
	 * Get the singleton instance of the Google OAuth token manager
	 * @return The token manager instance
	 */
	static UN2CGoogleOAuthTokenManager* Get();

	/**
	 * Exchange an authorization code for tokens
	 * @param Code - The authorization code from the redirect
	 * @param OnComplete - Callback with success status
	 */
	void ExchangeCodeForTokens(const FString& Code, const FOnTokenExchangeComplete& OnComplete);

	/**
	 * Refresh the access token using the stored refresh token
	 * @param OnComplete - Callback with success status
	 */
	void RefreshAccessToken(const FOnTokenRefreshComplete& OnComplete);

	// ============================================
	// Code Assist Session Methods (Google-specific)
	// ============================================

	/**
	 * Get the Code Assist project ID
	 * Will initialize session synchronously if not already initialized
	 * @return The project ID from loadCodeAssist, or empty if initialization fails
	 */
	FString GetProjectId();

	/**
	 * Check if the Code Assist session is initialized
	 * @return true if loadCodeAssist has been called successfully
	 */
	bool IsSessionInitialized() const { return bSessionInitialized; }

	/**
	 * Ensure the Code Assist session is initialized
	 * Blocks until session is ready or times out
	 * @return true if session is initialized
	 */
	bool EnsureSessionInitialized();

protected:
	// ============================================
	// Provider Configuration
	// ============================================

	virtual FString GetProviderName() const override { return TEXT("Google"); }
	virtual EN2COAuthProvider GetProviderId() const override { return EN2COAuthProvider::Google; }
	virtual const FN2COAuthProviderConfig& GetProviderConfig() const override;
	virtual int32 GetDefaultTokenExpirySeconds() const override { return 3600; } // 1 hour

	// ============================================
	// Google-Specific Customizations
	// ============================================

	/**
	 * Add access_type=offline and prompt=consent parameters
	 */
	virtual FString GetAdditionalAuthUrlParams() const override
	{
		return TEXT("&access_type=offline&prompt=consent");
	}

	/**
	 * Format token request as form-encoded body
	 */
	virtual FString FormatTokenRequestBody(const FString& Code) const override;

	/**
	 * Format refresh request as form-encoded body
	 */
	virtual FString FormatRefreshRequestBody() const override;

	/**
	 * Use application/x-www-form-urlencoded Content-Type
	 */
	virtual FString GetTokenRequestContentType() const override
	{
		return TEXT("application/x-www-form-urlencoded");
	}

	/**
	 * Initialize Code Assist session after successful token exchange
	 */
	virtual void OnTokenExchangeSuccess(TFunction<void(bool)> OnComplete) override;

	/**
	 * Clear Code Assist session state on logout
	 */
	virtual void OnLogoutCleanup() override;

	/**
	 * Restore Code Assist session when loading tokens from storage
	 */
	virtual void OnInitializeWithTokens() override;

private:
	/** Singleton instance */
	static UN2CGoogleOAuthTokenManager* Instance;

	/** Cached provider configuration */
	mutable FN2COAuthProviderConfig ProviderConfig;
	mutable bool bConfigInitialized = false;

	// ============================================
	// Code Assist Session State (Google-specific)
	// ============================================

	/** Cached Code Assist project ID */
	FString CachedProjectId;

	/** Whether the Code Assist session has been initialized */
	bool bSessionInitialized = false;

	/**
	 * Initialize the Code Assist session by calling loadCodeAssist
	 * This must be called after authentication to get a project ID
	 * @param OnComplete - Callback with success status
	 */
	void InitializeCodeAssistSession(TFunction<void(bool)> OnComplete);

	/**
	 * Parse the loadCodeAssist response and extract project ID
	 * @param ResponseJson - The JSON response from loadCodeAssist
	 * @return true if parsing was successful
	 */
	bool ParseLoadCodeAssistResponse(const FString& ResponseJson);
};
