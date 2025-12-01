// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#include "Core/Widgets/SN2CBatchProgressModal.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Styling/AppStyle.h"
#include "Models/N2CLogging.h"

#define LOCTEXT_NAMESPACE "SN2CBatchProgressModal"

// NodeToCode color scheme
namespace N2CProgressColors
{
	const FLinearColor BgPanel = FLinearColor::FromSRGBColor(FColor(37, 37, 38));
	const FLinearColor BgPanelDarker = FLinearColor::FromSRGBColor(FColor(26, 26, 26));
	const FLinearColor BgInput = FLinearColor::FromSRGBColor(FColor(45, 45, 45));
	const FLinearColor BorderColor = FLinearColor::FromSRGBColor(FColor(60, 60, 60));
	const FLinearColor BorderSubtle = FLinearColor::FromSRGBColor(FColor(42, 42, 42));
	const FLinearColor TextPrimary = FLinearColor::FromSRGBColor(FColor(204, 204, 204));
	const FLinearColor TextSecondary = FLinearColor::FromSRGBColor(FColor(157, 157, 157));
	const FLinearColor TextMuted = FLinearColor::FromSRGBColor(FColor(107, 107, 107));
	const FLinearColor AccentOrange = FLinearColor::FromSRGBColor(FColor(212, 160, 74));
	const FLinearColor AccentGreen = FLinearColor::FromSRGBColor(FColor(78, 201, 176));
	const FLinearColor AccentRed = FLinearColor::FromSRGBColor(FColor(241, 76, 76));
}

void SN2CBatchProgressModal::Construct(const FArguments& InArgs)
{
	OnCancelRequestedDelegate = InArgs._OnCancelRequested;
	CurrentItemIndex = 0;
	TotalItemCount = 0;
	bIsComplete = false;

	// Calculate minimum width to fit completion text "Complete - X succeeded, Y failed"
	// Assuming 450px is sufficient for the longest expected text
	const float MinModalWidth = 450.0f;
	const float ModalWidth = FMath::Max(InArgs._ModalWidth, MinModalWidth);
	// Max height for the entire modal (header + body + footer)
	const float MaxModalHeight = 400.0f;
	// Max height for the progress list area
	const float MaxListHeight = 200.0f;

	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(ModalWidth)
		.MaxDesiredHeight(MaxModalHeight)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.BorderBackgroundColor(N2CProgressColors::BgPanel)
			.Padding(0)
			[
				SNew(SVerticalBox)

				// Header
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
					.BorderBackgroundColor(N2CProgressColors::BgPanel)
					.Padding(FMargin(16.0f, 14.0f))
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ModalHeader", "Batch Translation in Progress"))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
						.ColorAndOpacity(N2CProgressColors::TextPrimary)
					]
				]

				// Body
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(16.0f)
				[
					SNew(SVerticalBox)

					// Current item text
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 0.0f, 0.0f, 12.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("ProcessingLabel", "Processing: "))
							.ColorAndOpacity(N2CProgressColors::TextSecondary)
						]
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						[
							SAssignNew(CurrentItemText, STextBlock)
							.Text(LOCTEXT("Waiting", "Waiting..."))
							.ColorAndOpacity(N2CProgressColors::AccentOrange)
							.Font(FCoreStyle::GetDefaultFontStyle("Mono", 12))
						]
					]

					// Progress bar
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 0.0f, 0.0f, 8.0f)
					[
						SNew(SBox)
						.HeightOverride(6.0f)
						[
							SNew(SProgressBar)
							.Percent(this, &SN2CBatchProgressModal::GetProgressBarPercent)
							.FillColorAndOpacity(N2CProgressColors::AccentOrange)
							.BackgroundImage(FAppStyle::GetBrush("ProgressBar.Background"))
						]
					]

					// Progress count
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 0.0f, 0.0f, 16.0f)
					[
						SNew(SBox)
						.HAlign(HAlign_Right)
						[
							SAssignNew(ProgressCountText, STextBlock)
							.Text(LOCTEXT("InitialCount", "0 / 0"))
							.ColorAndOpacity(N2CProgressColors::TextMuted)
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
						]
					]

					// Progress list with max height and scrollbar
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SBox)
						.MaxDesiredHeight(MaxListHeight)
						[
							SNew(SBorder)
							.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
							.BorderBackgroundColor(N2CProgressColors::BgPanelDarker)
							.Padding(0)
							[
								SNew(SScrollBox)
								+ SScrollBox::Slot()
								[
									SAssignNew(ProgressListView, SListView<TSharedPtr<FN2CBatchProgressItem>>)
									.ItemHeight(24.0f)
									.ListItemsSource(&ProgressItems)
									.OnGenerateRow(this, &SN2CBatchProgressModal::GenerateProgressRow)
									.SelectionMode(ESelectionMode::None)
								]
							]
						]
					]
				]

				// Footer with cancel button
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
					.BorderBackgroundColor(N2CProgressColors::BgPanel)
					.Padding(FMargin(16.0f, 12.0f))
					[
						SNew(SBox)
						.HAlign(HAlign_Right)
						[
							SAssignNew(CancelButton, SButton)
							.ButtonStyle(FAppStyle::Get(), "Button")
							.ContentPadding(FMargin(16.0f, 6.0f))
							.OnClicked(this, &SN2CBatchProgressModal::HandleCancelClicked)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("CancelButton", "Cancel"))
							]
						]
					]
				]
			]
		]
	];
}

