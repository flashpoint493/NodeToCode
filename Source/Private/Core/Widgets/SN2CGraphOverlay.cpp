// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#include "Core/Widgets/SN2CGraphOverlay.h"
#include "Core/N2CEditorIntegration.h"
#include "Core/N2CEditorWindow.h"
#include "Core/N2CTagManager.h"
#include "Core/N2CGraphStateManager.h"
#include "Utils/N2CLogger.h"
#include "BlueprintEditor.h"
#include "TagManager/Models/N2CTagManagerTypes.h"
#include "Framework/Docking/TabManager.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Styling/AppStyle.h"
#include "HAL/PlatformApplicationMisc.h"

#define LOCTEXT_NAMESPACE "SN2CGraphOverlay"

void SN2CGraphOverlay::Construct(const FArguments& InArgs)
{
	FN2CLogger::Get().Log(TEXT("SN2CGraphOverlay::Construct: ENTER"), EN2CLogSeverity::Warning);

	FN2CLogger::Get().Log(TEXT("SN2CGraphOverlay::Construct: Storing arguments"), EN2CLogSeverity::Warning);
	GraphGuid = InArgs._GraphGuid;
	BlueprintPath = InArgs._BlueprintPath;
	GraphName = InArgs._GraphName;
	BlueprintEditor = InArgs._BlueprintEditor;

	FN2CLogger::Get().Log(FString::Printf(TEXT("SN2CGraphOverlay::Construct: GraphName=%s, BlueprintPath=%s"), *GraphName, *BlueprintPath), EN2CLogSeverity::Warning);
	FN2CLogger::Get().Log(FString::Printf(TEXT("SN2CGraphOverlay::Construct: BlueprintEditor valid=%d"), BlueprintEditor.IsValid() ? 1 : 0), EN2CLogSeverity::Warning);

	// Get initial tag count
	FN2CLogger::Get().Log(TEXT("SN2CGraphOverlay::Construct: Getting initial tag count"), EN2CLogSeverity::Warning);
	TArray<FN2CTaggedBlueprintGraph> Tags = UN2CTagManager::Get().GetTagsForGraph(GraphGuid);
	CachedTagCount = Tags.Num();
	FN2CLogger::Get().Log(FString::Printf(TEXT("SN2CGraphOverlay::Construct: CachedTagCount=%d"), CachedTagCount), EN2CLogSeverity::Warning);

	// Subscribe to tag manager events
	FN2CLogger::Get().Log(TEXT("SN2CGraphOverlay::Construct: Subscribing to tag manager events"), EN2CLogSeverity::Warning);
	OnTagAddedHandle = UN2CTagManager::Get().OnBlueprintTagAdded.AddRaw(this, &SN2CGraphOverlay::OnTagAdded);
	OnTagRemovedHandle = UN2CTagManager::Get().OnBlueprintTagRemoved.AddRaw(this, &SN2CGraphOverlay::OnTagRemoved);
	FN2CLogger::Get().Log(TEXT("SN2CGraphOverlay::Construct: Tag event subscriptions done"), EN2CLogSeverity::Warning);

	// Subscribe to global translation state changes
	OnTranslationStateChangedHandle = FN2CEditorIntegration::Get().OnTranslationStateChanged.AddRaw(
		this, &SN2CGraphOverlay::OnGlobalTranslationStateChanged);

	// Initialize local state from global state
	bIsTranslating = FN2CEditorIntegration::Get().IsAnyTranslationInProgress();

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.BorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.02f, 0.85f))
		.Padding(FMargin(6.0f, 4.0f))
		[
			SNew(SHorizontalBox)

			// Open Window button
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 4.0f, 0.0f)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.ToolTipText(LOCTEXT("OpenWindowTooltip", "Open NodeToCode Window"))
				.OnClicked(this, &SN2CGraphOverlay::OnOpenWindowClicked)
				.ContentPadding(FMargin(4.0f, 2.0f))
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("Icons.Layout"))
				]
			]

			// Separator
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(2.0f, 0.0f)
			[
				SNew(SSeparator)
				.Orientation(Orient_Vertical)
				.Thickness(1.0f)
			]

			// Tag button with count badge
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.0f, 0.0f, 4.0f, 0.0f)
			[
				SAssignNew(TagMenuAnchor, SMenuAnchor)
				.Placement(MenuPlacement_BelowAnchor)
				.OnGetMenuContent(this, &SN2CGraphOverlay::CreateTagPopoverContent)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.ToolTipText(this, &SN2CGraphOverlay::GetTagButtonTooltip)
					.OnClicked(this, &SN2CGraphOverlay::OnTagButtonClicked)
					.ContentPadding(FMargin(4.0f, 2.0f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SImage)
							.Image(FAppStyle::GetBrush("Icons.Star"))
							.ColorAndOpacity(this, &SN2CGraphOverlay::GetTagButtonColor)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(4.0f, 0.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock)
							.Text(this, &SN2CGraphOverlay::GetTagCountText)
							.TextStyle(FAppStyle::Get(), "SmallText")
							.ColorAndOpacity(this, &SN2CGraphOverlay::GetTagButtonColor)
						]
					]
				]
			]

			// Separator
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(2.0f, 0.0f)
			[
				SNew(SSeparator)
				.Orientation(Orient_Vertical)
				.Thickness(1.0f)
			]

			// Copy JSON button
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.0f, 0.0f)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.ToolTipText(this, &SN2CGraphOverlay::GetCopyJsonTooltip)
				.OnClicked(this, &SN2CGraphOverlay::OnCopyJsonClicked)
				.ContentPadding(FMargin(4.0f, 2.0f))
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("Icons.Clipboard"))
				]
			]

			// Separator
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(2.0f, 0.0f)
			[
				SNew(SSeparator)
				.Orientation(Orient_Vertical)
				.Thickness(1.0f)
			]

			// Translate button
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.ToolTipText(this, &SN2CGraphOverlay::GetTranslateTooltip)
				.OnClicked(this, &SN2CGraphOverlay::OnTranslateClicked)
				.IsEnabled_Lambda([this]() { return !bIsTranslating; })
				.ContentPadding(FMargin(4.0f, 2.0f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush("Icons.Play"))
						.Visibility(this, &SN2CGraphOverlay::GetTranslateTextVisibility)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SAssignNew(TranslateSpinner, SThrobber)
						.Visibility(this, &SN2CGraphOverlay::GetSpinnerVisibility)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(4.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("TranslateButton", "Translate"))
						.TextStyle(FAppStyle::Get(), "SmallText")
					]
				]
			]
		]
	];

	FN2CLogger::Get().Log(TEXT("SN2CGraphOverlay::Construct: Widget hierarchy built successfully"), EN2CLogSeverity::Warning);
	FN2CLogger::Get().Log(FString::Printf(TEXT("SN2CGraphOverlay::Construct: TagMenuAnchor valid=%d"), TagMenuAnchor.IsValid() ? 1 : 0), EN2CLogSeverity::Warning);
	FN2CLogger::Get().Log(FString::Printf(TEXT("SN2CGraphOverlay::Construct: TranslateSpinner valid=%d"), TranslateSpinner.IsValid() ? 1 : 0), EN2CLogSeverity::Warning);
	FN2CLogger::Get().Log(TEXT("SN2CGraphOverlay::Construct: EXIT"), EN2CLogSeverity::Warning);
}

