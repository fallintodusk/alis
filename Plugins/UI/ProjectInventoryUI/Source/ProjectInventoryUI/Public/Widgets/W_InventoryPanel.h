// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Templates/PimplPtr.h"
#include "Widgets/ProjectUserWidget.h"
#include "Interaction/ProjectUIGridHitDetector.h"
#include "Presentation/ProjectUIGridVisualState.h"
#include "Widgets/InventoryPanelGridBuilder.h"
#include "Interaction/ProjectUIGridDragDropController.h"
#include "Widgets/InventoryPanelState.h"
#include "Widgets/InventoryPanelTextUpdater.h"
#include "MVVM/InventoryDragEvent.h"
#include "Slice20SabotageToggle.h"
#include "Overlay/ProjectUIPopupPresenter.h"
#include "Overlay/ProjectUIHoverTooltipPresenter.h"
#include "Presentation/ProjectUIActionDescriptor.h"
#include "Interfaces/IInventoryReadOnly.h"
#include "W_InventoryPanel.generated.h"

class UInventoryViewModel;
class UButton;
class UTextBlock;
class UUniformGridPanel;
class UHorizontalBox;
class UVerticalBox;
class USizeBox;
class UProjectUIThemeData;
class UProjectGridCell;
class UBorder;
class UCanvasPanel;
class UW_ItemContextMenu;
class UW_ItemTooltip;
class UW_InventoryEquipSlotDropTarget;
class UInventoryDragDropOperation;

// Internal scaffolding types kept in Private/Widgets/ - forward declared
// here so the pimpl'd registry member and grid-context helpers don't leak
// implementation details through this public header.
struct FInventoryPanelGridContext;
struct FInventoryPanelSurfaceRegistry;

// Friend-targets for the stateless grid-layout helpers that bind the
// panel's private UFUNCTION handlers via CreateUObject / AddUObject,
// and for the const-only tooltip-anchor resolver that reads cell
// arrays + cached grid dims to find the drop-target visual.
// Full declarations live in Private/Widgets/InventoryPanelGridLayout.h.
struct FInventoryEntryView;
namespace InventoryPanelGridLayout
{
    void RebuildHandGrids(FInventoryPanelGridContext& Ctx);
    void RebuildPocketGrids(FInventoryPanelGridContext& Ctx);
    void RebuildTabs(FInventoryPanelGridContext& Ctx);
    void RebuildEquipSlots(FInventoryPanelGridContext& Ctx);
    bool TryResolveTooltipAnchorViewportPos(const UW_InventoryPanel& Panel, const UWidget* AbsoluteContext, const FInventoryEntryView& Entry, FVector2D& OutViewportPos);
}

DECLARE_LOG_CATEGORY_EXTERN(LogInventoryPanel, Log, All);

/**
 * Runtime state for a single pocket grid (one grid panel + cell arrays).
 *
 * File-scope so TArray<FPocketGridRuntime> can be a UW_InventoryPanel
 * member without dragging the full grid-layout helper header into public
 * scope. Consumed by the stateless helpers in namespace
 * InventoryPanelGridLayout (see Private/Widgets/InventoryPanelGridLayout.h).
 */
struct FPocketGridRuntime
{
    int32 ViewModelPocketIndex = INDEX_NONE;
    FGameplayTag ContainerId;
    int32 GridWidth = 0;
    int32 GridHeight = 0;
    TObjectPtr<UUniformGridPanel> GridPanel = nullptr;
    TArray<TObjectPtr<UTextBlock>> PrimaryWidgets;
    TArray<TObjectPtr<UTextBlock>> QuantityWidgets;
    TArray<TObjectPtr<UBorder>> QuantityBadges;
    TArray<TObjectPtr<UProjectGridCell>> CellBorders;
};

/**
 * Inventory panel widget - thin orchestrator for grid-based inventory UI.
 * Delegates work to specialized helper classes for maintainability.
 *
 * GUARDRAIL:
 * - Keep this class as orchestration only.
 * - Do not add new generic UI mechanics here (hit math, popup plumbing, generic drag systems).
 * - If logic is reusable across multiple UI modules, move it to ProjectUI first.
 *
 * VISIBILITY CONTRACT (Slice 19, 2026-04-21):
 * - Root visibility is `Visible`. The root canvas does not anchor to a
 *   Fill area that overlaps the sibling nearby panel, so intercepting
 *   input at the root does not steal events from other widgets.
 * - Per-cell drop targets (`UW_InventoryCellDropTarget`) are children
 *   of the grid hosts and are the smallest semantic drop target for
 *   inventory grid drops.
 * - Per-equip-slot drop targets (`UW_InventoryEquipSlotDropTarget`)
 *   wrap each equip slot's visual tree and are the smallest semantic
 *   drop target for equip drags (Slice 19). The root widget no longer
 *   overrides `NativeOnDragOver` or `NativeOnDrop`.
 */
