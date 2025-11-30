// Copyright (c) 2025 Nick McClure (Protospatial). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FBlueprintEditor;
class SN2CGraphOverlay;
class UEdGraph;

/**
 * Wrapper widget that wraps an existing SGraphEditor in an SOverlay
 * to add the NodeToCode overlay UI in the top-right corner.
 *
 * This widget is created when a graph tab is activated and wraps
 * the original content to inject our overlay UI.
 */
class NODETOCODE_API SN2CGraphEditorWrapper : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SN2CGraphEditorWrapper) {}
		/** The original graph editor widget to wrap */
		SLATE_ARGUMENT(TSharedPtr<SWidget>, GraphEditorContent)
		/** Weak reference to the Blueprint editor */
		SLATE_ARGUMENT(TWeakPtr<FBlueprintEditor>, BlueprintEditor)
		/** The graph being displayed */
		SLATE_ARGUMENT(UEdGraph*, Graph)
	SLATE_END_ARGS()

	/** Constructs this widget */
	void Construct(const FArguments& InArgs);

	/** Get the overlay widget */
	TSharedPtr<SN2CGraphOverlay> GetOverlay() const { return OverlayWidget; }

	/** Get the original wrapped content */
	TSharedPtr<SWidget> GetOriginalContent() const { return OriginalContent; }

	/** Get the graph this wrapper is for */
	UEdGraph* GetGraph() const { return Graph.Get(); }

	/** Update the overlay (e.g., when graph changes) */
	void UpdateOverlay();

	/** Check if this wrapper is for the given graph */
	bool IsForGraph(const UEdGraph* InGraph) const;

private:
	/** The overlay widget with buttons */
	TSharedPtr<SN2CGraphOverlay> OverlayWidget;

	/** Reference to the original graph editor content */
	TSharedPtr<SWidget> OriginalContent;

	/** Reference to the graph */
	TWeakObjectPtr<UEdGraph> Graph;

	/** Reference to the Blueprint editor */
	TWeakPtr<FBlueprintEditor> BlueprintEditor;
};
