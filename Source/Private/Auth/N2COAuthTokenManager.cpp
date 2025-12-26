// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#include "Auth/N2COAuthTokenManager.h"
#include "Core/N2CUserSecrets.h"
#include "Utils/N2CLogger.h"

#include "GenericPlatform/GenericPlatformHttp.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/Base64.h"
#include "Misc/DateTime.h"
#include "Misc/Guid.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "TimerManager.h"
#include "Async/TaskGraphInterfaces.h"

// Windows BCrypt for SHA-256 (PKCE requirement)
#include "Windows/AllowWindowsPlatformTypes.h"
#include <bcrypt.h>
#include "Windows/HideWindowsPlatformTypes.h"

#pragma comment(lib, "bcrypt.lib")

// SHA-256 implementation for PKCE challenge generation using Windows BCrypt
namespace
{
	TArray<uint8> ComputeSHA256(const TArray<uint8>& Data)
	{
		TArray<uint8> Hash;
		Hash.SetNumUninitialized(32);

		BCRYPT_ALG_HANDLE hAlg = nullptr;
		NTSTATUS Status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
		if (BCRYPT_SUCCESS(Status))
		{
			BCRYPT_HASH_HANDLE hHash = nullptr;
			Status = BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0);
			if (BCRYPT_SUCCESS(Status))
			{
				BCryptHashData(hHash, (PUCHAR)Data.GetData(), Data.Num(), 0);
				BCryptFinishHash(hHash, Hash.GetData(), 32, 0);
				BCryptDestroyHash(hHash);
			}
			BCryptCloseAlgorithmProvider(hAlg, 0);
		}

		return Hash;
	}
}

// Singleton instance
UN2COAuthTokenManager* UN2COAuthTokenManager::Instance = nullptr;

UN2COAuthTokenManager* UN2COAuthTokenManager::Get()
{
	if (!Instance)
	{
		Instance = NewObject<UN2COAuthTokenManager>();
		Instance->AddToRoot(); // Prevent garbage collection
		Instance->Initialize();
	}
	return Instance;
}

void UN2COAuthTokenManager::Initialize()
{
	// Get or create user secrets
	UserSecrets = NewObject<UN2CUserSecrets>();
	UserSecrets->LoadSecrets();

	// Load existing tokens
	LoadTokensFromStorage();

	// Schedule refresh if we have valid tokens
	if (IsAuthenticated() && !IsTokenExpired())
	{
		ScheduleTokenRefresh();
	}

	FN2CLogger::Get().Log(TEXT("OAuth Token Manager initialized"), EN2CLogSeverity::Info);
}

FString UN2COAuthTokenManager::GenerateAuthorizationUrl()
{
	// Generate PKCE values
	CurrentVerifier = GenerateVerifier();
	CurrentState = GenerateState();
	FString Challenge = GenerateChallenge(CurrentVerifier);

	// Build authorization URL
	FString AuthUrl = FString::Printf(
		TEXT("%s?response_type=code&client_id=%s&redirect_uri=%s&scope=%s&code_challenge=%s&code_challenge_method=S256&state=%s"),
		*FN2COAuthConstants::AuthEndpoint,
		*FN2COAuthConstants::ClientId,
		*FGenericPlatformHttp::UrlEncode(FN2COAuthConstants::RedirectUri),
		*FGenericPlatformHttp::UrlEncode(FN2COAuthConstants::Scopes),
		*Challenge,
		*CurrentState
	);

	FN2CLogger::Get().Log(TEXT("Generated OAuth authorization URL"), EN2CLogSeverity::Debug);

	return AuthUrl;
}