SN2CGraphOverlay::~SN2CGraphOverlay()
{
	// Unsubscribe from tag manager events
	if (OnTagAddedHandle.IsValid())
	{
		UN2CTagManager::Get().OnBlueprintTagAdded.Remove(OnTagAddedHandle);
	}
	if (OnTagRemovedHandle.IsValid())
	{
		UN2CTagManager::Get().OnBlueprintTagRemoved.Remove(OnTagRemovedHandle);
	}
	// Unsubscribe from global translation state
	if (OnTranslationStateChangedHandle.IsValid())
	{
		FN2CEditorIntegration::Get().OnTranslationStateChanged.Remove(OnTranslationStateChangedHandle);
	}
}

void SN2CGraphOverlay::RefreshTagCount()
{
	TArray<FN2CTaggedBlueprintGraph> Tags = UN2CTagManager::Get().GetTagsForGraph(GraphGuid);
	CachedTagCount = Tags.Num();
}

void SN2CGraphOverlay::SetTranslating(bool bInProgress)
{
	bIsTranslating = bInProgress;
}

FReply SN2CGraphOverlay::OnCopyJsonClicked()
{
	// Ensure the editor integration has the correct active editor
	// Use our stored BlueprintEditor reference which is guaranteed to be the correct one
	if (BlueprintEditor.IsValid())
	{
		FN2CEditorIntegration::Get().StoreActiveBlueprintEditor(BlueprintEditor);
	}
	else
	{
		FN2CLogger::Get().Log(TEXT("Blueprint editor reference is invalid"),
			EN2CLogSeverity::Warning, TEXT("SN2CGraphOverlay"));
		return FReply::Handled();
	}

	FString ErrorMsg;
	FString JsonString = FN2CEditorIntegration::Get().GetFocusedBlueprintAsJson(true, ErrorMsg);

	if (!JsonString.IsEmpty())
	{
		FPlatformApplicationMisc::ClipboardCopy(*JsonString);
		FN2CLogger::Get().Log(TEXT("Blueprint graph JSON copied to clipboard"),
			EN2CLogSeverity::Info, TEXT("SN2CGraphOverlay"));
	}
	else
	{
		FN2CLogger::Get().Log(FString::Printf(TEXT("Failed to copy JSON: %s"), *ErrorMsg),
			EN2CLogSeverity::Warning, TEXT("SN2CGraphOverlay"));
	}

	return FReply::Handled();
}

