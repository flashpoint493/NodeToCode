// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#include "TagManager/Widgets/SN2CGraphListRow.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Styling/AppStyle.h"
#include "Models/N2CLogging.h"

#define LOCTEXT_NAMESPACE "SN2CGraphListRow"

void SN2CGraphListRow::Construct(const FArguments& InArgs)
{
	Item = InArgs._Item;
	OnCheckboxChangedDelegate = InArgs._OnCheckboxChanged;
	OnTranslateClickedDelegate = InArgs._OnTranslateClicked;
	OnJsonExportClickedDelegate = InArgs._OnJsonExportClicked;
	OnViewTranslationClickedDelegate = InArgs._OnViewTranslationClicked;
	OnDoubleClickedDelegate = InArgs._OnDoubleClicked;

	if (!Item.IsValid())
	{
		UE_LOG(LogNodeToCode, Warning, TEXT("[SN2CGraphListRow] Construct called with invalid Item"));
		return;
	}

	ChildSlot
	[
		// Use SOverlay to layer buttons on top of content
		SNew(SOverlay)

		// Layer 0: Background and main content
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			// Use a solid brush that will show the background color
			.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
			.BorderBackgroundColor(this, &SN2CGraphListRow::GetBackgroundColor)
			.Padding(FMargin(0.0f, 2.0f))
			[
				SNew(SHorizontalBox)

				// Checkbox column - fixed width to match header (30px)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(4.0f, 0.0f)
				[
					SNew(SBox)
					.WidthOverride(22.0f)
					[
						SNew(SCheckBox)
						.IsChecked(this, &SN2CGraphListRow::GetCheckboxState)
						.OnCheckStateChanged(this, &SN2CGraphListRow::OnCheckboxStateChanged)
					]
				]

				// Graph name column - fill width
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				.Padding(4.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Item->TagInfo.GraphName))
					.Font(FAppStyle::GetFontStyle("SmallFont"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]

				// Blueprint name column - fill width
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				.Padding(4.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Item->GetBlueprintDisplayName()))
					.Font(FAppStyle::GetFontStyle("SmallFont"))
					.ColorAndOpacity(FLinearColor(0.31f, 0.76f, 1.0f, 1.0f)) // Blueprint blue
					.ToolTipText(FText::FromString(Item->TagInfo.BlueprintPath))
				]
			]
		]

		// Layer 1: Action buttons overlay (right-aligned, visible on hover)
		+ SOverlay::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.Padding(FMargin(0.0f, 0.0f, 8.0f, 0.0f))
		[
			SNew(SBorder)
			// Give buttons a background so they're visible over text
			.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
			.BorderBackgroundColor(this, &SN2CGraphListRow::GetButtonBackgroundColor)
			.Padding(FMargin(0.0f, 0.0f))
			.Visibility(this, &SN2CGraphListRow::GetActionButtonsVisibility)
			[
				SNew(SHorizontalBox)

				// Translate button
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f, 0.0f)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.ToolTipText(LOCTEXT("TranslateTooltip", "Translate this graph"))
					.OnClicked(this, &SN2CGraphListRow::HandleTranslateClicked)
					.ContentPadding(FMargin(4.0f, 2.0f))
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("\U0001F504"))) // Right arrow
						.Font(FAppStyle::GetFontStyle("NormalFont"))
					]
				]

				// JSON Export button
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f, 0.0f)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.ToolTipText(LOCTEXT("JsonExportTooltip", "Export as JSON"))
					.OnClicked(this, &SN2CGraphListRow::HandleJsonExportClicked)
					.ContentPadding(FMargin(4.0f, 2.0f))
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("{  }")))
						.Font(FAppStyle::GetFontStyle("NormalFont"))
					]
				]

				// View Translation button
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f, 0.0f)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.IsEnabled(this, &SN2CGraphListRow::IsViewButtonEnabled)
					.ToolTipText(this, &SN2CGraphListRow::GetViewButtonTooltip)
					.OnClicked(this, &SN2CGraphListRow::HandleViewTranslationClicked)
					.ContentPadding(FMargin(4.0f, 2.0f))
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush("Icons.Visible"))
						.DesiredSizeOverride(FVector2D(14.0f, 14.0f))
						.ColorAndOpacity(FSlateColor::UseForeground())
					]
				]
			]
		]
	];
}