UCLASS()
class PROJECTINVENTORYUI_API UW_InventoryPanel : public UProjectUserWidget
{
    GENERATED_BODY()

    // The stateless grid-layout helpers in namespace
    // InventoryPanelGridLayout bind UFUNCTION() handlers declared
    // below (HandleLeft/Right/PocketCellMouseDown,
    // HandleContainerTabSelected, HandleEquipSlotClicked, etc.) via
    // CreateUObject/AddUObject during cell construction.  Grant them
    // friendship so they can name these private handlers; state
    // remains on this UClass and the helpers only pass `this` through
    // as a delegate target.
    friend void InventoryPanelGridLayout::RebuildHandGrids(FInventoryPanelGridContext&);
    friend void InventoryPanelGridLayout::RebuildPocketGrids(FInventoryPanelGridContext&);
    friend void InventoryPanelGridLayout::RebuildTabs(FInventoryPanelGridContext&);
    friend void InventoryPanelGridLayout::RebuildEquipSlots(FInventoryPanelGridContext&);

    // Tooltip-anchor resolver is read-only; friending it lets the
    // helper name private cell arrays + cached dims without an
    // 8-accessor public surface. Signature drift breaks compilation
    // immediately, which is the intended safety.
    friend bool InventoryPanelGridLayout::TryResolveTooltipAnchorViewportPos(const UW_InventoryPanel&, const UWidget*, const FInventoryEntryView&, FVector2D&);

    // Surface registry owns drag-host subscription + tag-indexed
    // registration; friend access eliminates the 6 ForRegistry()
    // accessors that only existed to serve this one helper.
    friend struct FInventoryPanelSurfaceRegistry;

public:
    UW_InventoryPanel(const FObjectInitializer& ObjectInitializer);

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void SetInventoryViewModel(UInventoryViewModel* InViewModel);

    FEventReply HandleCellMouseDown(int32 CellIndex, bool bSecondary, const FPointerEvent& MouseEvent);

protected:
    virtual void BindCallbacks() override;
    virtual void OnViewModelChanged_Implementation(UProjectViewModel* OldViewModel, UProjectViewModel* NewViewModel) override;
    virtual void RefreshFromViewModel_Implementation() override;
    virtual void OnThemeChanged_Implementation(UProjectUIThemeData* NewTheme) override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
    virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
    // NativeOnDragOver removed (Slice 17): per-cell UW_InventoryCellDropTarget
    // wrappers now own preview updates. Visual repaint is driven via an
    // OnDragEvent subscription set up in NativeConstruct.
#if SLICE20_SABOTAGE
    // Slice 20 fitness test 2 sabotage - see Public/Slice20SabotageToggle.h.
    // UW_InventoryPanel does NOT implement IInventoryDropTarget, so
    // declaring a drag handler override here is exactly what the
    // OnlyDropTargetWidgetsOverrideDragHandlers fitness test must flag.
    virtual bool NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
#endif
    virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual void NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    // Slice 19: NativeOnDrop removed. Equip-slot drops are owned by
    // UW_InventoryEquipSlotDropTarget wrappers (smallest semantic drop
    // target). Grid / hand / pocket drops remain on the Slice 17
    // UW_InventoryCellDropTarget wrappers. The root widget no longer
    // participates in drag-drop routing.

private:
    /**
     * Build an FInventoryPanelGridContext that aliases this panel's
     * widget-tree pointers, cell arrays, cached dims, and VM.  Used
     * exclusively by the Rebuild + ResetRuntimeWidgetState forwarders
     * that delegate into namespace InventoryPanelGridLayout.  Nothing
     * is copied; the context points at this panel's UPROPERTY arrays.
     */
    FInventoryPanelGridContext BuildGridContext();

    void ResetRuntimeWidgetState();

    // Build methods - delegate to GridBuilder
    void RebuildGrids();
    void RebuildHandGrids();
    void RebuildPocketGrids();
    void RebuildTabs();
    void RebuildEquipSlots();