FReply SN2CGraphOverlay::OnTranslateClicked()
{
	// Check global translation state
	if (FN2CEditorIntegration::Get().IsAnyTranslationInProgress())
	{
		FN2CLogger::Get().Log(TEXT("Translation already in progress globally"),
			EN2CLogSeverity::Warning, TEXT("SN2CGraphOverlay"));
		return FReply::Handled();
	}

	// Ensure the editor integration has the correct active editor
	if (BlueprintEditor.IsValid())
	{
		FN2CEditorIntegration::Get().StoreActiveBlueprintEditor(BlueprintEditor);
	}
	else
	{
		FN2CLogger::Get().Log(TEXT("Blueprint editor reference is invalid"),
			EN2CLogSeverity::Warning, TEXT("SN2CGraphOverlay"));
		return FReply::Handled();
	}

	// Request translation through the central system
	// This broadcasts to the main window which will show the progress modal
	FN2CEditorIntegration::Get().RequestOverlayTranslation(GraphGuid, GraphName, BlueprintPath);

	return FReply::Handled();
}

FReply SN2CGraphOverlay::OnTagButtonClicked()
{
	if (TagMenuAnchor.IsValid())
	{
		TagMenuAnchor->SetIsOpen(!TagMenuAnchor->IsOpen());
	}
	return FReply::Handled();
}

FReply SN2CGraphOverlay::OnOpenWindowClicked()
{
	// Open the main NodeToCode window using the global tab manager
	FGlobalTabmanager::Get()->TryInvokeTab(SN2CEditorWindow::TabId);

	FN2CLogger::Get().Log(TEXT("NodeToCode window opened from graph overlay"),
		EN2CLogSeverity::Info, TEXT("SN2CGraphOverlay"));

	return FReply::Handled();
}

