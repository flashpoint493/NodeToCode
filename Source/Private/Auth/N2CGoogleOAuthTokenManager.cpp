// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#include "Auth/N2CGoogleOAuthTokenManager.h"
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
	TArray<uint8> ComputeSHA256Google(const TArray<uint8>& Data)
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
UN2CGoogleOAuthTokenManager* UN2CGoogleOAuthTokenManager::Instance = nullptr;

UN2CGoogleOAuthTokenManager* UN2CGoogleOAuthTokenManager::Get()
{
	if (!Instance)
	{
		Instance = NewObject<UN2CGoogleOAuthTokenManager>();
		Instance->AddToRoot(); // Prevent garbage collection
		Instance->Initialize();
	}
	return Instance;
}

void UN2CGoogleOAuthTokenManager::Initialize()
{
	// Get or create user secrets
	UserSecrets = NewObject<UN2CUserSecrets>();
	UserSecrets->LoadSecrets();

	// Load existing tokens
	LoadTokensFromStorage();

	// If we have valid tokens, schedule refresh and initialize Code Assist session
	if (IsAuthenticated() && !IsTokenExpired())
	{
		ScheduleTokenRefresh();

		// Initialize Code Assist session (required for API access)
		// This is done asynchronously - requests may need to wait for it
		InitializeCodeAssistSession([](bool bSuccess)
		{
			if (bSuccess)
			{
				FN2CLogger::Get().Log(TEXT("Code Assist session restored from saved tokens"), EN2CLogSeverity::Info);
			}
			else
			{
				FN2CLogger::Get().LogWarning(TEXT("Failed to restore Code Assist session - may need to re-login"));
			}
		});
	}

	FN2CLogger::Get().Log(TEXT("Google OAuth Token Manager initialized"), EN2CLogSeverity::Info);
}

FString UN2CGoogleOAuthTokenManager::GenerateAuthorizationUrl()
{
	// Generate PKCE values
	CurrentVerifier = GenerateVerifier();
	CurrentState = GenerateState();
	FString Challenge = GenerateChallenge(CurrentVerifier);

	// Build authorization URL with access_type=offline to get refresh token
	FString AuthUrl = FString::Printf(
		TEXT("%s?response_type=code&client_id=%s&redirect_uri=%s&scope=%s&code_challenge=%s&code_challenge_method=S256&state=%s&access_type=offline&prompt=consent"),
		*FN2CGoogleOAuthConstants::AuthEndpoint,
		*FN2CGoogleOAuthConstants::ClientId,
		*FGenericPlatformHttp::UrlEncode(FN2CGoogleOAuthConstants::RedirectUri),
		*FGenericPlatformHttp::UrlEncode(FN2CGoogleOAuthConstants::Scopes),
		*Challenge,
		*CurrentState
	);

	FN2CLogger::Get().Log(TEXT("Generated Google OAuth authorization URL"), EN2CLogSeverity::Debug);

	return AuthUrl;
}

