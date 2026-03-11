// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/ProjectUserWidget.h"
#include "Interaction/ProjectUIGridHitDetector.h"
#include "Presentation/ProjectUIGridVisualState.h"
#include "Widgets/InventoryPanelGridBuilder.h"
#include "Interaction/ProjectUIGridDragDropController.h"
#include "Widgets/InventoryPanelState.h"
#include "Widgets/InventoryPanelTextUpdater.h"
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
class UProjectUIThemeData;
class UProjectGridCell;
class UBorder;
class UCanvasPanel;
class UW_ItemContextMenu;
class UW_ItemTooltip;

DECLARE_LOG_CATEGORY_EXTERN(LogInventoryPanel, Log, All);

/**
 * Inventory panel widget - thin orchestrator for grid-based inventory UI.
 * Delegates work to specialized helper classes for maintainability.
 *
 * GUARDRAIL:
 * - Keep this class as orchestration only.
 * - Do not add new generic UI mechanics here (hit math, popup plumbing, generic drag systems).
 * - If logic is reusable across multiple UI modules, move it to ProjectUI first.
 */
UCLASS()
class PROJECTINVENTORYUI_API UW_InventoryPanel : public UProjectUserWidget
{
    GENERATED_BODY()

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
    virtual bool NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

private:
    // Build methods - delegate to GridBuilder
    void RebuildGrids();
    void RebuildHandGrids();
    void RebuildPocketGrids();
    void RebuildTabs();
    void RebuildEquipSlots();

    // Update methods - delegate to helpers
    void UpdateAllVisuals();
    void RefreshAllText();
    void HandleViewModelPropertyChanged(FName PropertyName);

    // Click handlers (UFUNCTION required for delegates)
    UFUNCTION() void HandleUseClicked();
    UFUNCTION() void HandleDropClicked();
    UFUNCTION() void HandleEquipClicked();
    UFUNCTION() void HandleQtyDownClicked();
    UFUNCTION() void HandleQtyUpClicked();
    UFUNCTION() void HandleRotateClicked();

    void HandleLeftHandCellClicked(int32 CellIndex);
    void HandleRightHandCellClicked(int32 CellIndex);
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

private:
    // Core references
    UPROPERTY() TObjectPtr<UInventoryViewModel> InventoryVM;
    UPROPERTY() TObjectPtr<UProjectUIThemeData> CurrentTheme;

    // Layout containers (from JSON)
    UPROPERTY() TObjectPtr<UBorder> GridHost;
    UPROPERTY() TObjectPtr<UBorder> GridHostSecondary;
    UPROPERTY() TObjectPtr<UHorizontalBox> ContainerTabs;
    UPROPERTY() TObjectPtr<UHorizontalBox> ContainerTabsSecondary;
    UPROPERTY() TObjectPtr<UVerticalBox> EquipSlotsHost;

    // Grid parent containers (collapsed when no storage containers)
    UPROPERTY() TObjectPtr<UWidget> GridRow;
    UPROPERTY() TObjectPtr<UWidget> GridSizeBoxPrimary;
    UPROPERTY() TObjectPtr<UWidget> GridSizeBoxSecondary;

    // Placeholder shown when no storage containers exist
    UPROPERTY() TObjectPtr<UWidget> EmptyStoragePlaceholder;

    // Hand grid containers (always visible, separate from tabs)
    UPROPERTY() TObjectPtr<UBorder> LeftHandGridHost;
    UPROPERTY() TObjectPtr<UBorder> RightHandGridHost;
    UPROPERTY() TObjectPtr<UHorizontalBox> PocketGridsHost;

    // Built grid panels
    UPROPERTY() TObjectPtr<UUniformGridPanel> GridPanel;
    UPROPERTY() TObjectPtr<UUniformGridPanel> GridPanelSecondary;

    // Cell widgets (populated by GridBuilder)
    UPROPERTY() TArray<TObjectPtr<UProjectGridCell>> ContainerTabCells;
    UPROPERTY() TArray<TObjectPtr<UProjectGridCell>> SecondaryContainerTabCells;
    UPROPERTY() TArray<TObjectPtr<UTextBlock>> CellWidgets;
    UPROPERTY() TArray<TObjectPtr<UProjectGridCell>> CellBorders;
    UPROPERTY() TArray<TObjectPtr<UTextBlock>> SecondaryCellWidgets;
    UPROPERTY() TArray<TObjectPtr<UProjectGridCell>> SecondaryCellBorders;
    UPROPERTY() TArray<TObjectPtr<UProjectGridCell>> EquipSlotCells;

    // Hand grid cells (2x2 each, always visible)
    UPROPERTY() TArray<TObjectPtr<UTextBlock>> LeftHandCellWidgets;
    UPROPERTY() TArray<TObjectPtr<UTextBlock>> RightHandCellWidgets;
    UPROPERTY() TArray<TObjectPtr<UProjectGridCell>> LeftHandCells;
    UPROPERTY() TArray<TObjectPtr<UProjectGridCell>> RightHandCells;
    UPROPERTY() TObjectPtr<UUniformGridPanel> LeftHandGridPanel;
    UPROPERTY() TObjectPtr<UUniformGridPanel> RightHandGridPanel;
    bool bHandGridsBuilt = false;

    struct FPocketGridRuntime
    {
        int32 ViewModelPocketIndex = INDEX_NONE;
        FGameplayTag ContainerId;
        int32 GridWidth = 0;
        int32 GridHeight = 0;
        TObjectPtr<UUniformGridPanel> GridPanel = nullptr;
        TArray<TObjectPtr<UTextBlock>> CellWidgets;
        TArray<TObjectPtr<UProjectGridCell>> CellBorders;
    };
    TArray<FPocketGridRuntime> PocketGridRuntime;

    int32 PendingDragInstanceId = INDEX_NONE;

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
    FProjectUIGridDragDropController DragDropHandler;
    FInventoryPanelTextUpdater TextUpdater;
    FProjectUIPopupPresenter ContextMenuPresenter;
    FProjectUIHoverTooltipPresenter TooltipPresenter;

    // Cache tooltip content updates while hovering to avoid redundant widget updates.
    uint32 CachedTooltipContentHash = 0;
    bool bHasCachedTooltipContentHash = false;
};
