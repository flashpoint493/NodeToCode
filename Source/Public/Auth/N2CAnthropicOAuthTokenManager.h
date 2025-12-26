// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Auth/N2COAuthTokenManagerBase.h"
#include "N2CAnthropicOAuthTokenManager.generated.h"

/**
 * @class UN2CAnthropicOAuthTokenManager
 * @brief OAuth manager for Anthropic/Claude Pro/Max authentication
 *
 * This singleton class implements the Anthropic-specific OAuth 2.0 PKCE flow:
 * - JSON-encoded token request bodies
 * - code#state parsing for authorization callback
 * - 8-hour default token expiry
 */
UCLASS()
class NODETOCODE_API UN2CAnthropicOAuthTokenManager : public UN2COAuthTokenManagerBase
{
	GENERATED_BODY()

public:
	/**
	 * Get the singleton instance of the Anthropic OAuth token manager
	 * @return The token manager instance
	 */
	static UN2CAnthropicOAuthTokenManager* Get();

	/**
	 * Exchange an authorization code for tokens
	 * The code should be in format "code#state" from the redirect URL
	 * @param CodeWithState - The code#state string from the redirect
	 * @param OnComplete - Callback with success status
	 */
	void ExchangeCodeForTokens(const FString& CodeWithState, const FOnTokenExchangeComplete& OnComplete);

	/**
	 * Refresh the access token using the stored refresh token
	 * @param OnComplete - Callback with success status
	 */
	void RefreshAccessToken(const FOnTokenRefreshComplete& OnComplete);

protected:
	// ============================================
	// Provider Configuration
	// ============================================

	virtual FString GetProviderName() const override { return TEXT("Anthropic"); }
	virtual EN2COAuthProvider GetProviderId() const override { return EN2COAuthProvider::Anthropic; }
	virtual const FN2COAuthProviderConfig& GetProviderConfig() const override;
	virtual int32 GetDefaultTokenExpirySeconds() const override { return 28800; } // 8 hours

	// ============================================
	// Anthropic-Specific Customizations
	// ============================================

	/**
	 * Parse authorization code from code#state format
	 */
	virtual bool ParseAuthorizationCode(const FString& Input, FString& OutCode, FString& OutState) override;

	/**
	 * Format token request as JSON body
	 */
	virtual FString FormatTokenRequestBody(const FString& Code) const override;

	/**
	 * Format refresh request as JSON body
	 */
	virtual FString FormatRefreshRequestBody() const override;

	/**
	 * Use application/json Content-Type
	 */
	virtual FString GetTokenRequestContentType() const override { return TEXT("application/json"); }

private:
	/** Singleton instance */
	static UN2CAnthropicOAuthTokenManager* Instance;

	/** Cached provider configuration */
	mutable FN2COAuthProviderConfig ProviderConfig;
	mutable bool bConfigInitialized = false;
};