void UN2COAuthTokenManager::ExchangeCodeForTokens(const FString& CodeWithState, const FOnTokenExchangeComplete& OnComplete)
{
	// Parse code#state format
	FString Code;
	FString State;

	int32 HashIndex;
	if (CodeWithState.FindChar(TEXT('#'), HashIndex))
	{
		Code = CodeWithState.Left(HashIndex);
		State = CodeWithState.Mid(HashIndex + 1);
	}
	else
	{
		// Try just using the whole thing as a code
		Code = CodeWithState;
	}

	// Validate state if provided
	if (!State.IsEmpty() && State != CurrentState)
	{
		FN2CLogger::Get().LogError(TEXT("OAuth state mismatch - possible CSRF attack"));
		OnError.Broadcast(TEXT("Security error: State mismatch. Please try again."));
		OnComplete.ExecuteIfBound(false);
		return;
	}

	if (Code.IsEmpty())
	{
		FN2CLogger::Get().LogError(TEXT("OAuth code is empty"));
		OnError.Broadcast(TEXT("Invalid authorization code"));
		OnComplete.ExecuteIfBound(false);
		return;
	}

	if (CurrentVerifier.IsEmpty())
	{
		FN2CLogger::Get().LogError(TEXT("No PKCE verifier found - auth flow may not have been initiated properly"));
		OnError.Broadcast(TEXT("Authentication error: Please start the login flow again."));
		OnComplete.ExecuteIfBound(false);
		return;
	}

	// Build token request payload as JSON (state is required by Anthropic's endpoint)
	TSharedPtr<FJsonObject> PayloadJson = MakeShareable(new FJsonObject);
	PayloadJson->SetStringField(TEXT("code"), Code);
	PayloadJson->SetStringField(TEXT("state"), State.IsEmpty() ? CurrentState : State);
	PayloadJson->SetStringField(TEXT("grant_type"), TEXT("authorization_code"));
	PayloadJson->SetStringField(TEXT("client_id"), FN2COAuthConstants::ClientId);
	PayloadJson->SetStringField(TEXT("redirect_uri"), FN2COAuthConstants::RedirectUri);
	PayloadJson->SetStringField(TEXT("code_verifier"), CurrentVerifier);

	FString PayloadString;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&PayloadString);
	FJsonSerializer::Serialize(PayloadJson.ToSharedRef(), Writer);

	FN2CLogger::Get().Log(FString::Printf(TEXT("Token request payload: %s"), *PayloadString), EN2CLogSeverity::Debug);
	FN2CLogger::Get().Log(FString::Printf(TEXT("Token endpoint: %s"), *FN2COAuthConstants::TokenEndpoint), EN2CLogSeverity::Debug);

	// Create HTTP request (matching OpenCode's minimal headers)
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(FN2COAuthConstants::TokenEndpoint);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetContentAsString(PayloadString);

	// Weak reference for lambda capture
	TWeakObjectPtr<UN2COAuthTokenManager> WeakThis(this);

	Request->OnProcessRequestComplete().BindLambda(
		[WeakThis, OnComplete](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
		{
			if (UN2COAuthTokenManager* This = WeakThis.Get())
			{
				This->HandleTokenResponse(Response, bConnectedSuccessfully,
					[OnComplete](bool bSuccess)
					{
						OnComplete.ExecuteIfBound(bSuccess);
					});
			}
		});

	Request->ProcessRequest();

	FN2CLogger::Get().Log(TEXT("Exchanging authorization code for tokens..."), EN2CLogSeverity::Info);
}

