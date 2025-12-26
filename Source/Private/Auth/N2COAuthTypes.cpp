// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#include "Auth/N2COAuthTypes.h"

// OAuth client ID for Claude Code
const FString FN2COAuthConstants::ClientId = TEXT("9d1c250a-e61b-44d9-88ed-5944d1962f5e");

// OAuth authorization endpoint
const FString FN2COAuthConstants::AuthEndpoint = TEXT("https://claude.ai/oauth/authorize");

// OAuth token exchange endpoint
const FString FN2COAuthConstants::TokenEndpoint = TEXT("https://console.anthropic.com/v1/oauth/token");

// OAuth redirect URI
const FString FN2COAuthConstants::RedirectUri = TEXT("https://console.anthropic.com/oauth/code/callback");

// OAuth scopes required for NodeToCode
const FString FN2COAuthConstants::Scopes = TEXT("org:create_api_key user:profile user:inference");

// Required anthropic-beta header value for OAuth (all four betas required per OpenCode implementation)
const FString FN2COAuthConstants::BetaHeader = TEXT("oauth-2025-04-20,claude-code-20250219,interleaved-thinking-2025-05-14,fine-grained-tool-streaming-2025-05-14");

// Required system prompt prefix for OAuth
const FString FN2COAuthConstants::SystemPromptPrefix = TEXT("You are Claude Code, Anthropic's official CLI for Claude.");

// ============================================
// Google OAuth Constants (for Gemini)
// ============================================

// OAuth client ID for gemini-cli
const FString FN2CGoogleOAuthConstants::ClientId = TEXT("GOOGLE_OAUTH_CLIENT_ID");

// OAuth client secret for gemini-cli
const FString FN2CGoogleOAuthConstants::ClientSecret = TEXT("GOOGLE_OAUTH_CLIENT_SECRET");

// OAuth authorization endpoint
const FString FN2CGoogleOAuthConstants::AuthEndpoint = TEXT("https://accounts.google.com/o/oauth2/v2/auth");

// OAuth token exchange endpoint
const FString FN2CGoogleOAuthConstants::TokenEndpoint = TEXT("https://oauth2.googleapis.com/token");

// OAuth redirect URI (User Code Flow)
const FString FN2CGoogleOAuthConstants::RedirectUri = TEXT("https://codeassist.google.com/authcode");

// OAuth scopes required for Gemini API access
const FString FN2CGoogleOAuthConstants::Scopes = TEXT("https://www.googleapis.com/auth/cloud-platform https://www.googleapis.com/auth/userinfo.email https://www.googleapis.com/auth/userinfo.profile");

// ============================================
// Provider Config Factory Methods
// ============================================

FN2COAuthProviderConfig FN2COAuthProviderConfig::CreateAnthropicConfig()
{
	FN2COAuthProviderConfig Config;
	Config.ClientId = FN2COAuthConstants::ClientId;
	Config.ClientSecret = FString(); // Anthropic doesn't require client secret
	Config.AuthEndpoint = FN2COAuthConstants::AuthEndpoint;
	Config.TokenEndpoint = FN2COAuthConstants::TokenEndpoint;
	Config.RedirectUri = FN2COAuthConstants::RedirectUri;
	Config.Scopes = FN2COAuthConstants::Scopes;
	return Config;
}

FN2COAuthProviderConfig FN2COAuthProviderConfig::CreateGoogleConfig()
{
	FN2COAuthProviderConfig Config;
	Config.ClientId = FN2CGoogleOAuthConstants::ClientId;
	Config.ClientSecret = FN2CGoogleOAuthConstants::ClientSecret;
	Config.AuthEndpoint = FN2CGoogleOAuthConstants::AuthEndpoint;
	Config.TokenEndpoint = FN2CGoogleOAuthConstants::TokenEndpoint;
	Config.RedirectUri = FN2CGoogleOAuthConstants::RedirectUri;
	Config.Scopes = FN2CGoogleOAuthConstants::Scopes;
	return Config;
}