void UN2CGoogleOAuthTokenManager::ExchangeCodeForTokens(const FString& Code, const FOnTokenExchangeComplete& OnComplete)
{
	if (Code.IsEmpty())
	{
		FN2CLogger::Get().LogError(TEXT("Google OAuth code is empty"));
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

	// Build form-encoded request body (Google requires form-encoded, not JSON)
	FString RequestBody = FString::Printf(
		TEXT("grant_type=authorization_code&client_id=%s&client_secret=%s&code=%s&redirect_uri=%s&code_verifier=%s"),
		*FGenericPlatformHttp::UrlEncode(FN2CGoogleOAuthConstants::ClientId),
		*FGenericPlatformHttp::UrlEncode(FN2CGoogleOAuthConstants::ClientSecret),
		*FGenericPlatformHttp::UrlEncode(Code),
		*FGenericPlatformHttp::UrlEncode(FN2CGoogleOAuthConstants::RedirectUri),
		*FGenericPlatformHttp::UrlEncode(CurrentVerifier)
	);

	FN2CLogger::Get().Log(TEXT("Exchanging Google authorization code for tokens..."), EN2CLogSeverity::Info);
	FN2CLogger::Get().Log(FString::Printf(TEXT("Token endpoint: %s"), *FN2CGoogleOAuthConstants::TokenEndpoint), EN2CLogSeverity::Debug);

	// Create HTTP request with form-encoded content type
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(FN2CGoogleOAuthConstants::TokenEndpoint);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/x-www-form-urlencoded"));
	Request->SetContentAsString(RequestBody);

	// Weak reference for lambda capture
	TWeakObjectPtr<UN2CGoogleOAuthTokenManager> WeakThis(this);

	Request->OnProcessRequestComplete().BindLambda(
		[WeakThis, OnComplete](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
		{
			if (UN2CGoogleOAuthTokenManager* This = WeakThis.Get())
			{
				This->HandleTokenResponse(Response, bConnectedSuccessfully,
					[OnComplete](bool bSuccess)
					{
						OnComplete.ExecuteIfBound(bSuccess);
					});
			}
		});

	Request->ProcessRequest();
}

void UN2CGoogleOAuthTokenManager::RefreshAccessToken(const FOnTokenRefreshComplete& OnComplete)
{
	if (CachedTokens.RefreshToken.IsEmpty())
	{
		FN2CLogger::Get().LogError(TEXT("No Google refresh token available"));
		OnError.Broadcast(TEXT("No refresh token. Please log in again."));
		OnComplete.ExecuteIfBound(false);
		return;
	}

	// Build form-encoded refresh request body
	FString RequestBody = FString::Printf(
		TEXT("grant_type=refresh_token&client_id=%s&client_secret=%s&refresh_token=%s"),
		*FGenericPlatformHttp::UrlEncode(FN2CGoogleOAuthConstants::ClientId),
		*FGenericPlatformHttp::UrlEncode(FN2CGoogleOAuthConstants::ClientSecret),
		*FGenericPlatformHttp::UrlEncode(CachedTokens.RefreshToken)
	);

	// Create HTTP request with form-encoded content type
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(FN2CGoogleOAuthConstants::TokenEndpoint);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/x-www-form-urlencoded"));
	Request->SetContentAsString(RequestBody);

	// Weak reference for lambda capture
	TWeakObjectPtr<UN2CGoogleOAuthTokenManager> WeakThis(this);

	Request->OnProcessRequestComplete().BindLambda(
		[WeakThis, OnComplete](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
		{
			if (UN2CGoogleOAuthTokenManager* This = WeakThis.Get())
			{
				This->HandleTokenResponse(Response, bConnectedSuccessfully,
					[OnComplete](bool bSuccess)
					{
						OnComplete.ExecuteIfBound(bSuccess);
					});
			}
		});

	Request->ProcessRequest();

	FN2CLogger::Get().Log(TEXT("Refreshing Google access token..."), EN2CLogSeverity::Info);
}

bool UN2CGoogleOAuthTokenManager::RefreshAccessTokenSync()
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
			FN2CLogger::Get().LogError(TEXT("Google token refresh timed out"));
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

FString UN2CGoogleOAuthTokenManager::GetAccessToken()
{
	if (!IsAuthenticated())
	{
		return FString();
	}

	// If token is expired, try to refresh synchronously
	if (IsTokenExpired())
	{
		FN2CLogger::Get().Log(TEXT("Google access token expired, attempting refresh..."), EN2CLogSeverity::Info);
		if (!RefreshAccessTokenSync())
		{
			return FString();
		}
	}

	return CachedTokens.AccessToken;
}

FString UN2CGoogleOAuthTokenManager::GetProjectId()
{
	// Ensure session is initialized before returning project ID
	if (!bSessionInitialized && IsAuthenticated())
	{
		EnsureSessionInitialized();
	}
	return CachedProjectId;
}

bool UN2CGoogleOAuthTokenManager::EnsureSessionInitialized()
{
	// Already initialized
	if (bSessionInitialized)
	{
		return true;
	}

	// Not authenticated, can't initialize
	if (!IsAuthenticated())
	{
		return false;
	}

	FN2CLogger::Get().Log(TEXT("Synchronously initializing Code Assist session..."), EN2CLogSeverity::Info);

	// Use a blocking approach for synchronous initialization
	volatile bool bCompleted = false;
	volatile bool bSuccess = false;

	InitializeCodeAssistSession([&bCompleted, &bSuccess](bool bResult)
	{
		bSuccess = bResult;
		bCompleted = true;
	});

	// Wait for completion (with timeout)
	const double TimeoutSeconds = 30.0;
	const double StartTime = FPlatformTime::Seconds();

	while (!bCompleted)
	{
		if (FPlatformTime::Seconds() - StartTime > TimeoutSeconds)
		{
			FN2CLogger::Get().LogError(TEXT("Code Assist session initialization timed out"));
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

bool UN2CGoogleOAuthTokenManager::IsAuthenticated() const
{
	return CachedTokens.HasTokens();
}

bool UN2CGoogleOAuthTokenManager::IsTokenExpired() const
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

FString UN2CGoogleOAuthTokenManager::GetExpirationTimeString() const
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

void UN2CGoogleOAuthTokenManager::Logout()
{
	CancelTokenRefresh();
	CachedTokens.Clear();
	CurrentVerifier.Empty();
	CurrentState.Empty();
	CachedProjectId.Empty();
	bSessionInitialized = false;

	if (UserSecrets)
	{
		UserSecrets->ClearGoogleOAuthTokens();
	}

	OnAuthStateChanged.Broadcast(false);

	FN2CLogger::Get().Log(TEXT("Google OAuth logout complete"), EN2CLogSeverity::Info);
}

// ============================================
// PKCE Helper Methods
// ============================================

FString UN2CGoogleOAuthTokenManager::GenerateVerifier()
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

FString UN2CGoogleOAuthTokenManager::GenerateChallenge(const FString& Verifier)
{
	// Convert verifier to UTF-8 bytes
	FTCHARToUTF8 VerifierUtf8(*Verifier);
	TArray<uint8> VerifierBytes;
	VerifierBytes.SetNumUninitialized(VerifierUtf8.Length());
	FMemory::Memcpy(VerifierBytes.GetData(), VerifierUtf8.Get(), VerifierUtf8.Length());

	// SHA-256 hash of verifier
	TArray<uint8> HashBytes = ComputeSHA256Google(VerifierBytes);

	return Base64UrlEncode(HashBytes);
}

FString UN2CGoogleOAuthTokenManager::GenerateState()
{
	return FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
}

FString UN2CGoogleOAuthTokenManager::Base64UrlEncode(const TArray<uint8>& Bytes)
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

void UN2CGoogleOAuthTokenManager::LoadTokensFromStorage()
{
	if (UserSecrets)
	{
		UserSecrets->GetGoogleOAuthTokens(CachedTokens);
	}
}

void UN2CGoogleOAuthTokenManager::SaveTokensToStorage()
{
	if (UserSecrets)
	{
		UserSecrets->SetGoogleOAuthTokens(
			CachedTokens.AccessToken,
			CachedTokens.RefreshToken,
			CachedTokens.ExpiresAt,
			CachedTokens.Scope
		);
	}
}

void UN2CGoogleOAuthTokenManager::ScheduleTokenRefresh()
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
							FN2CLogger::Get().LogError(TEXT("Automatic Google token refresh failed"));
						}
					}));
				}),
				TimeUntilRefresh.GetTotalSeconds(),
				false
			);

			FN2CLogger::Get().Log(
				FString::Printf(TEXT("Google token refresh scheduled in %.0f minutes"), TimeUntilRefresh.GetTotalMinutes()),
				EN2CLogSeverity::Debug
			);
		}
	}
}

