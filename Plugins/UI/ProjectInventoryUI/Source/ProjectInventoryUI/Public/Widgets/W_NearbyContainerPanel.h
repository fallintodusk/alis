// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/ProjectUserWidget.h"
#include "Widgets/InventoryPanelGridBuilder.h"
#include "W_NearbyContainerPanel.generated.h"

class UInventoryViewModel;
class UBorder;
class UButton;
class UProjectGridCell;
class USizeBox;
class UTextBlock;
class UUniformGridPanel;
class UProjectUIThemeData;

DECLARE_LOG_CATEGORY_EXTERN(LogNearbyContainerPanel, Log, All);

/**
 * Right-anchored world-container surface for inventory sessions.
 *
 * Bound to the same UInventoryViewModel as W_InventoryPanel and shows
 * the nearby world storage grid independently of the main panel. Cell
 * sizing is driven by FInventoryUISettings::Get(); drag/drop is routed
 * through UInventoryUIDragHostSubsystem with surface tag
 * Item.Container.WorldStorage; drops are dispatched through
 * FInventoryDropRouter so this widget never picks a VM command inline.
 *
 * VISIBILITY CONTRACT (Slice 17, 2026-04-21; hardened 2026-04-24):
 * - The root CanvasPanel in NearbyContainerPanel.json anchors Fill, so
 *   a plain `Visible` root would intercept drag/drop events anywhere
 *   in the viewport and prevent them from reaching the sibling
 *   W_InventoryPanel (which is NOT Fill-anchored). Use
 *   `SelfHitTestInvisible` on the root so the canvas itself is
 *   skipped during hit-testing; the inner NearbyBackground / grid
 *   cells / TakeAllButton stay `Visible` and intercept their own
 *   clicks within their bounds.
 * - Cell-level hit-testing is provided by `UW_InventoryCellDropTarget`
 *   children built by FInventoryPanelGridBuilder. Those wrappers are
 *   Visible (the default). Flipping either the root to Visible OR the
 *   cell hosts away from Visible would break drop routing; the
 *   framework test `NearbyPanelRootSelfHitTestInvisibleWithVisibleCells`
 *   pins this contract.
 * - Root is HARDENED (2026-04-24): `SetVisibility(Visible)` is coerced
 *   to `SelfHitTestInvisible`, and `SynchronizeProperties` re-asserts
 *   the contract after any factory / layer-host stamping. Fixes the
 *   first-frame race where the UI factory stamped the UMG default
 *   `Visible` into this root before the first VM OnPropertyChanged
 *   could run `UpdateVisibilityFromViewModel`, causing the first drag
 *   from a freshly-opened world container to cancel instead of
 *   reaching W_InventoryPanel cells.
 *
 * Visibility logic: Visible iff ViewModel.GetbPanelVisible() AND
 * ViewModel.GetbHasNearbyContainer(). Otherwise Collapsed (no reserved
 * layout).
 *
 * GUARDRAIL:
 * - Orchestration only. No generic UI mechanics here; reuse ProjectUI
 *   framework helpers (FInventoryPanelGridBuilder, drag host subsystem,
 *   FProjectUIHoverTooltipPresenter).
 * - Never reach into W_InventoryPanel. If you need a shared contract,
 *   put it in ViewModel or a ProjectCore interface.
 * - Slice 17: this widget does NOT override NativeOnDragOver /
 *   NativeOnDrop. Grid drops are owned by UW_InventoryCellDropTarget
 *   wrappers created by the grid builder. NativeOnDragDetected stays
 *   (drag source side) and NativeOnMouseButtonDown stays (fallthrough).
 */
UCLASS()
class PROJECTINVENTORYUI_API UW_NearbyContainerPanel : public UProjectUserWidget
{
    GENERATED_BODY()

public:
    UW_NearbyContainerPanel(const FObjectInitializer& ObjectInitializer);

    UFUNCTION(BlueprintCallable, Category = "Inventory|Nearby")
    void SetInventoryViewModel(UInventoryViewModel* InViewModel);