    /**
     * Resize a grid host SizeBox to match (GridWidth, GridHeight) cells at CachedCellSize.
     * Keeps every grid in the panel on the single sizing formula owned by
     * FInventoryPanelGridBuilder::ComputeGridHostPixelSize.
     */
    void ApplyGridHostSize(USizeBox* Host, int32 GridWidth, int32 GridHeight) const;

    // Update methods - delegate to helpers
    void UpdateAllVisuals();
    void RefreshAllText();
    void HandleViewModelPropertyChanged(FName PropertyName);
    void ReconcilePanelStateWithViewModel();
    void UpdateResponsiveLayout();

    // Click handlers (UFUNCTION required for delegates)
    UFUNCTION() void HandleUseClicked();
    UFUNCTION() void HandleDropClicked();
    UFUNCTION() void HandleEquipClicked();
    UFUNCTION() void HandleQtyDownClicked();
    UFUNCTION() void HandleQtyUpClicked();
    UFUNCTION() void HandleRotateClicked();

    void HandleLeftHandCellClicked(int32 CellIndex);
    void HandleRightHandCellClicked(int32 CellIndex);
    FEventReply HandleLeftHandCellMouseDown(int32 CellIndex, bool bSecondary, const FPointerEvent& MouseEvent);
    FEventReply HandleRightHandCellMouseDown(int32 CellIndex, bool bSecondary, const FPointerEvent& MouseEvent);
    void HandleLeftHandCellContextRequested(int32 CellIndex);
    void HandleRightHandCellContextRequested(int32 CellIndex);
    void HandlePocketCellClicked(int32 EncodedCellIndex);
    void HandlePocketCellContextRequested(int32 EncodedCellIndex);
    void HandleEquipSlotClicked(int32 SlotIndex);
    void HandleEquipSlotRightClicked(int32 SlotIndex);
    void HandleContainerTabSelected(int32 TabIndex);
    void HandleSecondaryContainerTabSelected(int32 TabIndex);
    void HandleInventoryError(const FText& ErrorMessage);
    static int32 EncodePocketCellIndex(int32 PocketIndex, int32 CellIndex);
    static bool DecodePocketCellIndex(int32 EncodedCellIndex, int32& OutPocketIndex, int32& OutCellIndex);
    FEventReply HandlePocketCellMouseDown(int32 EncodedCellIndex, bool bSecondary, const FPointerEvent& MouseEvent);
    bool ResolveHandDropTargetAtScreenPos(const FVector2D& ScreenPos, FGameplayTag& OutContainerId, FIntPoint& OutGridPos) const;
    bool IsDropOccupantAllowed(const UInventoryDragDropOperation* DragOp, bool bSecondary, int32 CellIndex, int32 OccupantId) const;
    bool TryResolveTooltipAnchorViewportPos(const FInventoryEntryView& Entry, FVector2D& OutViewportPos) const;
    bool TryResolveWidgetTopCenterViewportPos(const UWidget* Widget, FVector2D& OutViewportPos) const;
    void ShowInteractionError(const FText& ErrorMessage);

    // Subsystem-registered drop surfaces.  FInventoryPanelSurfaceRegistry
    // owns the hand/pocket/primary/secondary surface bookkeeping and the
    // OnDragEvent subscription; the panel calls into it at rebuild time
    // (see UW_InventoryPanel::RebuildGrids / RebuildHandGrids / etc.).
    // UpdateAllVisuals reads the cached primary/secondary tag directly
    // from the registry to index the shared preview map.
    class UInventoryUIDragHostSubsystem* ResolveDragHostSubsystem() const;

    /**
     * Re-run a preview tick against the shared subsystem with the current
     * drag payload. Called from the rotate handler so flipping orientation
     * mid-drag updates the preview immediately. NOT called from
     * NativeOnDragOver - that path is owned by the cell-host wrappers.
     */
    bool UpdateDragPreviewFromSubsystem(UInventoryDragDropOperation* DragOp, const FVector2D& ScreenPos);

private:
    // Core references
    UPROPERTY() TObjectPtr<UInventoryViewModel> InventoryVM;
    UPROPERTY() TObjectPtr<UProjectUIThemeData> CurrentTheme;

