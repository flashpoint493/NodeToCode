// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"
#include "TagManager/Models/N2CTagManagerTypes.h"

/**
 * Slate widget for displaying categories and tags in a hierarchical tree view
 */
class NODETOCODE_API SN2CTagCategoryTree : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SN2CTagCategoryTree)
	{}
		/** Called when a tag is selected */
		SLATE_EVENT(FSimpleDelegate, OnSelectionChanged)
	SLATE_END_ARGS()

	/** Constructs this widget */
	void Construct(const FArguments& InArgs);

	/** Refresh tree data from tag manager */
	void RefreshData();

	/** Apply search filter to tree items */
	void SetSearchFilter(const FString& SearchText);

	/** Get the currently selected item */
	TSharedPtr<FN2CTreeItem> GetSelectedItem() const;

	/** Check if selection is a category */
	bool IsSelectedCategory() const;

	/** Check if selection is a tag */
	bool IsSelectedTag() const;

	/** Get selected tag name (empty if category selected) */
	FString GetSelectedTag() const;

	/** Get selected category name */
	FString GetSelectedCategory() const;

	/** Select a specific tag programmatically */
	void SelectTag(const FString& InTag, const FString& InCategory);

	/** Select a specific category programmatically */
	void SelectCategory(const FString& Category);

	/** Clear selection */
	void ClearSelection();

	/** Delegate fired when selection changes */
	FSimpleDelegate OnSelectionChangedDelegate;

private:
	/** Generate a row for the tree view */
	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FN2CTreeItem> Item, const TSharedRef<STableViewBase>& OwnerTable);

	/** Get children for a tree item */
	void OnGetChildren(TSharedPtr<FN2CTreeItem> Item, TArray<TSharedPtr<FN2CTreeItem>>& OutChildren);

	/** Handle selection change */
	void OnSelectionChanged(TSharedPtr<FN2CTreeItem> Item, ESelectInfo::Type SelectInfo);

	/** Handle expansion change */
	void OnExpansionChanged(TSharedPtr<FN2CTreeItem> Item, bool bIsExpanded);

	/** Build tree data from tag manager */
	void BuildTreeData();

	/** Check if item matches search filter */
	bool ItemMatchesFilter(const TSharedPtr<FN2CTreeItem>& Item) const;

	/** The tree view widget */
	TSharedPtr<STreeView<TSharedPtr<FN2CTreeItem>>> TreeView;

	/** Root items (categories) */
	TArray<TSharedPtr<FN2CTreeItem>> RootItems;

	/** All items (for searching) */
	TArray<TSharedPtr<FN2CTreeItem>> AllItems;

	/** Filtered items for display */
	TArray<TSharedPtr<FN2CTreeItem>> FilteredRootItems;

	/** Currently selected item */
	TSharedPtr<FN2CTreeItem> SelectedItem;

	/** Current search filter */
	FString CurrentSearchFilter;
};
