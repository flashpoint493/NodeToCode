// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#include "TagManager/Widgets/SN2CTaggedGraphsList.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SButton.h"
#include "Styling/AppStyle.h"
#include "Misc/Paths.h"
#include "Models/N2CLogging.h"

#define LOCTEXT_NAMESPACE "SN2CTaggedGraphsList"

const FName SN2CTaggedGraphsList::Column_Checkbox = TEXT("Checkbox");
const FName SN2CTaggedGraphsList::Column_GraphName = TEXT("GraphName");
const FName SN2CTaggedGraphsList::Column_Blueprint = TEXT("Blueprint");

void SN2CTaggedGraphsList::Construct(const FArguments& InArgs)
{
	OnSelectionChangedDelegate = InArgs._OnSelectionChanged;
	OnGraphDoubleClickedDelegate = InArgs._OnGraphDoubleClicked;
	OnSingleTranslateRequestedDelegate = InArgs._OnSingleTranslateRequested;
	OnSingleJsonExportRequestedDelegate = InArgs._OnSingleJsonExportRequested;
	OnViewTranslationRequestedDelegate = InArgs._OnViewTranslationRequested;

	// Create header row
	// Note: We use a button wrapper for the select-all checkbox because SHeaderRow
	// intercepts mouse clicks for column sorting, preventing the checkbox's OnCheckStateChanged
	// from being triggered. The button ensures our click handler is called.
	HeaderRow = SNew(SHeaderRow)
		+ SHeaderRow::Column(Column_Checkbox)
		.DefaultLabel(FText::GetEmpty())
		.FixedWidth(30.0f)
		.HeaderContent()
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "NoBorder")
			.ContentPadding(0.0f)
			.OnClicked(this, &SN2CTaggedGraphsList::OnSelectAllClicked)
			[
				SNew(SCheckBox)
				.IsChecked(this, &SN2CTaggedGraphsList::GetSelectAllCheckboxState)
				// HitTestInvisible makes the checkbox not respond to clicks
				// while still appearing enabled - the parent button handles the click
				.Visibility(EVisibility::HitTestInvisible)
			]
		]
		+ SHeaderRow::Column(Column_GraphName)
		.DefaultLabel(LOCTEXT("GraphNameHeader", "Graph Name"))
		.FillWidth(1.0f)
		+ SHeaderRow::Column(Column_Blueprint)
		.DefaultLabel(LOCTEXT("BlueprintHeader", "Blueprint"))
		.FillWidth(1.0f);

	ChildSlot
	[
		SNew(SVerticalBox)
		// Header path display
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 4.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(4.0f)
			[
				SAssignNew(HeaderPathText, STextBlock)
				.Text(LOCTEXT("SelectTagPrompt", "Select a category or tag"))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
		]
		// List view
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(0.0f)
			[
				SAssignNew(ListView, SListView<TSharedPtr<FN2CGraphListItem>>)
				.ListItemsSource(&FilteredItems)
				.OnGenerateRow(this, &SN2CTaggedGraphsList::OnGenerateRow)
				.OnMouseButtonDoubleClick(this, &SN2CTaggedGraphsList::OnRowDoubleClicked)
				.SelectionMode(ESelectionMode::None) // We handle selection via checkboxes
				.HeaderRow(HeaderRow)
			]
		]
	];
}

void SN2CTaggedGraphsList::SetGraphs(const TArray<FN2CTagInfo>& InTagInfos)
{
	AllItems.Empty();

	for (const FN2CTagInfo& TagInfo : InTagInfos)
	{
		TSharedPtr<FN2CGraphListItem> Item = MakeShared<FN2CGraphListItem>();
		Item->TagInfo = TagInfo;
		Item->bIsSelected = false;
		Item->bIsStarred = false;
		AllItems.Add(Item);
	}

	// Sort by graph name
	AllItems.Sort([](const TSharedPtr<FN2CGraphListItem>& A, const TSharedPtr<FN2CGraphListItem>& B)
	{
		return A->TagInfo.GraphName < B->TagInfo.GraphName;
	});

	ApplyFilters();
}

