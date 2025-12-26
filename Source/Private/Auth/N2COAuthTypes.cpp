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
