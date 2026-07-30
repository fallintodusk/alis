// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "GameplayTagContainer.h"
#include "MVVM/InventoryCellVisualState.h"
#include "Widgets/ProjectGridCell.h"

class UUserWidget;
class UInventoryViewModel;
class UProjectGridCell;
class UW_InventoryCellDropTarget;
class UW_InventoryEquipSlotDropTarget;
class UBorder;
class UTextBlock;
class UUniformGridPanel;
class UHorizontalBox;
class UVerticalBox;
class UWidgetTree;
class UProjectUIThemeData;

/**
 * Handles all grid and UI element construction for the inventory panel.
 * Single responsibility: build and populate widget hierarchies.
 */
class PROJECTINVENTORYUI_API FInventoryPanelGridBuilder
{
public:
	FInventoryPanelGridBuilder() = default;

	/** Initialize with required context. Owner is any UUserWidget that
	 *  wants grids built into its widget tree (UW_InventoryPanel or
	 *  UW_NearbyContainerPanel today). */
	void Initialize(UUserWidget* InOwner, UWidgetTree* InWidgetTree);

	/** Update theme reference for styling */
	void SetTheme(UProjectUIThemeData* InTheme) { Theme = InTheme; }

	/** Set cell size for grid layout */
	void SetCellSize(float InCellSize) { CellSize = InCellSize; }

	/**
	 * Optional handler bound to every UProjectGridCell created by BuildGrid.
	 * Callers that need per-cell mouse-down routing set this before calling
	 * BuildGrid; callers that do not (or wire their own events later) leave
	 * it unset. Keeping this out of the builder's constructor avoids coupling
	 * the builder to any specific widget class.
	 */
	void SetCellMouseDownHandler(UProjectGridCell::FOnGridCellMouseDown InHandler) { CellMouseDownHandler = MoveTemp(InHandler); }

	/** Overhead per side reserved for grid line + internal cell padding (drives font sizing). */
	static float GetCellFrameOverhead();

	/**
	 * Pixel size required for a grid host wrapping a UniformGrid of W x H cells
	 * at the given CellSize. Encodes the single formula used by every grid
	 * (hands, storage, nearby) so every cell in the panel renders identically.
	 */
	static FIntPoint ComputeGridHostPixelSize(int32 GridWidth, int32 GridHeight, float CellSize);

	/**
	 * Build container tabs for primary or secondary grid
	 */
	void BuildContainerTabs(
		UHorizontalBox* TabsHost,
		const TArray<FText>& Labels,
		TArray<TObjectPtr<UProjectGridCell>>& OutTabCells,
		bool bIsSecondary);

	/**
	 * Build a grid panel with cells.
	 *
	 * Each cell is wrapped in a UW_InventoryCellDropTarget so the
	 * wrapper - the smallest-semantic drop target (Slice 17) - receives
	 * NativeOnDragOver / NativeOnDrop. The visual UProjectGridCell stays
	 * unchanged and is hosted inside the wrapper as its root content.
	 *
	 * Layout hierarchy produced per cell (zero pixel shift vs pre-Slice-17):
	 *   UniformGridSlot -> SizeBox -> UW_InventoryCellDropTarget -> UProjectGridCell
	 *
	 * SurfaceTag is the gameplay tag the grid will be registered under in
	 * UInventoryUIDragHostSubsystem. The wrapper uses it to stamp drop
	 * events with a stable surface identity (diagnostics / logging).
	 *
	 * OutCellHosts mirrors OutCellBorders 1:1; callers usually only need
	 * OutCellBorders (visual references). The hosts are kept alive via
	 * the UniformGridPanel subtree and the UPROPERTY references the
	 * caller stores, so the out-array is optional for callers that do
	 * not need to assert the wrapper chain in tests.
	 *
	 * @return The created UniformGridPanel (caller takes ownership)
	 */
	UUniformGridPanel* BuildGrid(
		int32 GridWidth,
		int32 GridHeight,
		const FGameplayTag& SurfaceTag,
		TArray<TObjectPtr<UTextBlock>>& OutPrimaryCellWidgets,
		TArray<TObjectPtr<UTextBlock>>& OutQuantityCellWidgets,
		TArray<TObjectPtr<UBorder>>& OutQuantityBadgeWidgets,
		TArray<TObjectPtr<UProjectGridCell>>& OutCellBorders,
		TArray<TObjectPtr<UW_InventoryCellDropTarget>>& OutCellHosts,
		bool bIsSecondary);

	/**
	 * Legacy overload kept for callers that do not need the wrapper
	 * reference array. Internally forwards to the primary overload with
	 * a local wrapper array.
	 */
	UUniformGridPanel* BuildGrid(
		int32 GridWidth,
		int32 GridHeight,
		const FGameplayTag& SurfaceTag,
		TArray<TObjectPtr<UTextBlock>>& OutPrimaryCellWidgets,
		TArray<TObjectPtr<UTextBlock>>& OutQuantityCellWidgets,
		TArray<TObjectPtr<UBorder>>& OutQuantityBadgeWidgets,
		TArray<TObjectPtr<UProjectGridCell>>& OutCellBorders,
		bool bIsSecondary);

	/**
	 * Build equipment slot cells in body-position layout using nested boxes.
	 *
	 * Slice 19: each slot is wrapped in a UW_InventoryEquipSlotDropTarget
	 * (the smallest-semantic drop target for equip). The wrapper holds
	 * the slot's gameplay tag identity (e.g. Item.EquipmentSlot.Chest)
	 * and owns NativeOnDragOver / NativeOnDrop; it forwards to the drag
	 * host subsystem's equip methods so equip drops emit the same
	 * FInventoryDragEvent sequence container drops do.
	 *
	 * OutSlotCells mirrors the visible UProjectGridCell array (same size
	 * as equip slot count). OutSlotDropTargets mirrors it 1:1 and is the
	 * array callers use to manage slot-wrapper lifetime (e.g. passing
	 * identity to a new builder call). Callers that only need the visual
	 * cells can pass a dummy TArray for the drop-target out-param.
	 */
	void BuildEquipSlots(
		UVerticalBox* SlotsHost,
		UInventoryViewModel* ViewModel,
		TArray<TObjectPtr<UProjectGridCell>>& OutSlotCells,
		TArray<TObjectPtr<UW_InventoryEquipSlotDropTarget>>& OutSlotDropTargets);

	/** Legacy overload for callers that do not need wrapper references. */
	void BuildEquipSlots(
		UVerticalBox* SlotsHost,
		UInventoryViewModel* ViewModel,
		TArray<TObjectPtr<UProjectGridCell>>& OutSlotCells);

	/**
	 * Update text in grid cells (uses cached fonts from Initialize)
	 */
	void UpdateGridVisuals(
		const TArray<FInventoryCellVisualState>& CellVisuals,
		TArray<TObjectPtr<UTextBlock>>& PrimaryCellWidgets,
		TArray<TObjectPtr<UTextBlock>>& QuantityCellWidgets,
		TArray<TObjectPtr<UBorder>>& QuantityBadgeWidgets);

private:
	UUserWidget* Owner = nullptr;
	UWidgetTree* WidgetTree = nullptr;
	UProjectUIThemeData* Theme = nullptr;
	float CellSize = 64.f;
	UProjectGridCell::FOnGridCellMouseDown CellMouseDownHandler;
	void EnsureFontsResolved();

	FSlateFontInfo CachedIconFont;
	FSlateFontInfo CachedTextFont;
	bool bFontsResolved = false;
};
