// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/SHeaderRow.h"
#include "TagManager/Models/N2CTagManagerTypes.h"
#include "TagManager/Widgets/SN2CGraphListRow.h"

/**
 * Slate widget for displaying tagged graphs in a list/table view
 */
class NODETOCODE_API SN2CTaggedGraphsList : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SN2CTaggedGraphsList)
	{}
		/** Called when selection changes */
		SLATE_EVENT(FSimpleDelegate, OnSelectionChanged)
		/** Called when a graph row is double-clicked */
		SLATE_EVENT(FSimpleDelegate, OnGraphDoubleClicked)
		/** Called when translate button is clicked on a single graph */
		SLATE_EVENT(FSimpleDelegate, OnSingleTranslateRequested)
		/** Called when JSON export button is clicked on a single graph */
		SLATE_EVENT(FSimpleDelegate, OnSingleJsonExportRequested)
		/** Called when view translation button is clicked on a single graph */
		SLATE_EVENT(FSimpleDelegate, OnViewTranslationRequested)
	SLATE_END_ARGS()

	/** Constructs this widget */
	void Construct(const FArguments& InArgs);

	/** Set the graphs to display */
	void SetGraphs(const TArray<FN2CTagInfo>& InTagInfos);

	/** Set the header path display (e.g., "Category > Tag") */
	void SetHeaderPath(const FString& InCategory, const FString& InTag);

	/** Apply search filter to graphs */
	void SetSearchFilter(const FString& SearchText);

	/** Get currently selected graphs */
	TArray<FN2CTagInfo> GetSelectedGraphs() const;

	/** Get count of selected graphs */
	int32 GetSelectedCount() const;

	/** Select all graphs */
	void SelectAll();

	/** Deselect all graphs */
	void DeselectAll();

	/** Get the graph that was double-clicked */
	FN2CTagInfo GetDoubleClickedGraph() const;

	/** Get the graph for which translate was requested */
	FN2CTagInfo GetTranslateRequestedGraph() const;

	/** Get the graph for which JSON export was requested */
	FN2CTagInfo GetJsonExportRequestedGraph() const;

	/** Get the graph for which view translation was requested */
	FN2CTagInfo GetViewTranslationRequestedGraph() const;

	/** Delegate fired when selection changes */
	FSimpleDelegate OnSelectionChangedDelegate;

	/** Delegate fired when a graph is double-clicked */
	FSimpleDelegate OnGraphDoubleClickedDelegate;

	/** Delegate fired when translate is requested for a single graph */
	FSimpleDelegate OnSingleTranslateRequestedDelegate;

	/** Delegate fired when JSON export is requested for a single graph */
	FSimpleDelegate OnSingleJsonExportRequestedDelegate;

	/** Delegate fired when view translation is requested for a single graph */
	FSimpleDelegate OnViewTranslationRequestedDelegate;

private:
	/** Generate a row for the list view */
	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FN2CGraphListItem> Item, const TSharedRef<STableViewBase>& OwnerTable);

	/** Handle select-all checkbox */
	void OnSelectAllChanged(ECheckBoxState NewState);

	/** Get checkbox state for select-all */
	ECheckBoxState GetSelectAllCheckboxState() const;

	/** Handle select-all button click */
	FReply OnSelectAllClicked();

	/** Filter items based on current filters */
	void ApplyFilters();

	/** Check if item passes current filters */
	bool ItemPassesFilters(const TSharedPtr<FN2CGraphListItem>& Item) const;

	/** Update selection count display */
	void UpdateSelectionDisplay();

	// Row widget callbacks
	/** Handle checkbox change from row widget */
	void HandleRowCheckboxChanged(TSharedPtr<FN2CGraphListItem> Item);

	/** Handle translate button click from row widget */
	void HandleRowTranslateClicked(TSharedPtr<FN2CGraphListItem> Item);

	/** Handle JSON export button click from row widget */
	void HandleRowJsonExportClicked(TSharedPtr<FN2CGraphListItem> Item);

	/** Handle view translation button click from row widget */
	void HandleRowViewTranslationClicked(TSharedPtr<FN2CGraphListItem> Item);

	/** Handle double-click from row widget */
	void HandleRowDoubleClicked(TSharedPtr<FN2CGraphListItem> Item);

	/** The list view widget */
	TSharedPtr<SListView<TSharedPtr<FN2CGraphListItem>>> ListView;

	/** Header row */
	TSharedPtr<SHeaderRow> HeaderRow;

	/** Header path display */
	TSharedPtr<STextBlock> HeaderPathText;

	/** All items (unfiltered) */
	TArray<TSharedPtr<FN2CGraphListItem>> AllItems;

	/** Filtered items for display */
	TArray<TSharedPtr<FN2CGraphListItem>> FilteredItems;

	/** Current search filter */
	FString CurrentSearchFilter;

	/** Last double-clicked graph */
	FN2CTagInfo LastDoubleClickedGraph;

	/** Last graph for which translate was requested */
	FN2CTagInfo LastTranslateRequestedGraph;

	/** Last graph for which JSON export was requested */
	FN2CTagInfo LastJsonExportRequestedGraph;

	/** Last graph for which view translation was requested */
	FN2CTagInfo LastViewTranslationRequestedGraph;

	/** Column IDs */
	static const FName Column_Checkbox;
	static const FName Column_GraphName;
	static const FName Column_Blueprint;
};