void UN2COAuthTokenManager::RefreshAccessToken(const FOnTokenRefreshComplete& OnComplete)
{
	if (CachedTokens.RefreshToken.IsEmpty())
	{
		FN2CLogger::Get().LogError(TEXT("No refresh token available"));
		OnError.Broadcast(TEXT("No refresh token. Please log in again."));
		OnComplete.ExecuteIfBound(false);
		return;
	}

	// Build refresh request payload as JSON
	TSharedPtr<FJsonObject> PayloadJson = MakeShareable(new FJsonObject);
	PayloadJson->SetStringField(TEXT("grant_type"), TEXT("refresh_token"));
	PayloadJson->SetStringField(TEXT("client_id"), FN2COAuthConstants::ClientId);
	PayloadJson->SetStringField(TEXT("refresh_token"), CachedTokens.RefreshToken);

	FString PayloadString;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&PayloadString);
	FJsonSerializer::Serialize(PayloadJson.ToSharedRef(), Writer);

	// Create HTTP request (matching OpenCode's minimal headers)
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(FN2COAuthConstants::TokenEndpoint);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetContentAsString(PayloadString);

	// Weak reference for lambda capture
	TWeakObjectPtr<UN2COAuthTokenManager> WeakThis(this);

	Request->OnProcessRequestComplete().BindLambda(
		[WeakThis, OnComplete](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
		{
			if (UN2COAuthTokenManager* This = WeakThis.Get())
			{
				This->HandleTokenResponse(Response, bConnectedSuccessfully,
					[OnComplete](bool bSuccess)
					{
						OnComplete.ExecuteIfBound(bSuccess);
					});
			}
		});

	Request->ProcessRequest();

	FN2CLogger::Get().Log(TEXT("Refreshing access token..."), EN2CLogSeverity::Info);
}

bool UN2COAuthTokenManager::RefreshAccessTokenSync()
{
	// Use a blocking approach for synchronous refresh
	volatile bool bCompleted = false;
	volatile bool bSuccess = false;

	RefreshAccessToken(FOnTokenRefreshComplete::CreateLambda(
		[&bCompleted, &bSuccess](bool bResult)
		{
			bSuccess = bResult;
			bCompleted = true;
		}));

	// Wait for completion (with timeout)
	const double TimeoutSeconds = 30.0;
	const double StartTime = FPlatformTime::Seconds();

	while (!bCompleted)
	{
		if (FPlatformTime::Seconds() - StartTime > TimeoutSeconds)
		{
			FN2CLogger::Get().LogError(TEXT("Token refresh timed out"));
			return false;
		}

		// Process pending tasks on game thread to allow HTTP callbacks to execute
		if (FTaskGraphInterface::IsRunning())
		{
			FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
		}
		FPlatformProcess::Sleep(0.05f);
	}

	return bSuccess;
}

FString UN2COAuthTokenManager::GetAccessToken()
{
	if (!IsAuthenticated())
	{
		return FString();
	}

	// If token is expired, try to refresh synchronously
	if (IsTokenExpired())
	{
		FN2CLogger::Get().Log(TEXT("Access token expired, attempting refresh..."), EN2CLogSeverity::Info);
		if (!RefreshAccessTokenSync())
		{
			return FString();
		}
	}

	return CachedTokens.AccessToken;
}

bool UN2COAuthTokenManager::IsAuthenticated() const
{
	return CachedTokens.HasTokens();
}

bool UN2COAuthTokenManager::IsTokenExpired() const
{
	if (CachedTokens.ExpiresAt.IsEmpty())
	{
		return true;
	}

	FDateTime ExpiryTime;
	if (!FDateTime::ParseIso8601(*CachedTokens.ExpiresAt, ExpiryTime))
	{
		return true;
	}

	// Consider expired if less than 5 minutes remaining
	return (ExpiryTime - FDateTime::UtcNow()).GetTotalMinutes() < 5.0;
}

FString UN2COAuthTokenManager::GetExpirationTimeString() const
{
	if (!IsAuthenticated())
	{
		return TEXT("Not authenticated");
	}

	FDateTime ExpiryTime;
	if (!FDateTime::ParseIso8601(*CachedTokens.ExpiresAt, ExpiryTime))
	{
		return TEXT("Unknown");
	}

	return ExpiryTime.ToString();
}

void UN2COAuthTokenManager::Logout()
{
	CancelTokenRefresh();
	CachedTokens.Clear();
	CurrentVerifier.Empty();
	CurrentState.Empty();

	if (UserSecrets)
	{
		UserSecrets->ClearOAuthTokens();
	}

	OnAuthStateChanged.Broadcast(false);

	FN2CLogger::Get().Log(TEXT("OAuth logout complete"), EN2CLogSeverity::Info);
}