void UN2CGoogleOAuthTokenManager::CancelTokenRefresh()
{
	if (GEngine && GEngine->GetWorld())
	{
		GEngine->GetWorld()->GetTimerManager().ClearTimer(RefreshTimerHandle);
	}
}

bool UN2CGoogleOAuthTokenManager::ParseTokenResponse(const FString& ResponseJson)
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseJson);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		FN2CLogger::Get().LogError(TEXT("Failed to parse Google token response JSON"));
		return false;
	}

	// Check for error
	if (JsonObject->HasField(TEXT("error")))
	{
		FString Error = JsonObject->GetStringField(TEXT("error"));
		FString ErrorDescription = JsonObject->GetStringField(TEXT("error_description"));
		FN2CLogger::Get().LogError(FString::Printf(TEXT("Google OAuth error: %s - %s"), *Error, *ErrorDescription));
		OnError.Broadcast(ErrorDescription.IsEmpty() ? Error : ErrorDescription);
		return false;
	}

	// Extract tokens
	FString AccessToken = JsonObject->GetStringField(TEXT("access_token"));
	FString RefreshToken = JsonObject->GetStringField(TEXT("refresh_token"));
	FString Scope = JsonObject->GetStringField(TEXT("scope"));

	if (AccessToken.IsEmpty())
	{
		FN2CLogger::Get().LogError(TEXT("No access token in Google response"));
		return false;
	}

	// Calculate expiry time (Google typically provides expires_in in seconds)
	int32 ExpiresIn = 3600; // Default 1 hour
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

