// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Auth/N2COAuthTypes.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "N2COAuthTokenManager.generated.h"

class UN2CUserSecrets;

/**
 * @class UN2COAuthTokenManager
 * @brief Manages OAuth 2.0 PKCE authentication flow for Claude Pro/Max subscriptions
 *
 * This singleton class handles:
 * - PKCE code generation (verifier and challenge)
 * - Authorization URL generation
 * - Token exchange and refresh
 * - Automatic token refresh scheduling
 * - Token persistence via UN2CUserSecrets
 */
UCLASS()
class NODETOCODE_API UN2COAuthTokenManager : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Get the singleton instance of the OAuth token manager
	 * @return The token manager instance
	 */
	static UN2COAuthTokenManager* Get();

	/**
	 * Initialize the token manager
	 * Loads any existing tokens from storage
	 */
	void Initialize();

	// ============================================
	// OAuth Flow Methods
	// ============================================

	/**
	 * Generate the OAuth authorization URL for browser redirect
	 * Also generates and stores PKCE verifier/challenge
	 * @return The full authorization URL to open in browser
	 */
	FString GenerateAuthorizationUrl();

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

	/**
	 * Synchronously refresh the access token (blocks until complete)
	 * @return true if refresh was successful
	 */
	bool RefreshAccessTokenSync();

	// ============================================
	// Token Access Methods
	// ============================================

	/**
	 * Get the current access token
	 * Automatically refreshes if expired
	 * @return The access token, or empty string if not authenticated
	 */
	FString GetAccessToken();

	/**
	 * Check if the user is authenticated with valid tokens
	 * @return true if tokens are present
	 */
	bool IsAuthenticated() const;

	/**
	 * Check if the current access token is expired
	 * @return true if expired or no token exists
	 */
	bool IsTokenExpired() const;

	/**
	 * Get the token expiration time as a readable string
	 * @return Formatted expiration time or "Not authenticated"
	 */
	FString GetExpirationTimeString() const;

	/**
	 * Clear all tokens and log out
	 */
	void Logout();

	// ============================================
	// Delegates
	// ============================================

	/** Broadcast when authentication state changes */
	UPROPERTY(BlueprintAssignable, Category = "OAuth")
	FOnOAuthStateChanged OnAuthStateChanged;

	/** Broadcast when an OAuth error occurs */
	UPROPERTY(BlueprintAssignable, Category = "OAuth")
	FOnOAuthError OnError;

private:
	/** Singleton instance */
	static UN2COAuthTokenManager* Instance;

	/** User secrets for token storage */
	UPROPERTY()
	UN2CUserSecrets* UserSecrets;

	/** Current PKCE verifier (stored during auth flow) */
	FString CurrentVerifier;

	/** Current state for CSRF protection */
	FString CurrentState;

	/** Cached tokens loaded from storage */
	FN2COAuthTokens CachedTokens;

	/** Timer handle for automatic token refresh */
	FTimerHandle RefreshTimerHandle;

	// ============================================
	// PKCE Helper Methods
	// ============================================

	/**
	 * Generate a cryptographically random PKCE verifier
	 * @return Base64URL encoded verifier (43-128 chars)
	 */
	static FString GenerateVerifier();

	/**
	 * Generate a PKCE challenge from a verifier using SHA-256
	 * @param Verifier - The verifier to hash
	 * @return Base64URL encoded challenge
	 */
	static FString GenerateChallenge(const FString& Verifier);

	/**
	 * Generate a random state string for CSRF protection
	 * @return Random UUID string
	 */
	static FString GenerateState();

	/**
	 * Encode bytes to Base64URL (no padding)
	 * @param Bytes - Data to encode
	 * @return Base64URL encoded string
	 */
	static FString Base64UrlEncode(const TArray<uint8>& Bytes);

	// ============================================
	// Token Management
	// ============================================

	/**
	 * Load tokens from storage into cache
	 */
	void LoadTokensFromStorage();

	/**
	 * Save cached tokens to storage
	 */
	void SaveTokensToStorage();

	/**
	 * Schedule automatic token refresh before expiration
	 */
	void ScheduleTokenRefresh();

	/**
	 * Cancel any pending token refresh
	 */
	void CancelTokenRefresh();

	/**
	 * Parse token response JSON and update cached tokens
	 * @param ResponseJson - The JSON response from token endpoint
	 * @return true if parsing was successful
	 */
	bool ParseTokenResponse(const FString& ResponseJson);

	/**
	 * Handle HTTP response from token exchange/refresh
	 * @param Response - HTTP response object
	 * @param bSuccess - Whether the request succeeded
	 * @param OnComplete - Callback to invoke with result
	 */
	void HandleTokenResponse(FHttpResponsePtr Response, bool bSuccess,
		TFunction<void(bool)> OnComplete);
};