TSharedRef<SWidget> SN2CGraphOverlay::CreateTagPopoverContent()
{
	TArray<FN2CTaggedBlueprintGraph> Tags = UN2CTagManager::Get().GetTagsForGraph(GraphGuid);

	TSharedRef<SVerticalBox> TagList = SNew(SVerticalBox);

	// Header
	TagList->AddSlot()
	.AutoHeight()
	.Padding(8.0f, 6.0f)
	[
		SNew(STextBlock)
		.Text(FText::Format(LOCTEXT("TagsForGraph", "Tags for \"{0}\""), FText::FromString(GraphName)))
		.TextStyle(FAppStyle::Get(), "NormalText.Important")
	];

	// Separator
	TagList->AddSlot()
	.AutoHeight()
	.Padding(8.0f, 0.0f)
	[
		SNew(SSeparator)
	];

	// Tag list
	if (Tags.Num() > 0)
	{
		TSharedRef<SScrollBox> ScrollBox = SNew(SScrollBox);

		for (const FN2CTaggedBlueprintGraph& TagItem : Tags)
		{
			ScrollBox->AddSlot()
			.Padding(8.0f, 4.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("\u2022"))) // Bullet character
					.TextStyle(FAppStyle::Get(), "SmallText")
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				.Padding(4.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(FText::Format(LOCTEXT("TagDisplay", "{0} ({1})"),
						FText::FromString(TagItem.Tag), FText::FromString(TagItem.Category)))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.ToolTipText(LOCTEXT("RemoveTagTooltip", "Remove this tag"))
					.OnClicked_Lambda([this, TagItem]()
					{
						OnRemoveTagRequested(TagItem);
						return FReply::Handled();
					})
					.ContentPadding(FMargin(2.0f))
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush("Icons.X"))
						.ColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.2f, 0.2f)))
					]
				]
			];
		}

		TagList->AddSlot()
		.AutoHeight()
		.MaxHeight(150.0f)
		[
			ScrollBox
		];
	}
	else
	{
		TagList->AddSlot()
		.AutoHeight()
		.Padding(8.0f, 8.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NoTags", "No tags applied"))
			.TextStyle(FAppStyle::Get(), "NormalText.Subdued")
		];
	}

	// Separator
	TagList->AddSlot()
	.AutoHeight()
	.Padding(8.0f, 4.0f)
	[
		SNew(SSeparator)
	];

	// Add new tag section header
	TagList->AddSlot()
	.AutoHeight()
	.Padding(8.0f, 4.0f, 8.0f, 2.0f)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("AddNewTagHeader", "Add New Tag"))
		.TextStyle(FAppStyle::Get(), "SmallText")
		.ColorAndOpacity(FSlateColor::UseSubduedForeground())
	];

	// Get existing tags and categories for the dropdowns
	TArray<FString> ExistingTags = UN2CTagManager::Get().GetAllTagNames();
	TArray<FString> ExistingCategories = UN2CTagManager::Get().GetAllCategories();

	// Ensure "Default" is always an option for categories
	if (!ExistingCategories.Contains(TEXT("Default")))
	{
		ExistingCategories.Insert(TEXT("Default"), 0);
	}

	// Populate member arrays for combo box options (must persist beyond this function)
	TagOptions.Empty();
	for (const FString& TagName : ExistingTags)
	{
		TagOptions.Add(MakeShared<FString>(TagName));
	}

	CategoryOptions.Empty();
	for (const FString& Category : ExistingCategories)
	{
		CategoryOptions.Add(MakeShared<FString>(Category));
	}

	// Add tag input fields
	TSharedPtr<SEditableTextBox> TagInputBox;
	TSharedPtr<SEditableTextBox> CategoryInputBox;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> TagComboBox;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> CategoryComboBox;

	// Tag name row with combo box and text input
	TagList->AddSlot()
	.AutoHeight()
	.Padding(8.0f, 2.0f, 8.0f, 2.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.0f, 0.0f, 8.0f, 0.0f)
		[
			SNew(SBox)
			.WidthOverride(60.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("TagLabel", "Tag:"))
			]
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)
			// Editable text box for tag
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SAssignNew(TagInputBox, SEditableTextBox)
				.HintText(LOCTEXT("TagNameHint", "Enter or select..."))
			]
			// Dropdown button for existing tags (only show if there are existing tags)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SBox)
				.Visibility(TagOptions.Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed)
				[
					SAssignNew(TagComboBox, SComboBox<TSharedPtr<FString>>)
					.OptionsSource(&TagOptions)
					.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
					{
						return SNew(STextBlock).Text(FText::FromString(*Item));
					})
					.OnSelectionChanged_Lambda([TagInputBox](TSharedPtr<FString> SelectedItem, ESelectInfo::Type SelectInfo)
					{
						if (SelectedItem.IsValid() && TagInputBox.IsValid())
						{
							TagInputBox->SetText(FText::FromString(*SelectedItem));
						}
					})
					[
						SNew(STextBlock)
						.Text(FText::GetEmpty())
					]
				]
			]
		]
	];

	// Category row with combo box and text input
	TagList->AddSlot()
	.AutoHeight()
	.Padding(8.0f, 2.0f, 8.0f, 4.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.0f, 0.0f, 8.0f, 0.0f)
		[
			SNew(SBox)
			.WidthOverride(60.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("CategoryLabel", "Category:"))
			]
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)
			// Editable text box for category
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SAssignNew(CategoryInputBox, SEditableTextBox)
				.HintText(LOCTEXT("CategoryHint", "Enter or select..."))
				.Text(FText::FromString(TEXT("Default")))
			]
			// Dropdown button for existing categories
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f, 0.0f, 0.0f)
			[
				SAssignNew(CategoryComboBox, SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&CategoryOptions)
				.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
				{
					return SNew(STextBlock).Text(FText::FromString(*Item));
				})
				.OnSelectionChanged_Lambda([CategoryInputBox](TSharedPtr<FString> SelectedItem, ESelectInfo::Type SelectInfo)
				{
					if (SelectedItem.IsValid() && CategoryInputBox.IsValid())
					{
						CategoryInputBox->SetText(FText::FromString(*SelectedItem));
					}
				})
				[
					SNew(STextBlock)
					.Text(FText::GetEmpty())
				]
			]
		]
	];

	// Add button
	TagList->AddSlot()
	.AutoHeight()
	.Padding(8.0f, 4.0f, 8.0f, 8.0f)
	[
		SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "Button")
		.HAlign(HAlign_Center)
		.OnClicked_Lambda([this, TagInputBox, CategoryInputBox]()
		{
			if (TagInputBox.IsValid() && CategoryInputBox.IsValid())
			{
				FString TagName = TagInputBox->GetText().ToString();
				FString CategoryName = CategoryInputBox->GetText().ToString();

				if (TagName.IsEmpty())
				{
					return FReply::Handled();
				}

				if (CategoryName.IsEmpty())
				{
					CategoryName = TEXT("Default");
				}

				OnAddTagRequested(TagName, CategoryName);
			}
			return FReply::Handled();
		})
		.ContentPadding(FMargin(8.0f, 4.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("Icons.Plus"))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.2f, 0.8f, 0.2f)))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("AddTagButton", "Add Tag"))
			]
		]
	];

	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(0.0f)
		[
			SNew(SBox)
			.MinDesiredWidth(280.0f)
			[
				TagList
			]
		];
}

