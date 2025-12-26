// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Auth/N2COAuthTypes.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "N2CGoogleOAuthTokenManager.generated.h"

class UN2CUserSecrets;

/**
 * @class UN2CGoogleOAuthTokenManager
 * @brief Manages OAuth 2.0 PKCE authentication flow for Google/Gemini
 *
 * This singleton class handles:
 * - PKCE code generation (verifier and challenge)
 * - Authorization URL generation
 * - Token exchange and refresh (form-encoded body)
 * - Automatic token refresh scheduling
 * - Token persistence via UN2CUserSecrets
 */
UCLASS()
class NODETOCODE_API UN2CGoogleOAuthTokenManager : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Get the singleton instance of the Google OAuth token manager
	 * @return The token manager instance
	 */
	static UN2CGoogleOAuthTokenManager* Get();

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
	 * @param Code - The authorization code from the redirect
	 * @param OnComplete - Callback with success status
	 */
	void ExchangeCodeForTokens(const FString& Code, const FOnTokenExchangeComplete& OnComplete);

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
	static UN2CGoogleOAuthTokenManager* Instance;

	/** User secrets for token storage */
	UPROPERTY()
	UN2CUserSecrets* UserSecrets;

	/** Current PKCE verifier (stored during auth flow) */
	FString CurrentVerifier;

	/** Current state for CSRF protection */
	FString CurrentState;

	/** Cached tokens loaded from storage */
	FN2COAuthTokens CachedTokens;

	/** Cached Code Assist project ID */
	FString CachedProjectId;

	/** Whether the Code Assist session has been initialized */
	bool bSessionInitialized = false;

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

	// ============================================
	// Code Assist Session Management
	// ============================================

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