    // Layout containers (from JSON)
    UPROPERTY() TObjectPtr<USizeBox> BackgroundWidthSizer;
    UPROPERTY() TObjectPtr<UBorder> GridHost;
    UPROPERTY() TObjectPtr<UBorder> GridHostSecondary;
    UPROPERTY() TObjectPtr<UHorizontalBox> ContainerTabs;
    UPROPERTY() TObjectPtr<UHorizontalBox> ContainerTabsSecondary;
    UPROPERTY() TObjectPtr<UVerticalBox> EquipSlotsHost;

    // Grid parent containers (collapsed when no storage containers).
    // Grid SizeBoxes are sized at runtime from FInventoryPanelGridBuilder::ComputeGridHostPixelSize
    // so every grid in the panel renders cells at the same pixel size.
    // Nearby-container surface lives in UW_NearbyContainerPanel - not part of this widget any more.
    UPROPERTY() TObjectPtr<UWidget> GridRow;
    UPROPERTY() TObjectPtr<USizeBox> GridSizeBoxPrimary;
    UPROPERTY() TObjectPtr<USizeBox> GridSizeBoxSecondary;

    // Placeholder shown when no storage containers exist
    UPROPERTY() TObjectPtr<UWidget> EmptyStoragePlaceholder;

    // Hand grid containers (always visible, separate from tabs)
    UPROPERTY() TObjectPtr<UBorder> LeftHandGridHost;
    UPROPERTY() TObjectPtr<UBorder> RightHandGridHost;
    UPROPERTY() TObjectPtr<USizeBox> LeftHandGridSizeBox;
    UPROPERTY() TObjectPtr<USizeBox> RightHandGridSizeBox;
    UPROPERTY() TObjectPtr<UHorizontalBox> PocketGridsHost;

    // Built grid panels
    UPROPERTY() TObjectPtr<UUniformGridPanel> GridPanel;
    UPROPERTY() TObjectPtr<UUniformGridPanel> GridPanelSecondary;

    // Cell widgets (populated by GridBuilder)
    UPROPERTY() TArray<TObjectPtr<UProjectGridCell>> ContainerTabCells;
    UPROPERTY() TArray<TObjectPtr<UProjectGridCell>> SecondaryContainerTabCells;
    UPROPERTY() TArray<TObjectPtr<UTextBlock>> CellPrimaryWidgets;
    UPROPERTY() TArray<TObjectPtr<UTextBlock>> CellQuantityWidgets;
    UPROPERTY() TArray<TObjectPtr<UBorder>> CellQuantityBadges;
    UPROPERTY() TArray<TObjectPtr<UProjectGridCell>> CellBorders;
    UPROPERTY() TArray<TObjectPtr<UTextBlock>> SecondaryCellPrimaryWidgets;
    UPROPERTY() TArray<TObjectPtr<UTextBlock>> SecondaryCellQuantityWidgets;
    UPROPERTY() TArray<TObjectPtr<UBorder>> SecondaryCellQuantityBadges;
    UPROPERTY() TArray<TObjectPtr<UProjectGridCell>> SecondaryCellBorders;
    UPROPERTY() TArray<TObjectPtr<UProjectGridCell>> EquipSlotCells;

    // Slice 19: per-equip-slot drop target wrappers. 1:1 with EquipSlotCells.
    // Owned via the widget tree; the UPROPERTY array keeps them alive across
    // rebuilds without rooting them for longer than the panel's own lifetime.
    UPROPERTY() TArray<TObjectPtr<UW_InventoryEquipSlotDropTarget>> EquipSlotDropTargets;

    // Hand grid cells (2x2 each, always visible)
    UPROPERTY() TArray<TObjectPtr<UTextBlock>> LeftHandCellPrimaryWidgets;
    UPROPERTY() TArray<TObjectPtr<UTextBlock>> LeftHandCellQuantityWidgets;
    UPROPERTY() TArray<TObjectPtr<UBorder>> LeftHandCellQuantityBadges;
    UPROPERTY() TArray<TObjectPtr<UTextBlock>> RightHandCellPrimaryWidgets;
    UPROPERTY() TArray<TObjectPtr<UTextBlock>> RightHandCellQuantityWidgets;
    UPROPERTY() TArray<TObjectPtr<UBorder>> RightHandCellQuantityBadges;
    UPROPERTY() TArray<TObjectPtr<UProjectGridCell>> LeftHandCells;
    UPROPERTY() TArray<TObjectPtr<UProjectGridCell>> RightHandCells;
    UPROPERTY() TObjectPtr<UUniformGridPanel> LeftHandGridPanel;
    UPROPERTY() TObjectPtr<UUniformGridPanel> RightHandGridPanel;
    bool bHandGridsBuilt = false;

