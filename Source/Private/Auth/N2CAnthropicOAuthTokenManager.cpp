// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#include "Auth/N2CAnthropicOAuthTokenManager.h"
#include "Auth/N2COAuthTypes.h"
#include "Serialization/JsonSerializer.h"

// Singleton instance
UN2CAnthropicOAuthTokenManager* UN2CAnthropicOAuthTokenManager::Instance = nullptr;

UN2CAnthropicOAuthTokenManager* UN2CAnthropicOAuthTokenManager::Get()
{
	if (!Instance)
	{
		Instance = NewObject<UN2CAnthropicOAuthTokenManager>();
		Instance->AddToRoot(); // Prevent garbage collection
		Instance->Initialize();
	}
	return Instance;
}

void UN2CAnthropicOAuthTokenManager::ExchangeCodeForTokens(const FString& CodeWithState, const FOnTokenExchangeComplete& OnComplete)
{
	ExchangeCodeForTokensInternal(CodeWithState, [OnComplete](bool bSuccess)
	{
		OnComplete.ExecuteIfBound(bSuccess);
	});
}

void UN2CAnthropicOAuthTokenManager::RefreshAccessToken(const FOnTokenRefreshComplete& OnComplete)
{
	RefreshAccessTokenInternal([OnComplete](bool bSuccess)
	{
		OnComplete.ExecuteIfBound(bSuccess);
	});
}

const FN2COAuthProviderConfig& UN2CAnthropicOAuthTokenManager::GetProviderConfig() const
{
	if (!bConfigInitialized)
	{
		ProviderConfig = FN2COAuthProviderConfig::CreateAnthropicConfig();
		bConfigInitialized = true;
	}
	return ProviderConfig;
}

bool UN2CAnthropicOAuthTokenManager::ParseAuthorizationCode(const FString& Input, FString& OutCode, FString& OutState)
{
	// Anthropic uses code#state format
	int32 HashIndex;
	if (Input.FindChar(TEXT('#'), HashIndex))
	{
		OutCode = Input.Left(HashIndex);
		OutState = Input.Mid(HashIndex + 1);
	}
	else
	{
		// No state found, use input as code
		OutCode = Input;
		OutState = FString();
	}

	return !OutCode.IsEmpty();
}

FString UN2CAnthropicOAuthTokenManager::FormatTokenRequestBody(const FString& Code) const
{
	// Anthropic uses JSON body format with state included
	TSharedPtr<FJsonObject> PayloadJson = MakeShareable(new FJsonObject);
	PayloadJson->SetStringField(TEXT("code"), Code);
	PayloadJson->SetStringField(TEXT("state"), CurrentState);
	PayloadJson->SetStringField(TEXT("grant_type"), TEXT("authorization_code"));
	PayloadJson->SetStringField(TEXT("client_id"), GetProviderConfig().ClientId);
	PayloadJson->SetStringField(TEXT("redirect_uri"), GetProviderConfig().RedirectUri);
	PayloadJson->SetStringField(TEXT("code_verifier"), CurrentVerifier);

	FString PayloadString;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&PayloadString);
	FJsonSerializer::Serialize(PayloadJson.ToSharedRef(), Writer);

	return PayloadString;
}

FString UN2CAnthropicOAuthTokenManager::FormatRefreshRequestBody() const
{
	// Anthropic uses JSON body format for refresh
	TSharedPtr<FJsonObject> PayloadJson = MakeShareable(new FJsonObject);
	PayloadJson->SetStringField(TEXT("grant_type"), TEXT("refresh_token"));
	PayloadJson->SetStringField(TEXT("client_id"), GetProviderConfig().ClientId);
	PayloadJson->SetStringField(TEXT("refresh_token"), CachedTokens.RefreshToken);

	FString PayloadString;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&PayloadString);
	FJsonSerializer::Serialize(PayloadJson.ToSharedRef(), Writer);

	return PayloadString;
}