void SN2CTaggedGraphsList::SetHeaderPath(const FString& InCategory, const FString& InTag)
{
	if (HeaderPathText.IsValid())
	{
		if (InTag.IsEmpty())
		{
			// Category only
			HeaderPathText->SetText(FText::Format(LOCTEXT("CategoryPathFormat", "Category: {0}"), FText::FromString(InCategory)));
		}
		else
		{
			// Category > Tag
			HeaderPathText->SetText(FText::Format(LOCTEXT("TagPathFormat", "{0} > {1}"), FText::FromString(InCategory), FText::FromString(InTag)));
		}
	}
}

void SN2CTaggedGraphsList::SetSearchFilter(const FString& SearchText)
{
	CurrentSearchFilter = SearchText;
	ApplyFilters();
}

TArray<FN2CTagInfo> SN2CTaggedGraphsList::GetSelectedGraphs() const
{
	TArray<FN2CTagInfo> Selected;
	for (const TSharedPtr<FN2CGraphListItem>& Item : AllItems)
	{
		if (Item->bIsSelected)
		{
			Selected.Add(Item->TagInfo);
		}
	}
	return Selected;
}

int32 SN2CTaggedGraphsList::GetSelectedCount() const
{
	int32 Count = 0;
	for (const TSharedPtr<FN2CGraphListItem>& Item : AllItems)
	{
		if (Item->bIsSelected)
		{
			Count++;
		}
	}
	return Count;
}

void SN2CTaggedGraphsList::SelectAll()
{
	for (TSharedPtr<FN2CGraphListItem>& Item : FilteredItems)
	{
		Item->bIsSelected = true;
	}
	if (ListView.IsValid())
	{
		ListView->RequestListRefresh();
	}
	OnSelectionChangedDelegate.ExecuteIfBound();
}

void SN2CTaggedGraphsList::DeselectAll()
{
	for (TSharedPtr<FN2CGraphListItem>& Item : AllItems)
	{
		Item->bIsSelected = false;
	}
	if (ListView.IsValid())
	{
		ListView->RequestListRefresh();
	}
	OnSelectionChangedDelegate.ExecuteIfBound();
}

FN2CTagInfo SN2CTaggedGraphsList::GetDoubleClickedGraph() const
{
	return LastDoubleClickedGraph;
}

FN2CTagInfo SN2CTaggedGraphsList::GetTranslateRequestedGraph() const
{
	return LastTranslateRequestedGraph;
}

FN2CTagInfo SN2CTaggedGraphsList::GetJsonExportRequestedGraph() const
{
	return LastJsonExportRequestedGraph;
}

FN2CTagInfo SN2CTaggedGraphsList::GetViewTranslationRequestedGraph() const
{
	return LastViewTranslationRequestedGraph;
}