void SN2CGraphOverlay::OnAddTagRequested(const FString& TagName, const FString& CategoryName)
{
	FN2CTaggedBlueprintGraph NewTag(
		TagName,
		CategoryName,
		TEXT(""), // Description
		GraphGuid,
		GraphName,
		FSoftObjectPath(BlueprintPath)
	);

	if (UN2CTagManager::Get().AddTag(NewTag))
	{
		RefreshTagCount();
		// Refresh the popover by toggling it
		if (TagMenuAnchor.IsValid() && TagMenuAnchor->IsOpen())
		{
			TagMenuAnchor->SetIsOpen(false);
			TagMenuAnchor->SetIsOpen(true);
		}
	}
}

void SN2CGraphOverlay::OnRemoveTagRequested(const FN2CTaggedBlueprintGraph& TagInfo)
{
	if (UN2CTagManager::Get().RemoveTag(TagInfo.GraphGuid, TagInfo.Tag, TagInfo.Category))
	{
		RefreshTagCount();
		// Refresh the popover by toggling it
		if (TagMenuAnchor.IsValid() && TagMenuAnchor->IsOpen())
		{
			TagMenuAnchor->SetIsOpen(false);
			TagMenuAnchor->SetIsOpen(true);
		}
	}
}

void SN2CGraphOverlay::OnTagAdded(const FN2CTaggedBlueprintGraph& TagInfo)
{
	if (TagInfo.GraphGuid == GraphGuid)
	{
		RefreshTagCount();
	}
}

void SN2CGraphOverlay::OnTagRemoved(const FGuid& RemovedGraphGuid, const FString& RemovedTag)
{
	if (RemovedGraphGuid == GraphGuid)
	{
		RefreshTagCount();
	}
}

void SN2CGraphOverlay::OnGlobalTranslationStateChanged(bool bInProgress)
{
	// Update local state from global state
	// This ensures all overlays are synchronized
	bIsTranslating = bInProgress;
}

EVisibility SN2CGraphOverlay::GetSpinnerVisibility() const
{
	return bIsTranslating ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SN2CGraphOverlay::GetTranslateTextVisibility() const
{
	return bIsTranslating ? EVisibility::Collapsed : EVisibility::Visible;
}

FText SN2CGraphOverlay::GetTagCountText() const
{
	return FText::AsNumber(CachedTagCount);
}

FText SN2CGraphOverlay::GetCopyJsonTooltip() const
{
	return LOCTEXT("CopyJsonTooltip", "Copy Blueprint graph JSON to clipboard");
}

FText SN2CGraphOverlay::GetTranslateTooltip() const
{
	if (bIsTranslating)
	{
		return LOCTEXT("TranslatingTooltip", "Translation in progress...");
	}
	return LOCTEXT("TranslateTooltip", "Translate this graph using the configured LLM");
}

FText SN2CGraphOverlay::GetTagButtonTooltip() const
{
	if (CachedTagCount == 0)
	{
		return LOCTEXT("NoTagsTooltip", "No tags - click to add");
	}
	return FText::Format(LOCTEXT("TagCountTooltip", "{0} tag(s) - click to manage"), FText::AsNumber(CachedTagCount));
}

FSlateColor SN2CGraphOverlay::GetTagButtonColor() const
{
	if (CachedTagCount > 0)
	{
		return FSlateColor(FLinearColor(0.83f, 0.63f, 0.29f)); // Gold/amber for tagged
	}
	return FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)); // Gray for untagged
}

#undef LOCTEXT_NAMESPACE
