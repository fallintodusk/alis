// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class UWidget;

/**
 * Debug overlay settings for filtering and display options.
 */
struct PROJECTUI_API FProjectUIDebugOverlaySettings
{
	/** Show widget names */
	bool bShowNames = true;

	/** Show widget sizes */
	bool bShowSizes = false;

	/** Show Z-order values */
	bool bShowZOrder = false;

	/** Show only problematic widgets (zero size, invisible, etc.) */
	bool bFilterProblems = false;

	/** Filter by widget name pattern (empty = no filter) */
	FString NameFilter;

	/** Filter by layer name (empty = no filter) */
	FString LayerFilter;
};

/**
 * Widget state for debug overlay coloring.
 */
enum class EProjectDebugWidgetState : uint8
{
	/** Normal visible widget with correct size */
	Normal,

	/** Widget is being clipped by parent */
	Clipped,

	/** Widget has zero arranged size */
	ZeroSize,

	/** Widget is not visible */
	Hidden,

	/** Widget is currently focused */
	Focused,

	/** Widget has NO_VIEWMODEL issue */
	NoViewModel
};

/**
 * Slate widget that draws debug overlay rectangles over all UI widgets.
 *
 * Features:
 * - Colored bounds per widget state (green=ok, yellow=clipped, red=zero/hidden, blue=focused)
 * - Optional labels showing name, size, Z-order
 * - Filtering by name pattern or problems only
 *
 * Console commands (registered by ProjectUIDebugSubsystem):
 * - UI.Debug.Overlay [0/1] - Toggle overlay
 * - UI.Debug.Overlay.ShowNames [0/1] - Toggle name labels
 * - UI.Debug.Overlay.ShowSizes [0/1] - Toggle size labels
 * - UI.Debug.Overlay.FilterProblems [0/1] - Show only problematic widgets
 * - UI.Debug.Overlay.FilterName [pattern] - Filter by name
 */
class PROJECTUI_API SProjectUIDebugOverlay : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SProjectUIDebugOverlay) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// SWidget interface
	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

	// Settings control
	void SetSettings(const FProjectUIDebugOverlaySettings& InSettings) { Settings = InSettings; }
	FProjectUIDebugOverlaySettings& GetSettings() { return Settings; }
	const FProjectUIDebugOverlaySettings& GetSettings() const { return Settings; }

	/** Refresh the cached widget list (call when widgets change) */
	void RefreshWidgetCache();

private:
	/** Collect all widgets from viewport for overlay */
	void CollectWidgets(TArray<TWeakObjectPtr<UWidget>>& OutWidgets) const;

	/** Determine widget state for coloring */
	EProjectDebugWidgetState GetWidgetState(UWidget* Widget) const;

	/** Get color for widget state */
	FLinearColor GetStateColor(EProjectDebugWidgetState State) const;

	/** Check if widget passes current filters */
	bool PassesFilter(UWidget* Widget) const;

	/** Draw a single widget's debug bounds */
	void DrawWidgetBounds(
		UWidget* Widget,
		const FGeometry& AllottedGeometry,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId) const;

	/** Draw label for widget (name, size, etc.) */
	void DrawWidgetLabel(
		UWidget* Widget,
		const FGeometry& WidgetGeometry,
		const FGeometry& AllottedGeometry,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId) const;

	/** Overlay settings */
	FProjectUIDebugOverlaySettings Settings;

	/** Cached font for labels */
	mutable FSlateFontInfo LabelFont;
};