    // Visibility-contract guards. Public to match UWidget's parent
    // access (SetVisibility is public on UWidget; SynchronizeProperties
    // is public on UWidget in UE 5.7). See class comment VISIBILITY
    // CONTRACT.
    // SetVisibility: coerces `Visible` to `SelfHitTestInvisible` at the
    //     single public entrypoint so any caller (BP, factory, layer
    //     host, internal code) obeys the root-hit-test rule.
    // SynchronizeProperties: re-asserts the rule after UMG re-applies
    //     reflected properties (e.g. after AddToViewport stamping).
    virtual void SetVisibility(ESlateVisibility InVisibility) override;
    virtual void SynchronizeProperties() override;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void OnViewModelChanged_Implementation(UProjectViewModel* OldViewModel, UProjectViewModel* NewViewModel) override;
    virtual void RefreshFromViewModel_Implementation() override;
    virtual void OnThemeChanged_Implementation(UProjectUIThemeData* NewTheme) override;

    // UMG drag-drop overrides: initiate a drag from a nearby cell
    // (NativeOnDragDetected). Drop-side handlers (NativeOnDragOver /
    // NativeOnDrop) are NOT overridden here - Slice 17 moved them onto
    // UW_InventoryCellDropTarget, the per-cell wrapper built by
    // FInventoryPanelGridBuilder.
    virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
    virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual void NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

    // UMG requires a widget to return a reply with DetectDragIfPressed
    // from NativeOnMouseButtonDown to get NativeOnDragDetected fired.
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
    void RebuildGrid();
    void UpdateVisibilityFromViewModel();
    void UpdateTextAndControls();
    void ApplyGridHostSize(int32 GridWidth, int32 GridHeight) const;

    /**
     * Per-cell mouse-down handler. Mirrors the main panel pattern: each
     * cell binds this via FInventoryPanelGridBuilder::SetCellMouseDownHandler
     * so cell clicks are intercepted at the CELL level, not the user-widget
     * level. This lets the user-widget root be SelfHitTestInvisible (so
     * empty area passes events through to the sibling main panel) without
     * losing drag-from-nearby functionality.
     */
    FEventReply HandleNearbyCellMouseDown(int32 CellIndex, bool bSecondary, const FPointerEvent& MouseEvent);

    UFUNCTION() void HandleTakeAllClicked();

    /**
     * Dedicated delegate target for UInventoryViewModel::OnPropertyChanged.
     *
     * The base class's HandleViewModelPropertyChanged is bound to a
     * different (base) view-model delegate. We use a distinct UFUNCTION
     * name here so dynamic-delegate dispatch through the UClass
     * reflection table always hits this derived implementation without
     * any override/hide ambiguity.
     */
    UFUNCTION() void HandleInventoryVMPropertyChanged(FName PropertyName);

    UPROPERTY() TObjectPtr<UInventoryViewModel> InventoryVM;

    // Cached layout refs (bound in NativeConstruct via FProjectUIWidgetBinder).
    UPROPERTY() TObjectPtr<UBorder> NearbyGridHost;
    UPROPERTY() TObjectPtr<USizeBox> NearbyGridSizeBox;
    UPROPERTY() TObjectPtr<UTextBlock> NearbyTitleText;
    UPROPERTY() TObjectPtr<UTextBlock> NearbyStatsText;
    UPROPERTY() TObjectPtr<UTextBlock> NearbyHintText;
    UPROPERTY() TObjectPtr<UButton> TakeAllButton;

    // Built grid panel (owned by NearbyGridHost).
    UPROPERTY() TObjectPtr<UUniformGridPanel> GridPanel;
    UPROPERTY() TArray<TObjectPtr<UTextBlock>> CellPrimaryWidgets;
    UPROPERTY() TArray<TObjectPtr<UTextBlock>> CellQuantityWidgets;
    UPROPERTY() TArray<TObjectPtr<UBorder>> CellQuantityBadges;
    UPROPERTY() TArray<TObjectPtr<UProjectGridCell>> CellBorders;

    // Grid builder owned locally, seeded from FInventoryUISettings::Get().
    FInventoryPanelGridBuilder GridBuilder;

    // Cached dims to avoid redundant rebuilds.
    int32 CachedGridWidth = 0;
    int32 CachedGridHeight = 0;

    // Cached cell size read once at NativeConstruct from the shared SOT.
    float CachedCellSize = 64.f;

    // True once we have registered our surface with the drag host subsystem.
    bool bSurfaceRegistered = false;

    // Cell index captured at mouse-down time so NativeOnDragDetected knows
    // which cell the user pressed without a screen-pos hit-test (which
    // would have to run again at drag-detect time). INDEX_NONE means no
    // pending press.
    int32 PendingDragCellIndex = INDEX_NONE;
};