TSharedRef<ITableRow> SN2CTaggedGraphsList::OnGenerateRow(TSharedPtr<FN2CGraphListItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	// Capture Item by value for lambdas to ensure it stays valid
	TSharedPtr<FN2CGraphListItem> ItemCopy = Item;

	return SNew(STableRow<TSharedPtr<FN2CGraphListItem>>, OwnerTable)
		.Style(&FAppStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.Row"))
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("NoBorder"))
			// Use lambda for dynamic background color based on selection state
			.BorderBackgroundColor_Lambda([ItemCopy]()
			{
				return ItemCopy->bIsSelected
					? FSlateColor(FLinearColor(0.83f, 0.63f, 0.29f, 0.15f)) // Subtle orange for selected
					: FSlateColor(FLinearColor::Transparent);
			})
			.Padding(FMargin(0.0f, 2.0f))
			[
				SNew(SHorizontalBox)
				// Checkbox column
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Padding(4.0f, 0.0f)
				[
					SNew(SBox)
					.WidthOverride(22.0f)
					[
						SNew(SCheckBox)
						// Use lambda for dynamic checkbox state
						.IsChecked_Lambda([ItemCopy]()
						{
							return ItemCopy->bIsSelected ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
						})
						.OnCheckStateChanged(this, &SN2CTaggedGraphsList::OnCheckboxChanged, Item)
					]
				]
				// Graph name column
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				.Padding(4.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Item->TagInfo.GraphName))
					.Font(FAppStyle::GetFontStyle("SmallFont"))
				]
				// Blueprint column
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				.Padding(4.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Item->GetBlueprintDisplayName()))
					.Font(FAppStyle::GetFontStyle("SmallFont"))
					.ColorAndOpacity(FLinearColor(0.31f, 0.76f, 1.0f, 1.0f)) // Blue for blueprint names
				]
				// Action buttons column (always visible for MVP)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				.Padding(4.0f, 0.0f)
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
						.ContentPadding(2.0f)
						.OnClicked(this, &SN2CTaggedGraphsList::OnTranslateClicked, ItemCopy)
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("\u27A1"))) // Arrow symbol for translate
							.Font(FAppStyle::GetFontStyle("NormalFont"))
						]
					]
					// JSON export button
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2.0f, 0.0f)
					[
						SNew(SButton)
						.ButtonStyle(FAppStyle::Get(), "SimpleButton")
						.ToolTipText(LOCTEXT("JsonTooltip", "Export as JSON"))
						.ContentPadding(2.0f)
						.OnClicked(this, &SN2CTaggedGraphsList::OnJsonExportClicked, ItemCopy)
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("{}"))) // Braces for JSON
							.Font(FAppStyle::GetFontStyle("SmallFont"))
						]
					]
					// View translation button (disabled for MVP)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2.0f, 0.0f)
					[
						SNew(SButton)
						.ButtonStyle(FAppStyle::Get(), "SimpleButton")
						.IsEnabled(false) // Always disabled for MVP
						.ToolTipText(LOCTEXT("ViewTooltip", "View translation (not available)"))
						.ContentPadding(2.0f)
						.OnClicked(this, &SN2CTaggedGraphsList::OnViewTranslationClicked, ItemCopy)
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("\u{1F441}"))) // Eye symbol for view (fallback to simple text)
							.Font(FAppStyle::GetFontStyle("SmallFont"))
						]
					]
				]
			]
		];
}

void SN2CTaggedGraphsList::OnCheckboxChanged(ECheckBoxState NewState, TSharedPtr<FN2CGraphListItem> Item)
{
	if (Item.IsValid())
	{
		Item->bIsSelected = (NewState == ECheckBoxState::Checked);
		if (ListView.IsValid())
		{
			ListView->RequestListRefresh();
		}
		OnSelectionChangedDelegate.ExecuteIfBound();
	}
}

void SN2CTaggedGraphsList::OnSelectAllChanged(ECheckBoxState NewState)
{
	bool bSelect = (NewState == ECheckBoxState::Checked);
	for (TSharedPtr<FN2CGraphListItem>& Item : FilteredItems)
	{
		Item->bIsSelected = bSelect;
	}
	if (ListView.IsValid())
	{
		ListView->RequestListRefresh();
	}
	OnSelectionChangedDelegate.ExecuteIfBound();
}

FReply SN2CTaggedGraphsList::OnSelectAllClicked()
{
	UE_LOG(LogNodeToCode, Log, TEXT("[SN2CTaggedGraphsList] OnSelectAllClicked called"));

	// Toggle selection: if all are selected, deselect all; otherwise select all
	ECheckBoxState CurrentState = GetSelectAllCheckboxState();
	UE_LOG(LogNodeToCode, Log, TEXT("[SN2CTaggedGraphsList] Current checkbox state: %d (0=Unchecked, 1=Checked, 2=Undetermined)"), (int32)CurrentState);

	bool bShouldSelectAll = (CurrentState != ECheckBoxState::Checked);
	UE_LOG(LogNodeToCode, Log, TEXT("[SN2CTaggedGraphsList] Should select all: %s, FilteredItems count: %d"),
		bShouldSelectAll ? TEXT("true") : TEXT("false"), FilteredItems.Num());

	for (TSharedPtr<FN2CGraphListItem>& Item : FilteredItems)
	{
		Item->bIsSelected = bShouldSelectAll;
	}

	if (ListView.IsValid())
	{
		UE_LOG(LogNodeToCode, Log, TEXT("[SN2CTaggedGraphsList] Calling RequestListRefresh"));
		ListView->RequestListRefresh();
	}
	else
	{
		UE_LOG(LogNodeToCode, Warning, TEXT("[SN2CTaggedGraphsList] ListView is not valid!"));
	}

	OnSelectionChangedDelegate.ExecuteIfBound();
	UE_LOG(LogNodeToCode, Log, TEXT("[SN2CTaggedGraphsList] OnSelectAllClicked complete"));

	return FReply::Handled();
}