void SN2CBatchProgressModal::Initialize(const TArray<FString>& GraphNames)
{
	Reset();

	TotalItemCount = GraphNames.Num();

	// Create progress items for all graphs
	for (const FString& Name : GraphNames)
	{
		ProgressItems.Add(MakeShared<FN2CBatchProgressItem>(Name, EN2CBatchProgressItemStatus::Pending));
	}

	// Update count display
	if (ProgressCountText.IsValid())
	{
		ProgressCountText->SetText(FText::Format(
			LOCTEXT("CountFormat", "{0} / {1}"),
			FText::AsNumber(0),
			FText::AsNumber(TotalItemCount)
		));
	}

	// Refresh list view
	if (ProgressListView.IsValid())
	{
		ProgressListView->RequestListRefresh();
	}
}

void SN2CBatchProgressModal::SetProgress(int32 CurrentIndex, int32 TotalCount, const FString& InCurrentGraphName)
{
	CurrentItemIndex = CurrentIndex;
	TotalItemCount = TotalCount;
	CurrentGraphName = InCurrentGraphName;

	// Update current item text
	if (CurrentItemText.IsValid())
	{
		CurrentItemText->SetText(FText::FromString(CurrentGraphName));
	}

	// Update count display
	if (ProgressCountText.IsValid())
	{
		ProgressCountText->SetText(FText::Format(
			LOCTEXT("CountFormat", "{0} / {1}"),
			FText::AsNumber(CurrentIndex + 1),
			FText::AsNumber(TotalCount)
		));
	}

	// Update item status in the list
	for (int32 i = 0; i < ProgressItems.Num(); ++i)
	{
		if (ProgressItems[i]->GraphName == CurrentGraphName)
		{
			ProgressItems[i]->Status = EN2CBatchProgressItemStatus::InProgress;
		}
	}

	// Refresh list view
	if (ProgressListView.IsValid())
	{
		ProgressListView->RequestListRefresh();
	}
}

void SN2CBatchProgressModal::MarkItemComplete(const FString& GraphName, bool bSuccess)
{
	bool bFound = false;
	for (TSharedPtr<FN2CBatchProgressItem>& Item : ProgressItems)
	{
		if (Item->GraphName == GraphName)
		{
			Item->Status = bSuccess ? EN2CBatchProgressItemStatus::Completed : EN2CBatchProgressItemStatus::Failed;
			bFound = true;
			UE_LOG(LogNodeToCode, Log, TEXT("[SN2CBatchProgressModal] MarkItemComplete: Found '%s', set to %s"),
				*GraphName, bSuccess ? TEXT("Completed") : TEXT("Failed"));
			break;
		}
	}

	if (!bFound)
	{
		UE_LOG(LogNodeToCode, Warning, TEXT("[SN2CBatchProgressModal] MarkItemComplete: Could NOT find '%s' in %d items"),
			*GraphName, ProgressItems.Num());
		// Log all item names for debugging
		for (const TSharedPtr<FN2CBatchProgressItem>& Item : ProgressItems)
		{
			UE_LOG(LogNodeToCode, Log, TEXT("  - Item: '%s'"), *Item->GraphName);
		}
	}

	// Refresh list view
	if (ProgressListView.IsValid())
	{
		ProgressListView->RequestListRefresh();
	}
}

void SN2CBatchProgressModal::SetResult(const FN2CBatchTranslationResult& Result)
{
	bIsComplete = true;

	// Update current item text to show completion
	if (CurrentItemText.IsValid())
	{
		CurrentItemText->SetText(FText::Format(
			LOCTEXT("Complete", "Complete - {0} succeeded, {1} failed"),
			FText::AsNumber(Result.SuccessCount),
			FText::AsNumber(Result.FailureCount)
		));
	}

	// Update count to show final numbers
	if (ProgressCountText.IsValid())
	{
		ProgressCountText->SetText(FText::Format(
			LOCTEXT("CountFormat", "{0} / {1}"),
			FText::AsNumber(Result.TotalCount),
			FText::AsNumber(Result.TotalCount)
		));
	}

	// Update cancel button to show "Close"
	if (CancelButton.IsValid())
	{
		CancelButton->SetContent(
			SNew(STextBlock)
			.Text(LOCTEXT("CloseButton", "Close"))
		);
	}
}

