// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

class IDetailLayoutBuilder;
class UN2CSettings;

/**
 * @class FN2COAuthSettingsCustomization
 * @brief Custom property editor for OAuth authentication settings
 *
 * Provides a custom UI for the Anthropic OAuth authentication workflow:
 * - Login button that opens browser to authorization URL
 * - Code input field for pasting the authorization code
 * - Submit button to exchange code for tokens
 * - Logout button when connected
 * - Connection status display
 */
class FN2COAuthSettingsCustomization : public IDetailCustomization
{
public:
	/** Creates an instance of this customization */
	static TSharedRef<IDetailCustomization> MakeInstance();

	// IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	/** The settings object being customized */
	TWeakObjectPtr<UN2CSettings> SettingsObject;

	/** Text for code input field */
	FString AuthCodeInput;

	/** Open browser to OAuth authorization URL */
	FReply OnLoginClicked();

	/** Submit the authorization code for token exchange */
	FReply OnSubmitCodeClicked();

	/** Log out and clear tokens */
	FReply OnLogoutClicked();

	/** Get the current OAuth connection status text */
	FText GetStatusText() const;

	/** Get the status text color based on connection state */
	FSlateColor GetStatusColor() const;

	/** Check if we're currently authenticated */
	bool IsAuthenticated() const;

	/** Check if OAuth mode is currently selected */
	bool IsOAuthModeSelected() const;

	/** Refresh the details panel to update UI state */
	void RefreshDetailsPanel();
};
