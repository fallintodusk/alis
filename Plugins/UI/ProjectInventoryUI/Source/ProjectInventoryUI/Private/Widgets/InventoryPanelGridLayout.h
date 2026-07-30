// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/ObjectPtr.h"

// FPocketGridRuntime lives in "Widgets/W_InventoryPanel.h" (file-scope,
// pre-UCLASS). The panel holds TArray<FPocketGridRuntime> directly; the
// helpers in this header mutate that array through the grid context.
#include "Widgets/W_InventoryPanel.h"

class UBorder;
class UHorizontalBox;
class USizeBox;
class UTextBlock;
class UUniformGridPanel;
class UVerticalBox;
class UWidget;
class UWidgetTree;
class UInventoryDragDropOperation;
class UInventoryViewModel;
class UProjectGridCell;
class UProjectUIThemeData;
class UW_InventoryEquipSlotDropTarget;
class FInventoryPanelGridBuilder;
struct FInventoryEntryView;

/**
 * Bundled references/pointers to the parent panel's widget tree, cell
 * arrays, cached dims, and VM.  Constructed on the stack by
 * UW_InventoryPanel forwarders (which have private access) and passed
 * by reference to the stateless helpers.
 *
 * All state remains on the parent (UPROPERTY, GC-rooted).  Helpers
 * only populate arrays by reference and assign to the exposed
 * TObjectPtr<...> fields that live on the parent.
 */
struct FInventoryPanelGridContext
{
    // Owner - required for CreateUObject delegate bindings and for
    // accessing helpers (FInventoryPanelGridBuilder / theme) that live
    // on the parent.  Helpers never call UClass-private methods
    // through this pointer; they only use it as a delegate target.
    UW_InventoryPanel* Panel = nullptr;

    // Direct references to parent-owned helpers we drive.
    FInventoryPanelGridBuilder* GridBuilder = nullptr;
    UInventoryViewModel* InventoryVM = nullptr;
    UProjectUIThemeData* CurrentTheme = nullptr;
    UWidgetTree* WidgetTree = nullptr;

    // Layout hosts.
    TObjectPtr<UBorder>* GridHost = nullptr;
    TObjectPtr<UBorder>* GridHostSecondary = nullptr;
    TObjectPtr<UHorizontalBox>* ContainerTabs = nullptr;
    TObjectPtr<UHorizontalBox>* ContainerTabsSecondary = nullptr;
    TObjectPtr<UVerticalBox>* EquipSlotsHost = nullptr;
    TObjectPtr<UWidget>* GridRow = nullptr;
    TObjectPtr<USizeBox>* GridSizeBoxPrimary = nullptr;
    TObjectPtr<USizeBox>* GridSizeBoxSecondary = nullptr;
    TObjectPtr<UWidget>* EmptyStoragePlaceholder = nullptr;
    TObjectPtr<UBorder>* LeftHandGridHost = nullptr;
    TObjectPtr<UBorder>* RightHandGridHost = nullptr;
    TObjectPtr<USizeBox>* LeftHandGridSizeBox = nullptr;
    TObjectPtr<USizeBox>* RightHandGridSizeBox = nullptr;
    TObjectPtr<UHorizontalBox>* PocketGridsHost = nullptr;

    // Built grids.
    TObjectPtr<UUniformGridPanel>* GridPanel = nullptr;
    TObjectPtr<UUniformGridPanel>* GridPanelSecondary = nullptr;
    TObjectPtr<UUniformGridPanel>* LeftHandGridPanel = nullptr;
    TObjectPtr<UUniformGridPanel>* RightHandGridPanel = nullptr;

    // Cell widget arrays.
    TArray<TObjectPtr<UProjectGridCell>>* ContainerTabCells = nullptr;
    TArray<TObjectPtr<UProjectGridCell>>* SecondaryContainerTabCells = nullptr;
    TArray<TObjectPtr<UTextBlock>>* CellPrimaryWidgets = nullptr;
    TArray<TObjectPtr<UTextBlock>>* CellQuantityWidgets = nullptr;
    TArray<TObjectPtr<UBorder>>* CellQuantityBadges = nullptr;
    TArray<TObjectPtr<UProjectGridCell>>* CellBorders = nullptr;
    TArray<TObjectPtr<UTextBlock>>* SecondaryCellPrimaryWidgets = nullptr;
    TArray<TObjectPtr<UTextBlock>>* SecondaryCellQuantityWidgets = nullptr;
    TArray<TObjectPtr<UBorder>>* SecondaryCellQuantityBadges = nullptr;
    TArray<TObjectPtr<UProjectGridCell>>* SecondaryCellBorders = nullptr;
    TArray<TObjectPtr<UProjectGridCell>>* EquipSlotCells = nullptr;
    TArray<TObjectPtr<UW_InventoryEquipSlotDropTarget>>* EquipSlotDropTargets = nullptr;
    TArray<TObjectPtr<UTextBlock>>* LeftHandCellPrimaryWidgets = nullptr;
    TArray<TObjectPtr<UTextBlock>>* LeftHandCellQuantityWidgets = nullptr;
    TArray<TObjectPtr<UBorder>>* LeftHandCellQuantityBadges = nullptr;
    TArray<TObjectPtr<UTextBlock>>* RightHandCellPrimaryWidgets = nullptr;
    TArray<TObjectPtr<UTextBlock>>* RightHandCellQuantityWidgets = nullptr;
    TArray<TObjectPtr<UBorder>>* RightHandCellQuantityBadges = nullptr;
    TArray<TObjectPtr<UProjectGridCell>>* LeftHandCells = nullptr;
    TArray<TObjectPtr<UProjectGridCell>>* RightHandCells = nullptr;
    TArray<FPocketGridRuntime>* PocketGridRuntime = nullptr;