// ============================================
// PKCE Helper Methods
// ============================================

FString UN2COAuthTokenManager::GenerateVerifier()
{
	// Generate 32 random bytes
	TArray<uint8> RandomBytes;
	RandomBytes.SetNumUninitialized(32);

	for (int32 i = 0; i < 32; i++)
	{
		RandomBytes[i] = static_cast<uint8>(FMath::RandRange(0, 255));
	}

	return Base64UrlEncode(RandomBytes);
}

FString UN2COAuthTokenManager::GenerateChallenge(const FString& Verifier)
{
	// Convert verifier to UTF-8 bytes
	FTCHARToUTF8 VerifierUtf8(*Verifier);
	TArray<uint8> VerifierBytes;
	VerifierBytes.SetNumUninitialized(VerifierUtf8.Length());
	FMemory::Memcpy(VerifierBytes.GetData(), VerifierUtf8.Get(), VerifierUtf8.Length());

	// SHA-256 hash of verifier
	TArray<uint8> HashBytes = ComputeSHA256(VerifierBytes);

	return Base64UrlEncode(HashBytes);
}

FString UN2COAuthTokenManager::GenerateState()
{
	return FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
}

FString UN2COAuthTokenManager::Base64UrlEncode(const TArray<uint8>& Bytes)
{
	// Standard Base64 encode
	FString Base64 = FBase64::Encode(Bytes);

	// Convert to Base64URL (replace + with -, / with _, remove padding =)
	Base64.ReplaceInline(TEXT("+"), TEXT("-"));
	Base64.ReplaceInline(TEXT("/"), TEXT("_"));
	Base64.ReplaceInline(TEXT("="), TEXT(""));

	return Base64;
}

// ============================================
// Token Management
// ============================================

void UN2COAuthTokenManager::LoadTokensFromStorage()
{
	if (UserSecrets)
	{
		UserSecrets->GetOAuthTokens(CachedTokens);
	}
}

void UN2COAuthTokenManager::SaveTokensToStorage()
{
	if (UserSecrets)
	{
		UserSecrets->SetOAuthTokens(
			CachedTokens.AccessToken,
			CachedTokens.RefreshToken,
			CachedTokens.ExpiresAt,
			CachedTokens.Scope
		);
	}
}

void UN2COAuthTokenManager::ScheduleTokenRefresh()
{
	CancelTokenRefresh();

	if (CachedTokens.ExpiresAt.IsEmpty())
	{
		return;
	}

	FDateTime ExpiryTime;
	if (!FDateTime::ParseIso8601(*CachedTokens.ExpiresAt, ExpiryTime))
	{
		return;
	}

	// Refresh 5 minutes before expiry
	FTimespan TimeUntilRefresh = (ExpiryTime - FDateTime::UtcNow()) - FTimespan::FromMinutes(5);

	if (TimeUntilRefresh.GetTotalSeconds() > 0)
	{
		if (GEngine && GEngine->GetWorld())
		{
			GEngine->GetWorld()->GetTimerManager().SetTimer(
				RefreshTimerHandle,
				FTimerDelegate::CreateWeakLambda(this, [this]()
				{
					RefreshAccessToken(FOnTokenRefreshComplete::CreateLambda([](bool bSuccess)
					{
						if (!bSuccess)
						{
							FN2CLogger::Get().LogError(TEXT("Automatic token refresh failed"));
						}
					}));
				}),
				TimeUntilRefresh.GetTotalSeconds(),
				false
			);

			FN2CLogger::Get().Log(
				FString::Printf(TEXT("Token refresh scheduled in %.0f minutes"), TimeUntilRefresh.GetTotalMinutes()),
				EN2CLogSeverity::Debug
			);
		}
	}
}

void UN2COAuthTokenManager::CancelTokenRefresh()
{
	if (GEngine && GEngine->GetWorld())
	{
		GEngine->GetWorld()->GetTimerManager().ClearTimer(RefreshTimerHandle);
	}
}

