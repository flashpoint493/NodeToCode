// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Auth/N2COAuthTypes.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "N2COAuthTokenManagerBase.generated.h"

class UN2CUserSecrets;

/**
 * @class UN2COAuthTokenManagerBase
 * @brief Abstract base class for OAuth 2.0 PKCE authentication managers
 *
 * Uses Template Method Pattern to define the OAuth flow algorithm
 * while allowing derived classes to customize provider-specific behavior.
 *
 * This class handles:
 * - PKCE code generation (verifier and challenge)
 * - Authorization URL generation
 * - Token exchange and refresh
 * - Automatic token refresh scheduling
 * - Token persistence via UN2CUserSecrets
 *
 * Derived classes must implement:
 * - GetProviderName() - Provider identifier for logging
 * - GetProviderId() - Provider enum for storage
 * - GetProviderConfig() - OAuth configuration values
 * - FormatTokenRequestBody() - JSON vs form-encoded body
 * - FormatRefreshRequestBody() - Refresh request body format
 * - GetTokenRequestContentType() - Content-Type header value
 */
UCLASS(Abstract)
class NODETOCODE_API UN2COAuthTokenManagerBase : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Initialize the token manager
	 * Loads any existing tokens from storage
	 */
	virtual void Initialize();

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
	virtual void Logout();

	// ============================================
	// Delegates
	// ============================================

	/** Broadcast when authentication state changes */
	UPROPERTY(BlueprintAssignable, Category = "OAuth")
	FOnOAuthStateChanged OnAuthStateChanged;

	/** Broadcast when an OAuth error occurs */
	UPROPERTY(BlueprintAssignable, Category = "OAuth")
	FOnOAuthError OnError;

protected:
	// ============================================
	// Provider Configuration (Must Override)
	// ============================================

	/**
	 * Get the provider identifier for logging
	 * @return Provider name (e.g., "Anthropic", "Google")
	 */
	virtual FString GetProviderName() const { return TEXT("Unknown"); }

	/**
	 * Get the provider ID for storage keys
	 * @return Provider enum value
	 */
	virtual EN2COAuthProvider GetProviderId() const { return EN2COAuthProvider::Anthropic; }

	/**
	 * Get OAuth configuration for this provider
	 * @return Provider configuration struct
	 */
	virtual const FN2COAuthProviderConfig& GetProviderConfig() const
	{
		static FN2COAuthProviderConfig DefaultConfig;
		return DefaultConfig;
	}

	/**
	 * Get default token expiry in seconds if not provided in response
	 * @return Default expiry time in seconds
	 */
	virtual int32 GetDefaultTokenExpirySeconds() const { return 3600; }

	// ============================================
	// Customization Points (Virtual with Defaults)
	// ============================================

	/**
	 * Build additional auth URL parameters (e.g., access_type=offline)
	 * @return Additional URL parameters starting with &
	 */
	virtual FString GetAdditionalAuthUrlParams() const { return FString(); }

	/**
	 * Parse the authorization code from callback input
	 * Default implementation just returns the input as the code
	 * Override for code#state format parsing
	 * @param Input - The input string from callback
	 * @param OutCode - The extracted authorization code
	 * @param OutState - The extracted state (if present)
	 * @return true if parsing succeeded
	 */
	virtual bool ParseAuthorizationCode(const FString& Input, FString& OutCode, FString& OutState);

	/**
	 * Format the token exchange request body
	 * @param Code - The authorization code
	 * @return Formatted request body (JSON or form-encoded)
	 */
	virtual FString FormatTokenRequestBody(const FString& Code) const { return FString(); }

	/**
	 * Format the refresh token request body
	 * @return Formatted request body (JSON or form-encoded)
	 */
	virtual FString FormatRefreshRequestBody() const { return FString(); }

	/**
	 * Get Content-Type header for token requests
	 * @return Content-Type value (e.g., "application/json" or "application/x-www-form-urlencoded")
	 */
	virtual FString GetTokenRequestContentType() const { return TEXT("application/json"); }

	/**
	 * Called after successful token exchange
	 * Override for post-auth initialization (e.g., Code Assist session)
	 * @param OnComplete - Callback when post-auth actions complete
	 */
	virtual void OnTokenExchangeSuccess(TFunction<void(bool)> OnComplete);

	/**
	 * Called during logout for provider-specific cleanup
	 */
	virtual void OnLogoutCleanup() {}

	/**
	 * Provider-specific initialization after loading tokens from storage
	 */
	virtual void OnInitializeWithTokens() {}

	// ============================================
	// Token Exchange (Called by derived classes)
	// ============================================

	/**
	 * Exchange an authorization code for tokens
	 * @param CodeInput - The authorization code (may include state for some providers)
	 * @param OnComplete - Callback with success status
	 */
	void ExchangeCodeForTokensInternal(const FString& CodeInput, TFunction<void(bool)> OnComplete);

	/**
	 * Refresh the access token using the stored refresh token
	 * @param OnComplete - Callback with success status
	 */
	void RefreshAccessTokenInternal(TFunction<void(bool)> OnComplete);

	// ============================================
	// Token Storage (Virtual - uses provider-specific storage)
	// ============================================

	virtual void LoadTokensFromStorage();
	virtual void SaveTokensToStorage();

	// ============================================
	// Shared State
	// ============================================

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

private:
	// ============================================
	// PKCE Helpers (Static, private)
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
	// Token Management (Private)
	// ============================================

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
	 * @param bIsExchange - Whether this is an exchange (vs refresh)
	 * @param OnComplete - Callback to invoke with result
	 */
	void HandleTokenResponse(FHttpResponsePtr Response, bool bSuccess, bool bIsExchange,
		TFunction<void(bool)> OnComplete);
};
