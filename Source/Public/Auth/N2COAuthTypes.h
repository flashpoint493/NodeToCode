// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "N2COAuthTypes.generated.h"

/**
 * Delegate broadcast when OAuth authentication state changes.
 * @param bIsAuthenticated - Whether the user is now authenticated
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOAuthStateChanged, bool, bIsAuthenticated);

/**
 * Delegate broadcast when an OAuth error occurs.
 * @param ErrorMessage - Description of the error
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOAuthError, const FString&, ErrorMessage);

/**
 * Delegate for OAuth token exchange completion.
 * @param bSuccess - Whether the exchange was successful
 */
DECLARE_DELEGATE_OneParam(FOnTokenExchangeComplete, bool /* bSuccess */);

/**
 * Delegate for OAuth token refresh completion.
 * @param bSuccess - Whether the refresh was successful
 */
DECLARE_DELEGATE_OneParam(FOnTokenRefreshComplete, bool /* bSuccess */);

/**
 * Authentication method for Anthropic provider.
 * APIKey uses the traditional x-api-key header.
 * OAuth uses Bearer token from Claude Pro/Max subscription.
 */
UENUM(BlueprintType)
enum class EN2CAnthropicAuthMethod : uint8
{
	APIKey	UMETA(DisplayName = "API Key"),
	OAuth	UMETA(DisplayName = "Claude Pro/Max")
};

/**
 * Authentication method for Gemini provider.
 * APIKey uses the traditional API key in URL parameter.
 * OAuth uses Bearer token from Google account.
 */
UENUM(BlueprintType)
enum class EN2CGeminiAuthMethod : uint8
{
	APIKey	UMETA(DisplayName = "API Key"),
	OAuth	UMETA(DisplayName = "Google Account")
};

/**
 * @struct FN2COAuthTokens
 * @brief Container for OAuth 2.0 tokens and metadata
 */
USTRUCT(BlueprintType)
struct FN2COAuthTokens
{
	GENERATED_BODY()

	/** The OAuth access token for API authentication */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OAuth")
	FString AccessToken;

	/** The refresh token for obtaining new access tokens */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OAuth")
	FString RefreshToken;

	/** ISO 8601 timestamp when the access token expires */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OAuth")
	FString ExpiresAt;

	/** OAuth scopes granted by the authorization */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OAuth")
	FString Scope;

	/** Check if tokens are present (doesn't validate expiry) */
	bool HasTokens() const
	{
		return !AccessToken.IsEmpty() && !RefreshToken.IsEmpty();
	}

	/** Clear all token data */
	void Clear()
	{
		AccessToken.Empty();
		RefreshToken.Empty();
		ExpiresAt.Empty();
		Scope.Empty();
	}
};

/**
 * @struct FN2COAuthConstants
 * @brief Static OAuth configuration values for Claude Pro/Max authentication
 */
struct FN2COAuthConstants
{
	/** OAuth client ID for Claude Code */
	static const FString ClientId;

	/** OAuth authorization endpoint */
	static const FString AuthEndpoint;

	/** OAuth token exchange endpoint */
	static const FString TokenEndpoint;

	/** OAuth redirect URI */
	static const FString RedirectUri;

	/** OAuth scopes required for NodeToCode */
	static const FString Scopes;

	/** Required anthropic-beta header value for OAuth */
	static const FString BetaHeader;

	/** Required system prompt prefix for OAuth */
	static const FString SystemPromptPrefix;
};

/**
 * @struct FN2CGoogleOAuthConstants
 * @brief Static OAuth configuration values for Google/Gemini authentication
 */
struct FN2CGoogleOAuthConstants
{
	/** OAuth client ID for gemini-cli */
	static const FString ClientId;

	/** OAuth client secret for gemini-cli */
	static const FString ClientSecret;

	/** OAuth authorization endpoint */
	static const FString AuthEndpoint;

	/** OAuth token exchange endpoint */
	static const FString TokenEndpoint;

	/** OAuth redirect URI (User Code Flow) */
	static const FString RedirectUri;

	/** OAuth scopes required for Gemini API access */
	static const FString Scopes;
};