bool UN2COAuthTokenManager::ParseTokenResponse(const FString& ResponseJson)
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseJson);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		FN2CLogger::Get().LogError(TEXT("Failed to parse token response JSON"));
		return false;
	}

	// Check for error
	if (JsonObject->HasField(TEXT("error")))
	{
		FString Error = JsonObject->GetStringField(TEXT("error"));
		FString ErrorDescription = JsonObject->GetStringField(TEXT("error_description"));
		FN2CLogger::Get().LogError(FString::Printf(TEXT("OAuth error: %s - %s"), *Error, *ErrorDescription));
		OnError.Broadcast(ErrorDescription.IsEmpty() ? Error : ErrorDescription);
		return false;
	}

	// Extract tokens
	FString AccessToken = JsonObject->GetStringField(TEXT("access_token"));
	FString RefreshToken = JsonObject->GetStringField(TEXT("refresh_token"));
	FString Scope = JsonObject->GetStringField(TEXT("scope"));

	if (AccessToken.IsEmpty())
	{
		FN2CLogger::Get().LogError(TEXT("No access token in response"));
		return false;
	}

	// Calculate expiry time (default 8 hours if not specified)
	int32 ExpiresIn = 28800; // 8 hours in seconds
	if (JsonObject->HasField(TEXT("expires_in")))
	{
		ExpiresIn = static_cast<int32>(JsonObject->GetNumberField(TEXT("expires_in")));
	}

	FDateTime ExpiryTime = FDateTime::UtcNow() + FTimespan::FromSeconds(ExpiresIn);

	// Update cached tokens
	CachedTokens.AccessToken = AccessToken;
	if (!RefreshToken.IsEmpty())
	{
		CachedTokens.RefreshToken = RefreshToken;
	}
	CachedTokens.ExpiresAt = ExpiryTime.ToIso8601();
	CachedTokens.Scope = Scope;

	return true;
}

void UN2COAuthTokenManager::HandleTokenResponse(FHttpResponsePtr Response, bool bSuccess,
	TFunction<void(bool)> OnComplete)
{
	if (!bSuccess || !Response.IsValid())
	{
		FN2CLogger::Get().LogError(TEXT("Token request failed - no response"));
		OnError.Broadcast(TEXT("Network error. Please check your connection."));
		OnComplete(false);
		return;
	}

	int32 ResponseCode = Response->GetResponseCode();
	FString ResponseContent = Response->GetContentAsString();

	if (ResponseCode < 200 || ResponseCode >= 300)
	{
		FN2CLogger::Get().LogError(FString::Printf(
			TEXT("Token request failed with code %d: %s"), ResponseCode, *ResponseContent));

		// Try to parse error from response
		TSharedPtr<FJsonObject> ErrorJson;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);
		if (FJsonSerializer::Deserialize(Reader, ErrorJson) && ErrorJson.IsValid())
		{
			FString ErrorMessage = ErrorJson->GetStringField(TEXT("error_description"));
			if (ErrorMessage.IsEmpty())
			{
				ErrorMessage = ErrorJson->GetStringField(TEXT("error"));
			}
			OnError.Broadcast(ErrorMessage.IsEmpty() ? TEXT("Authentication failed") : ErrorMessage);
		}
		else
		{
			OnError.Broadcast(FString::Printf(TEXT("Authentication failed (HTTP %d)"), ResponseCode));
		}

		OnComplete(false);
		return;
	}

	// Parse successful response
	if (!ParseTokenResponse(ResponseContent))
	{
		OnComplete(false);
		return;
	}

	// Save tokens and schedule refresh
	SaveTokensToStorage();
	ScheduleTokenRefresh();

	// Clear PKCE values after successful exchange
	CurrentVerifier.Empty();
	CurrentState.Empty();

	// Broadcast success
	OnAuthStateChanged.Broadcast(true);

	FN2CLogger::Get().Log(TEXT("OAuth authentication successful"), EN2CLogSeverity::Info);

	OnComplete(true);
}