void SN2CTaggedGraphsList::OnRowDoubleClicked(TSharedPtr<FN2CGraphListItem> Item)
{
	if (Item.IsValid())
	{
		LastDoubleClickedGraph = Item->TagInfo;
		OnGraphDoubleClickedDelegate.ExecuteIfBound();
	}
}

ECheckBoxState SN2CTaggedGraphsList::GetSelectAllCheckboxState() const
{
	if (FilteredItems.Num() == 0)
	{
		return ECheckBoxState::Unchecked;
	}

	int32 SelectedCount = 0;
	for (const TSharedPtr<FN2CGraphListItem>& Item : FilteredItems)
	{
		if (Item->bIsSelected)
		{
			SelectedCount++;
		}
	}

	if (SelectedCount == 0)
	{
		return ECheckBoxState::Unchecked;
	}
	else if (SelectedCount == FilteredItems.Num())
	{
		return ECheckBoxState::Checked;
	}
	return ECheckBoxState::Undetermined;
}

void SN2CTaggedGraphsList::ApplyFilters()
{
	FilteredItems.Empty();

	for (const TSharedPtr<FN2CGraphListItem>& Item : AllItems)
	{
		if (ItemPassesFilters(Item))
		{
			FilteredItems.Add(Item);
		}
	}

	if (ListView.IsValid())
	{
		ListView->RequestListRefresh();
	}
}

bool SN2CTaggedGraphsList::ItemPassesFilters(const TSharedPtr<FN2CGraphListItem>& Item) const
{
	// Search filter
	if (!CurrentSearchFilter.IsEmpty())
	{
		bool bMatchesGraph = Item->TagInfo.GraphName.Contains(CurrentSearchFilter, ESearchCase::IgnoreCase);
		bool bMatchesBlueprint = Item->TagInfo.BlueprintPath.Contains(CurrentSearchFilter, ESearchCase::IgnoreCase);
		if (!bMatchesGraph && !bMatchesBlueprint)
		{
			return false;
		}
	}

	return true;
}

void SN2CTaggedGraphsList::UpdateSelectionDisplay()
{
	// This could update a selection count display if added to the UI
}

FReply SN2CTaggedGraphsList::OnTranslateClicked(TSharedPtr<FN2CGraphListItem> Item)
{
	if (Item.IsValid())
	{
		UE_LOG(LogNodeToCode, Log, TEXT("[SN2CTaggedGraphsList] Translate clicked for graph: %s"), *Item->TagInfo.GraphName);
		LastTranslateRequestedGraph = Item->TagInfo;
		OnSingleTranslateRequestedDelegate.ExecuteIfBound();
	}
	return FReply::Handled();
}

FReply SN2CTaggedGraphsList::OnJsonExportClicked(TSharedPtr<FN2CGraphListItem> Item)
{
	if (Item.IsValid())
	{
		UE_LOG(LogNodeToCode, Log, TEXT("[SN2CTaggedGraphsList] JSON export clicked for graph: %s"), *Item->TagInfo.GraphName);
		LastJsonExportRequestedGraph = Item->TagInfo;
		OnSingleJsonExportRequestedDelegate.ExecuteIfBound();
	}
	return FReply::Handled();
}

FReply SN2CTaggedGraphsList::OnViewTranslationClicked(TSharedPtr<FN2CGraphListItem> Item)
{
	if (Item.IsValid())
	{
		UE_LOG(LogNodeToCode, Log, TEXT("[SN2CTaggedGraphsList] View translation clicked for graph: %s"), *Item->TagInfo.GraphName);
		LastViewTranslationRequestedGraph = Item->TagInfo;
		OnViewTranslationRequestedDelegate.ExecuteIfBound();
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