    // FPocketGridRuntime is a file-scope struct declared above (pre-class).
    // Kept public-header scope so this TArray<...> member can be named
    // without pulling the grid-layout helper header into public scope.
    // Its nested TObjectPtr<...> members stay rooted with the panel
    // lifetime.
    TArray<FPocketGridRuntime> PocketGridRuntime;

    int32 PendingDragInstanceId = INDEX_NONE;
    FVector2D LastDragScreenPos = FVector2D::ZeroVector;
    bool bHasLastDragScreenPos = false;

    // Info display widgets
    UPROPERTY() TObjectPtr<UTextBlock> WeightText;
    UPROPERTY() TObjectPtr<UTextBlock> VolumeText;
    UPROPERTY() TObjectPtr<UTextBlock> ItemCountText;
    UPROPERTY() TObjectPtr<UTextBlock> SelectionText;
    UPROPERTY() TObjectPtr<UTextBlock> SelectionStatsText;
    UPROPERTY() TObjectPtr<UTextBlock> ItemIcon;
    UPROPERTY() TObjectPtr<UTextBlock> ItemDetailsText;
    UPROPERTY() TObjectPtr<UTextBlock> StatusText;
    UPROPERTY() TObjectPtr<UTextBlock> RotateStateText;
    UPROPERTY() TObjectPtr<UTextBlock> QtyValueText;
    UPROPERTY() TObjectPtr<UTextBlock> NearbyTitleText;
    UPROPERTY() TObjectPtr<UTextBlock> NearbyStatsText;

    // Context menu adapters (popup mechanics in ProjectUI presenter, commands in ViewModel)
    UPROPERTY() TObjectPtr<UCanvasPanel> RootCanvas;
    UFUNCTION() void HandleContextMenuUse(int32 InstanceId);
    UFUNCTION() void HandleContextMenuEquip(int32 InstanceId);
    UFUNCTION() void HandleContextMenuDrop(int32 InstanceId);
    UFUNCTION() void HandleContextMenuSplit(int32 InstanceId);
    UFUNCTION() void HandleContextMenuClosed();
    UFUNCTION() void HandleClickCatcherClicked();
    void ShowContextMenuForCell(int32 CellIndex, bool bSecondary, const FVector2D& ScreenPos);
    void ShowContextMenuForEntry(const FInventoryEntryView& Entry, const TArray<FProjectUIActionDescriptor>& ActionDescriptors, const FVector2D& AbsolutePos);
    void HideContextMenu();
    void UpdateTooltipForHover(const FVector2D& MouseViewportPos);
    void HideTooltip();
    FVector2D ScreenToViewportPos(const FVector2D& ScreenPos) const;

    // Cached grid dimensions
    int32 CachedGridWidth = 0;
    int32 CachedGridHeight = 0;
    int32 CachedGridWidthSecondary = 0;
    int32 CachedGridHeightSecondary = 0;
    float CachedCellSize = 64.f;

    // Helper classes (SOLID - single responsibility)
    // IMPORTANT:
    // - Keep helpers small (<150 lines each). Split if they grow.
    // - New non-domain helpers belong in ProjectUI, not ProjectInventoryUI.
    FInventoryPanelState PanelState;
    FProjectUIGridHitDetector HitDetector;
    FProjectUIGridVisualState VisualState;
    FInventoryPanelGridBuilder GridBuilder;
    FInventoryPanelTextUpdater TextUpdater;
    FProjectUIPopupPresenter ContextMenuPresenter;
    FProjectUIHoverTooltipPresenter TooltipPresenter;

    // Drag-host surface registration and the OnDragEvent subscription
    // live here.  All surface tags, the pocket-tag list, and the
    // hand-registered flag are owned by this struct; the panel keeps
    // zero mirrored state.  Pimpl'd via TPimplPtr so the registry
    // header can live in Private/Widgets/ without leaking through this
    // public header.  TPimplPtr uses an indirect deleter function
    // pointer so the struct can stay forward-declared here.
    TPimplPtr<FInventoryPanelSurfaceRegistry> SurfaceRegistry;

    // Cache tooltip content updates while hovering to avoid redundant widget updates.
    uint32 CachedTooltipContentHash = 0;
    bool bHasCachedTooltipContentHash = false;
};