    // Cached grid dimensions (mutated by RebuildGrids / ResetRuntimeWidgetState).
    int32* CachedGridWidth = nullptr;
    int32* CachedGridHeight = nullptr;
    int32* CachedGridWidthSecondary = nullptr;
    int32* CachedGridHeightSecondary = nullptr;
    float CachedCellSize = 64.f;

    // Hand grid construction flag.
    bool* bHandGridsBuilt = nullptr;
};

/**
 * Stateless, free-function grid-layout helpers extracted from
 * UW_InventoryPanel.cpp (Phase 1.5 File 1).  These never own state;
 * they mutate the parent's UPROPERTY arrays through
 * FInventoryPanelGridContext.
 *
 * No back-references to UW_InventoryPanel beyond the delegate-target
 * pointer already in the context; helpers do not call arbitrary
 * methods on the panel.
 */
namespace InventoryPanelGridLayout
{
    /**
     * Encode (PocketIndex, CellIndex) into a single int32 for use as a
     * UProjectGridCell::CellIndex.  Returns INDEX_NONE if either
     * component is out of range.  Top 16 bits = pocket index, bottom
     * 16 bits = cell index.
     */
    int32 EncodePocketCellIndex(int32 PocketIndex, int32 CellIndex);

    /** Reverse of EncodePocketCellIndex. */
    bool DecodePocketCellIndex(int32 EncodedCellIndex, int32& OutPocketIndex, int32& OutCellIndex);

    /**
     * Resize a grid-host SizeBox to match (GridWidth, GridHeight) cells
     * at CachedCellSize, using the single formula owned by
     * FInventoryPanelGridBuilder::ComputeGridHostPixelSize.
     */
    void ApplyGridHostSize(USizeBox* Host, int32 GridWidth, int32 GridHeight, float CellSize);

    /** Clear every runtime grid/cell widget array on the parent. */
    void ResetRuntimeWidgetState(FInventoryPanelGridContext& Ctx);

    /** (Re)build the main + secondary storage grids. */
    void RebuildGrids(FInventoryPanelGridContext& Ctx);

    /** (Re)build the 2x2 left/right hand grids (one-time until reset). */
    void RebuildHandGrids(FInventoryPanelGridContext& Ctx);

    /** (Re)build the pocket grid column row. */
    void RebuildPocketGrids(FInventoryPanelGridContext& Ctx);

    /** (Re)build the primary + secondary tab strips. */
    void RebuildTabs(FInventoryPanelGridContext& Ctx);

    /** (Re)build the equip-slots column, recycling wrapper drop targets. */
    void RebuildEquipSlots(FInventoryPanelGridContext& Ctx);

    /**
     * Viewport-space top-center position of `Widget` for tooltip
     * anchor placement.  Returns false for null or zero-size widgets.
     */
    bool TryResolveWidgetTopCenterViewportPos(const UWidget* Widget, const UWidget* AbsoluteContext, FVector2D& OutViewportPos);

    /**
     * Resolve a viewport-space anchor position (top-center of the
     * cell widget backing `Entry.GridPos`) across primary/secondary/
     * hand/pocket grids.  Returns false when no matching cell is
     * resolvable.
     *
     * Read-only: narrower than the full FInventoryPanelGridContext
     * (avoids a const_cast at the call site).  Replaces the previous
     * overload taking the mutable context.
     */
    bool TryResolveTooltipAnchorViewportPos(const UW_InventoryPanel& Panel, const UWidget* AbsoluteContext, const FInventoryEntryView& Entry, FVector2D& OutViewportPos);

    /**
     * Rotation-aware drag footprint: uses the VM entry's GridSize when
     * available, falling back to DragOp->ItemSize.  Returns (1,1) on
     * null DragOp or non-positive sizes.
     */
    FIntPoint ResolveDragItemFootprint(const UInventoryViewModel* InventoryVM, const UInventoryDragDropOperation* DragOp);
}