void SN2CBatchProgressModal::Reset()
{
	ProgressItems.Empty();
	CurrentItemIndex = 0;
	TotalItemCount = 0;
	CurrentGraphName = FString();
	bIsComplete = false;

	if (CurrentItemText.IsValid())
	{
		CurrentItemText->SetText(LOCTEXT("Waiting", "Waiting..."));
	}

	if (ProgressCountText.IsValid())
	{
		ProgressCountText->SetText(LOCTEXT("InitialCount", "0 / 0"));
	}

	if (CancelButton.IsValid())
	{
		CancelButton->SetContent(
			SNew(STextBlock)
			.Text(LOCTEXT("CancelButton", "Cancel"))
		);
	}

	if (ProgressListView.IsValid())
	{
		ProgressListView->RequestListRefresh();
	}
}

float SN2CBatchProgressModal::GetProgressPercent() const
{
	if (TotalItemCount <= 0)
	{
		return 0.0f;
	}
	return static_cast<float>(CurrentItemIndex + 1) / static_cast<float>(TotalItemCount);
}

FReply SN2CBatchProgressModal::HandleCancelClicked()
{
	OnCancelRequestedDelegate.ExecuteIfBound();
	return FReply::Handled();
}

TSharedRef<ITableRow> SN2CBatchProgressModal::GenerateProgressRow(TSharedPtr<FN2CBatchProgressItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	// Capture Item by value (shared pointer) so status updates are reflected
	TSharedPtr<FN2CBatchProgressItem> ItemPtr = Item;

	return SNew(STableRow<TSharedPtr<FN2CBatchProgressItem>>, OwnerTable)
		.Padding(FMargin(8.0f, 4.0f))
		[
			SNew(SHorizontalBox)
			// Status icon
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 8.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(16.0f)
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					// Use attribute binding to dynamically query current status
					.Text_Lambda([this, ItemPtr]() { return GetStatusIcon(ItemPtr->Status); })
					.ColorAndOpacity_Lambda([this, ItemPtr]() { return GetStatusColor(ItemPtr->Status); })
				]
			]
			// Graph name
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->GraphName))
				.Font(FCoreStyle::GetDefaultFontStyle("Mono", 11))
				// Use attribute binding to dynamically query current status
				.ColorAndOpacity_Lambda([this, ItemPtr]() { return GetItemTextColor(ItemPtr->Status); })
			]
		];
}

TOptional<float> SN2CBatchProgressModal::GetProgressBarPercent() const
{
	if (TotalItemCount <= 0)
	{
		return 0.0f;
	}
	return static_cast<float>(CurrentItemIndex + 1) / static_cast<float>(TotalItemCount);
}

FText SN2CBatchProgressModal::GetStatusIcon(EN2CBatchProgressItemStatus Status) const
{
	switch (Status)
	{
	case EN2CBatchProgressItemStatus::Completed:
		return FText::FromString(TEXT("\u2713")); // ✓
	case EN2CBatchProgressItemStatus::InProgress:
		return FText::FromString(TEXT("\u2192")); // →
	case EN2CBatchProgressItemStatus::Failed:
		return FText::FromString(TEXT("\u2717")); // ✗
	case EN2CBatchProgressItemStatus::Pending:
	default:
		return FText::FromString(TEXT("\u25CB")); // ○
	}
}

FSlateColor SN2CBatchProgressModal::GetStatusColor(EN2CBatchProgressItemStatus Status) const
{
	switch (Status)
	{
	case EN2CBatchProgressItemStatus::Completed:
		return N2CProgressColors::AccentGreen;
	case EN2CBatchProgressItemStatus::InProgress:
		return N2CProgressColors::AccentOrange;
	case EN2CBatchProgressItemStatus::Failed:
		return N2CProgressColors::AccentRed;
	case EN2CBatchProgressItemStatus::Pending:
	default:
		return N2CProgressColors::TextMuted;
	}
}

FSlateColor SN2CBatchProgressModal::GetItemTextColor(EN2CBatchProgressItemStatus Status) const
{
	switch (Status)
	{
	case EN2CBatchProgressItemStatus::InProgress:
		return N2CProgressColors::TextPrimary;
	case EN2CBatchProgressItemStatus::Completed:
	case EN2CBatchProgressItemStatus::Failed:
	case EN2CBatchProgressItemStatus::Pending:
	default:
		return N2CProgressColors::TextSecondary;
	}
}

#undef LOCTEXT_NAMESPACE