void UN2CGoogleOAuthTokenManager::HandleTokenResponse(FHttpResponsePtr Response, bool bSuccess,
	TFunction<void(bool)> OnComplete)
{
	if (!bSuccess || !Response.IsValid())
	{
		FN2CLogger::Get().LogError(TEXT("Google token request failed - no response"));
		OnError.Broadcast(TEXT("Network error. Please check your connection."));
		OnComplete(false);
		return;
	}

	int32 ResponseCode = Response->GetResponseCode();
	FString ResponseContent = Response->GetContentAsString();

	if (ResponseCode < 200 || ResponseCode >= 300)
	{
		FN2CLogger::Get().LogError(FString::Printf(
			TEXT("Google token request failed with code %d: %s"), ResponseCode, *ResponseContent));

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

	FN2CLogger::Get().Log(TEXT("Google OAuth authentication successful"), EN2CLogSeverity::Info);

	// Initialize Code Assist session after successful token exchange
	InitializeCodeAssistSession([OnComplete](bool bSessionSuccess)
	{
		if (!bSessionSuccess)
		{
			FN2CLogger::Get().LogWarning(TEXT("Code Assist session initialization failed, but authentication succeeded"));
		}
		OnComplete(true);  // Authentication still succeeded even if session init failed
	});
}

// ============================================
// Code Assist Session Management
// ============================================

void UN2CGoogleOAuthTokenManager::InitializeCodeAssistSession(TFunction<void(bool)> OnComplete)
{
	if (!IsAuthenticated())
	{
		FN2CLogger::Get().LogError(TEXT("Cannot initialize Code Assist session - not authenticated"));
		if (OnComplete) OnComplete(false);
		return;
	}

	FN2CLogger::Get().Log(TEXT("Initializing Code Assist session..."), EN2CLogSeverity::Info);

	// Build loadCodeAssist request payload
	TSharedPtr<FJsonObject> RequestObject = MakeShareable(new FJsonObject());

	// Metadata object
	TSharedPtr<FJsonObject> MetadataObject = MakeShareable(new FJsonObject());
	MetadataObject->SetStringField(TEXT("ideType"), TEXT("IDE_UNSPECIFIED"));
	MetadataObject->SetStringField(TEXT("platform"), TEXT("WINDOWS_AMD64"));
	MetadataObject->SetStringField(TEXT("pluginType"), TEXT("GEMINI"));

	RequestObject->SetObjectField(TEXT("metadata"), MetadataObject);

	// Serialize request
	FString RequestBody;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
	FJsonSerializer::Serialize(RequestObject.ToSharedRef(), Writer);

	// Create HTTP request
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(TEXT("https://cloudcode-pa.googleapis.com/v1internal:loadCodeAssist"));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *CachedTokens.AccessToken));
	Request->SetContentAsString(RequestBody);

	// Weak reference for lambda capture
	TWeakObjectPtr<UN2CGoogleOAuthTokenManager> WeakThis(this);

	Request->OnProcessRequestComplete().BindLambda(
		[WeakThis, OnComplete](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
		{
			if (!WeakThis.IsValid())
			{
				if (OnComplete) OnComplete(false);
				return;
			}

			UN2CGoogleOAuthTokenManager* This = WeakThis.Get();

			if (!bConnectedSuccessfully || !Response.IsValid())
			{
				FN2CLogger::Get().LogError(TEXT("loadCodeAssist request failed - no response"));
				if (OnComplete) OnComplete(false);
				return;
			}

			int32 ResponseCode = Response->GetResponseCode();
			FString ResponseContent = Response->GetContentAsString();

			FN2CLogger::Get().Log(FString::Printf(TEXT("loadCodeAssist response (%d): %s"),
				ResponseCode, *ResponseContent), EN2CLogSeverity::Debug);

			if (ResponseCode >= 200 && ResponseCode < 300)
			{
				if (This->ParseLoadCodeAssistResponse(ResponseContent))
				{
					This->bSessionInitialized = true;
					FN2CLogger::Get().Log(TEXT("Code Assist session initialized successfully"), EN2CLogSeverity::Info);
					if (OnComplete) OnComplete(true);
				}
				else
				{
					// Even if we can't parse, the session may still work
					This->bSessionInitialized = true;
					FN2CLogger::Get().LogWarning(TEXT("Could not parse loadCodeAssist response, but session may still work"));
					if (OnComplete) OnComplete(true);
				}
			}
			else
			{
				FN2CLogger::Get().LogError(FString::Printf(TEXT("loadCodeAssist failed with code %d"), ResponseCode));
				if (OnComplete) OnComplete(false);
			}
		});

	Request->ProcessRequest();
}

bool UN2CGoogleOAuthTokenManager::ParseLoadCodeAssistResponse(const FString& ResponseJson)
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseJson);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		return false;
	}

	// Extract project ID if available
	if (JsonObject->HasField(TEXT("cloudaicompanionProject")))
	{
		CachedProjectId = JsonObject->GetStringField(TEXT("cloudaicompanionProject"));
		FN2CLogger::Get().Log(FString::Printf(TEXT("Code Assist project ID: %s"), *CachedProjectId), EN2CLogSeverity::Debug);
	}

	return true;
}
