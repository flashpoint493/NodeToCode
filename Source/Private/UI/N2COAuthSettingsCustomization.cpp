// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#include "UI/N2COAuthSettingsCustomization.h"

#include "Auth/N2COAuthTokenManager.h"
#include "Auth/N2COAuthTypes.h"
#include "Core/N2CSettings.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "PropertyEditorModule.h"
#include "Utils/N2CLogger.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "NodeToCode"

TSharedRef<IDetailCustomization> FN2COAuthSettingsCustomization::MakeInstance()
{
	return MakeShareable(new FN2COAuthSettingsCustomization());
}

void FN2COAuthSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// Get the settings object being edited
	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
	DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);

	if (ObjectsBeingCustomized.Num() != 1)
	{
		return;
	}

	SettingsObject = Cast<UN2CSettings>(ObjectsBeingCustomized[0].Get());
	if (!SettingsObject.IsValid())
	{
		return;
	}

	// NOTE: DetailCustomization on DeveloperSettings causes category duplication issues.
	// The OAuth Login/Logout functionality is now handled via console commands:
	// - N2C.OAuth.Login - Opens browser for OAuth login
	// - N2C.OAuth.Submit <code#state> - Submits the authorization code
	// - N2C.OAuth.Logout - Clears OAuth tokens
	// The OAuthConnectionStatus property in settings shows the current connection state.
}

FReply FN2COAuthSettingsCustomization::OnLoginClicked()
{
	UN2COAuthTokenManager* TokenManager = UN2COAuthTokenManager::Get();
	if (TokenManager)
	{
		FString AuthUrl = TokenManager->GenerateAuthorizationUrl();
		FPlatformProcess::LaunchURL(*AuthUrl, nullptr, nullptr);

		FN2CLogger::Get().Log(TEXT("Opening browser for OAuth authorization"), EN2CLogSeverity::Info);
	}

	return FReply::Handled();
}

FReply FN2COAuthSettingsCustomization::OnSubmitCodeClicked()
{
	if (AuthCodeInput.IsEmpty())
	{
		FN2CLogger::Get().LogWarning(TEXT("No authorization code entered"));
		return FReply::Handled();
	}

	UN2COAuthTokenManager* TokenManager = UN2COAuthTokenManager::Get();
	if (TokenManager)
	{
		// Store weak reference to this for the callback
		TWeakPtr<FN2COAuthSettingsCustomization> WeakSelf =
			StaticCastSharedRef<FN2COAuthSettingsCustomization>(this->AsShared());

		TokenManager->ExchangeCodeForTokens(AuthCodeInput,
			FOnTokenExchangeComplete::CreateLambda([WeakSelf](bool bSuccess)
			{
				if (bSuccess)
				{
					FN2CLogger::Get().Log(TEXT("OAuth token exchange successful"), EN2CLogSeverity::Info);

					// Refresh settings status
					if (UN2CSettings* Settings = GetMutableDefault<UN2CSettings>())
					{
						Settings->RefreshOAuthStatus();
					}
				}
				else
				{
					FN2CLogger::Get().LogError(TEXT("OAuth token exchange failed"));
				}
			}));

		// Clear the input after submission
		AuthCodeInput.Empty();
	}

	return FReply::Handled();
}

FReply FN2COAuthSettingsCustomization::OnLogoutClicked()
{
	UN2COAuthTokenManager* TokenManager = UN2COAuthTokenManager::Get();
	if (TokenManager)
	{
		TokenManager->Logout();

		// Refresh settings status
		if (UN2CSettings* Settings = GetMutableDefault<UN2CSettings>())
		{
			Settings->RefreshOAuthStatus();
		}

		FN2CLogger::Get().Log(TEXT("OAuth logout complete"), EN2CLogSeverity::Info);
	}

	return FReply::Handled();
}

FText FN2COAuthSettingsCustomization::GetStatusText() const
{
	UN2COAuthTokenManager* TokenManager = UN2COAuthTokenManager::Get();
	if (TokenManager && TokenManager->IsAuthenticated())
	{
		if (TokenManager->IsTokenExpired())
		{
			return LOCTEXT("StatusExpired", "Status: Token expired (will refresh on next request)");
		}
		return FText::Format(
			LOCTEXT("StatusConnected", "Status: Connected (expires: {0})"),
			FText::FromString(TokenManager->GetExpirationTimeString())
		);
	}

	return LOCTEXT("StatusNotConnected", "Status: Not connected");
}

FSlateColor FN2COAuthSettingsCustomization::GetStatusColor() const
{
	UN2COAuthTokenManager* TokenManager = UN2COAuthTokenManager::Get();
	if (TokenManager && TokenManager->IsAuthenticated())
	{
		if (TokenManager->IsTokenExpired())
		{
			return FSlateColor(FLinearColor(1.0f, 0.7f, 0.0f)); // Orange for expired
		}
		return FSlateColor(FLinearColor(0.0f, 0.8f, 0.0f)); // Green for connected
	}

	return FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)); // Gray for not connected
}

bool FN2COAuthSettingsCustomization::IsAuthenticated() const
{
	UN2COAuthTokenManager* TokenManager = UN2COAuthTokenManager::Get();
	return TokenManager && TokenManager->IsAuthenticated();
}

bool FN2COAuthSettingsCustomization::IsOAuthModeSelected() const
{
	if (SettingsObject.IsValid())
	{
		return SettingsObject->IsUsingAnthropicOAuth();
	}
	return false;
}

void FN2COAuthSettingsCustomization::RefreshDetailsPanel()
{
	// Force a refresh of the details panel
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.NotifyCustomizationModuleChanged();
}

#undef LOCTEXT_NAMESPACE