void SN2CGraphListRow::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	SCompoundWidget::OnMouseEnter(MyGeometry, MouseEvent);

	if (!bIsHovered)
	{
		bIsHovered = true;
		Invalidate(EInvalidateWidgetReason::Paint);
	}
}

void SN2CGraphListRow::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	SCompoundWidget::OnMouseLeave(MouseEvent);

	if (bIsHovered)
	{
		bIsHovered = false;
		Invalidate(EInvalidateWidgetReason::Paint);
	}
}

FReply SN2CGraphListRow::OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		OnDoubleClickedDelegate.ExecuteIfBound(Item);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SN2CGraphListRow::UpdateVisuals()
{
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SN2CGraphListRow::OnCheckboxStateChanged(ECheckBoxState NewState)
{
	if (Item.IsValid())
	{
		Item->bIsSelected = (NewState == ECheckBoxState::Checked);
		OnCheckboxChangedDelegate.ExecuteIfBound(Item);
		Invalidate(EInvalidateWidgetReason::Paint);
	}
}

FReply SN2CGraphListRow::HandleTranslateClicked()
{
	if (Item.IsValid())
	{
		UE_LOG(LogNodeToCode, Log, TEXT("[SN2CGraphListRow] Translate clicked for graph: %s"), *Item->TagInfo.GraphName);
		OnTranslateClickedDelegate.ExecuteIfBound(Item);
	}
	return FReply::Handled();
}

FReply SN2CGraphListRow::HandleJsonExportClicked()
{
	if (Item.IsValid())
	{
		UE_LOG(LogNodeToCode, Log, TEXT("[SN2CGraphListRow] JSON Export clicked for graph: %s"), *Item->TagInfo.GraphName);
		OnJsonExportClickedDelegate.ExecuteIfBound(Item);
	}
	return FReply::Handled();
}

FReply SN2CGraphListRow::HandleViewTranslationClicked()
{
	if (Item.IsValid())
	{
		UE_LOG(LogNodeToCode, Log, TEXT("[SN2CGraphListRow] View Translation clicked for graph: %s"), *Item->TagInfo.GraphName);
		OnViewTranslationClickedDelegate.ExecuteIfBound(Item);
	}
	return FReply::Handled();
}

FSlateColor SN2CGraphListRow::GetBackgroundColor() const
{
	if (!Item.IsValid())
	{
		return FSlateColor(FLinearColor::Transparent);
	}

	// Selected + Hovered: Brighter gold
	if (Item->bIsSelected && bIsHovered)
	{
		return FSlateColor(FLinearColor(0.83f, 0.63f, 0.29f, 0.3f));
	}
	// Selected: Normal gold
	if (Item->bIsSelected)
	{
		return FSlateColor(FLinearColor(0.83f, 0.63f, 0.29f, 0.2f));
	}
	// Hovered: Subtle gray
	if (bIsHovered)
	{
		return FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f, 0.15f));
	}
	// Normal: Transparent
	return FSlateColor(FLinearColor::Transparent);
}

FSlateColor SN2CGraphListRow::GetButtonBackgroundColor() const
{
	// Use a semi-transparent dark background so buttons are visible over text
	// Match the row background when selected, otherwise use dark gray
	if (Item.IsValid() && Item->bIsSelected)
	{
		return FSlateColor(FLinearColor(0.15f, 0.15f, 0.15f, 0.95f));
	}
	return FSlateColor(FLinearColor(0.1f, 0.1f, 0.1f, 0.9f));
}

EVisibility SN2CGraphListRow::GetActionButtonsVisibility() const
{
	return bIsHovered ? EVisibility::Visible : EVisibility::Collapsed;
}

ECheckBoxState SN2CGraphListRow::GetCheckboxState() const
{
	if (Item.IsValid() && Item->bIsSelected)
	{
		return ECheckBoxState::Checked;
	}
	return ECheckBoxState::Unchecked;
}

bool SN2CGraphListRow::IsViewButtonEnabled() const
{
	return Item.IsValid() && Item->bHasTranslation;
}

FText SN2CGraphListRow::GetViewButtonTooltip() const
{
	if (Item.IsValid() && Item->bHasTranslation)
	{
		return LOCTEXT("ViewTranslationTooltipEnabled", "View translation");
	}
	return LOCTEXT("ViewTranslationTooltipDisabled", "No translation available");
}

#undef LOCTEXT_NAMESPACE
