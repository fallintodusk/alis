// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
//
// Second translation unit for UW_InventoryPanel.  Hosts interaction
// handlers (click / context-menu / tooltip / quantity / rotate / equip
// / use / drop / split) extracted from W_InventoryPanel.cpp during
// Phase 1.5 File 1 so the primary TU stays close to orchestration
// only.  Same UClass; no header change; no public API change.
//
// Method bodies here are defined on UW_InventoryPanel directly; C++
// permits splitting class definitions across multiple TUs.  Callers
// see no difference.

#include "Widgets/W_InventoryPanel.h"
#include "Widgets/W_ItemContextMenu.h"
#include "Widgets/W_ItemTooltip.h"
#include "Widgets/InventoryDragDropOperation.h"
#include "MVVM/InventoryViewModel.h"
#include "Interfaces/IInventoryReadOnly.h"
#include "Presentation/ProjectUIActionDescriptor.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"

namespace
{
uint32 BuildTooltipContentHashLocal(const FInventoryEntryView& Entry)
{
    uint32 Hash = GetTypeHash(Entry.InstanceId);
    Hash = HashCombine(Hash, GetTypeHash(Entry.Quantity));
    Hash = HashCombine(Hash, GetTypeHash(Entry.Weight));
    Hash = HashCombine(Hash, GetTypeHash(Entry.Volume));
    Hash = HashCombine(Hash, GetTypeHash(Entry.Durability));
    Hash = HashCombine(Hash, GetTypeHash(Entry.MaxDurability));
    Hash = HashCombine(Hash, GetTypeHash(Entry.Ammo));
    Hash = HashCombine(Hash, GetTypeHash(Entry.DisplayName.ToString()));
    Hash = HashCombine(Hash, GetTypeHash(Entry.Description.ToString()));
    Hash = HashCombine(Hash, GetTypeHash(Entry.IconCode));

    for (const FGameplayTag& Modifier : Entry.Modifiers)
    {
        Hash = HashCombine(Hash, GetTypeHash(Modifier));
    }

    return Hash;
}
} // namespace

// ============================================================================
// Cell Click Handlers (hand / pocket / equip / container tabs)
// ============================================================================

void UW_InventoryPanel::HandleLeftHandCellClicked(int32 CellIndex)
{
    if (!InventoryVM) { return; }
    const int32 InstanceId = InventoryVM->GetLeftHandInstanceId(CellIndex);
    PanelState.SetSelectedByInstanceId(
        InstanceId != UInventoryViewModel::EmptyCellInstanceId ? InstanceId : INDEX_NONE);
    RefreshAllText();
    UpdateAllVisuals();
}

void UW_InventoryPanel::HandleRightHandCellClicked(int32 CellIndex)
{
    if (!InventoryVM) { return; }
    const int32 InstanceId = InventoryVM->GetRightHandInstanceId(CellIndex);
    PanelState.SetSelectedByInstanceId(
        InstanceId != UInventoryViewModel::EmptyCellInstanceId ? InstanceId : INDEX_NONE);
    RefreshAllText();
    UpdateAllVisuals();
}

FEventReply UW_InventoryPanel::HandleLeftHandCellMouseDown(int32 CellIndex, bool /*bSecondary*/, const FPointerEvent& MouseEvent)
{
    if (!InventoryVM)
    {
        return UWidgetBlueprintLibrary::Handled();
    }

    const int32 InstanceId = InventoryVM->GetLeftHandInstanceId(CellIndex);
    const FKey Button = MouseEvent.GetEffectingButton();
    if (Button == EKeys::RightMouseButton)
    {
        PanelState.PendingDragCellIndex = INDEX_NONE;
        PendingDragInstanceId = INDEX_NONE;
        HandleLeftHandCellContextRequested(CellIndex);
        return UWidgetBlueprintLibrary::Handled();
    }

    if (Button == EKeys::LeftMouseButton)
    {
        PanelState.PendingDragCellIndex = INDEX_NONE;
        HandleLeftHandCellClicked(CellIndex);
        PendingDragInstanceId = InstanceId;
        if (InstanceId != UInventoryViewModel::EmptyCellInstanceId)
        {
            FInventoryEntryView Entry;
            if (InventoryVM->TryGetEntryByInstanceId(InstanceId, Entry))
            {
                return UWidgetBlueprintLibrary::DetectDragIfPressed(MouseEvent, this, EKeys::LeftMouseButton);
            }
        }
    }

    return UWidgetBlueprintLibrary::Handled();
}

FEventReply UW_InventoryPanel::HandleRightHandCellMouseDown(int32 CellIndex, bool /*bSecondary*/, const FPointerEvent& MouseEvent)
{
    if (!InventoryVM)
    {
        return UWidgetBlueprintLibrary::Handled();
    }

    const int32 InstanceId = InventoryVM->GetRightHandInstanceId(CellIndex);
    const FKey Button = MouseEvent.GetEffectingButton();
    if (Button == EKeys::RightMouseButton)
    {
        PanelState.PendingDragCellIndex = INDEX_NONE;
        PendingDragInstanceId = INDEX_NONE;
        HandleRightHandCellContextRequested(CellIndex);
        return UWidgetBlueprintLibrary::Handled();
    }

    if (Button == EKeys::LeftMouseButton)
    {
        PanelState.PendingDragCellIndex = INDEX_NONE;
        HandleRightHandCellClicked(CellIndex);
        PendingDragInstanceId = InstanceId;
        if (InstanceId != UInventoryViewModel::EmptyCellInstanceId)
        {
            FInventoryEntryView Entry;
            if (InventoryVM->TryGetEntryByInstanceId(InstanceId, Entry))
            {
                return UWidgetBlueprintLibrary::DetectDragIfPressed(MouseEvent, this, EKeys::LeftMouseButton);
            }
        }
    }

    return UWidgetBlueprintLibrary::Handled();
}

FEventReply UW_InventoryPanel::HandlePocketCellMouseDown(int32 EncodedCellIndex, bool /*bSecondary*/, const FPointerEvent& MouseEvent)
{
    if (!InventoryVM)
    {
        return UWidgetBlueprintLibrary::Handled();
    }

    int32 PocketIndex = INDEX_NONE;
    int32 CellIndex = INDEX_NONE;
    if (!DecodePocketCellIndex(EncodedCellIndex, PocketIndex, CellIndex))
    {
        return UWidgetBlueprintLibrary::Handled();
    }

    if (!InventoryVM->IsPocketCellEnabled(PocketIndex, CellIndex))
    {
        return UWidgetBlueprintLibrary::Handled();
    }

    const FKey Button = MouseEvent.GetEffectingButton();
    if (Button == EKeys::RightMouseButton)
    {
        PanelState.PendingDragCellIndex = INDEX_NONE;
        PendingDragInstanceId = INDEX_NONE;
        HandlePocketCellContextRequested(EncodedCellIndex);
        return UWidgetBlueprintLibrary::Handled();
    }

    if (Button == EKeys::LeftMouseButton)
    {
        PanelState.PendingDragCellIndex = INDEX_NONE;
        HandlePocketCellClicked(EncodedCellIndex);

        const int32 InstanceId = InventoryVM->GetPocketCellInstanceId(PocketIndex, CellIndex);
        PendingDragInstanceId = InstanceId;
        if (InstanceId != UInventoryViewModel::EmptyCellInstanceId)
        {
            FInventoryEntryView Entry;
            if (InventoryVM->TryGetEntryByInstanceId(InstanceId, Entry))
            {
                return UWidgetBlueprintLibrary::DetectDragIfPressed(MouseEvent, this, EKeys::LeftMouseButton);
            }
        }
    }

    return UWidgetBlueprintLibrary::Handled();
}

void UW_InventoryPanel::HandlePocketCellClicked(int32 EncodedCellIndex)
{
    if (!InventoryVM) { return; }

    int32 PocketIndex = INDEX_NONE;
    int32 CellIndex = INDEX_NONE;
    if (!DecodePocketCellIndex(EncodedCellIndex, PocketIndex, CellIndex))
    {
        return;
    }

    const int32 InstanceId = InventoryVM->GetPocketCellInstanceId(PocketIndex, CellIndex);
    PanelState.SetSelectedByInstanceId(
        InstanceId != UInventoryViewModel::EmptyCellInstanceId ? InstanceId : INDEX_NONE);
    RefreshAllText();
    UpdateAllVisuals();
}

void UW_InventoryPanel::HandleLeftHandCellContextRequested(int32 CellIndex)
{
    if (!InventoryVM) { return; }
    const int32 InstanceId = InventoryVM->GetLeftHandInstanceId(CellIndex);
    if (InstanceId == UInventoryViewModel::EmptyCellInstanceId)
    {
        PanelState.SetSelectedByInstanceId(INDEX_NONE);
        RefreshAllText();
        UpdateAllVisuals();
        HideContextMenu();
        return;
    }

    PanelState.SetSelectedByInstanceId(InstanceId);
    RefreshAllText();
    UpdateAllVisuals();

    FInventoryEntryView Entry;
    if (InventoryVM->TryGetEntryByInstanceId(InstanceId, Entry))
    {
        HideTooltip();
        const FVector2D AbsPos = FSlateApplication::Get().GetCursorPos();
        TArray<FProjectUIActionDescriptor> ActionDescriptors;
        InventoryVM->BuildActionDescriptors(Entry, ActionDescriptors);
        if (UInventoryViewModel::HasEnabledActions(ActionDescriptors))
        {
            ShowContextMenuForEntry(Entry, ActionDescriptors, AbsPos);
        }
        else
        {
            HideContextMenu();
        }
    }
    else
    {
        HideContextMenu();
    }
}

void UW_InventoryPanel::HandleRightHandCellContextRequested(int32 CellIndex)
{
    if (!InventoryVM) { return; }
    const int32 InstanceId = InventoryVM->GetRightHandInstanceId(CellIndex);
    if (InstanceId == UInventoryViewModel::EmptyCellInstanceId)
    {
        PanelState.SetSelectedByInstanceId(INDEX_NONE);
        RefreshAllText();
        UpdateAllVisuals();
        HideContextMenu();
        return;
    }

    PanelState.SetSelectedByInstanceId(InstanceId);
    RefreshAllText();
    UpdateAllVisuals();

    FInventoryEntryView Entry;
    if (InventoryVM->TryGetEntryByInstanceId(InstanceId, Entry))
    {
        HideTooltip();
        const FVector2D AbsPos = FSlateApplication::Get().GetCursorPos();
        TArray<FProjectUIActionDescriptor> ActionDescriptors;
        InventoryVM->BuildActionDescriptors(Entry, ActionDescriptors);
        if (UInventoryViewModel::HasEnabledActions(ActionDescriptors))
        {
            ShowContextMenuForEntry(Entry, ActionDescriptors, AbsPos);
        }
        else
        {
            HideContextMenu();
        }
    }
    else
    {
        HideContextMenu();
    }
}

void UW_InventoryPanel::HandlePocketCellContextRequested(int32 EncodedCellIndex)
{
    if (!InventoryVM) { return; }

    int32 PocketIndex = INDEX_NONE;
    int32 CellIndex = INDEX_NONE;
    if (!DecodePocketCellIndex(EncodedCellIndex, PocketIndex, CellIndex))
    {
        return;
    }

    const int32 InstanceId = InventoryVM->GetPocketCellInstanceId(PocketIndex, CellIndex);
    if (InstanceId == UInventoryViewModel::EmptyCellInstanceId)
    {
        PanelState.SetSelectedByInstanceId(INDEX_NONE);
        RefreshAllText();
        UpdateAllVisuals();
        HideContextMenu();
        return;
    }

    PanelState.SetSelectedByInstanceId(InstanceId);
    RefreshAllText();
    UpdateAllVisuals();

    FInventoryEntryView Entry;
    if (InventoryVM->TryGetEntryByInstanceId(InstanceId, Entry))
    {
        HideTooltip();
        const FVector2D AbsPos = FSlateApplication::Get().GetCursorPos();
        TArray<FProjectUIActionDescriptor> ActionDescriptors;
        InventoryVM->BuildActionDescriptors(Entry, ActionDescriptors);
        if (UInventoryViewModel::HasEnabledActions(ActionDescriptors))
        {
            ShowContextMenuForEntry(Entry, ActionDescriptors, AbsPos);
        }
        else
        {
            HideContextMenu();
        }
    }
    else
    {
        HideContextMenu();
    }
}

void UW_InventoryPanel::HandleEquipSlotClicked(int32 SlotIndex)
{
    if (!InventoryVM) { return; }
    // Left-click on equip slot = select item and show status (same as hand cells)
    const int32 InstanceId = InventoryVM->GetEquipSlotInstanceId(SlotIndex);
    if (InstanceId != 0)
    {
        PanelState.SetSelectedByInstanceId(InstanceId);
    }
    RefreshAllText();
    UpdateAllVisuals();
}

void UW_InventoryPanel::HandleEquipSlotRightClicked(int32 SlotIndex)
{
    if (!InventoryVM) { return; }
    const int32 InstanceId = InventoryVM->GetEquipSlotInstanceId(SlotIndex);
    if (InstanceId == 0) { return; }

    FInventoryEntryView Entry;
    if (!InventoryVM->TryGetEntryByInstanceId(InstanceId, Entry)) { return; }

    TArray<FProjectUIActionDescriptor> ActionDescriptors;
    InventoryVM->BuildActionDescriptors(Entry, ActionDescriptors);
    if (UInventoryViewModel::HasEnabledActions(ActionDescriptors))
    {
        HideTooltip();
        const FVector2D AbsPos = FSlateApplication::Get().GetCursorPos();
        ShowContextMenuForEntry(Entry, ActionDescriptors, AbsPos);
    }
}

void UW_InventoryPanel::HandleContainerTabSelected(int32 TabIndex)
{
    if (InventoryVM) { InventoryVM->SetSelectedContainerIndex(TabIndex); }
}

void UW_InventoryPanel::HandleSecondaryContainerTabSelected(int32 TabIndex)
{
    if (InventoryVM && !InventoryVM->GetbHasNearbyContainer()) { InventoryVM->SetSecondaryContainerIndex(TabIndex); }
}

// ============================================================================
// Command Click Handlers (Use / Drop / Equip / Qty / Rotate)
// ============================================================================

void UW_InventoryPanel::HandleUseClicked()
{
    FInventoryEntryView Entry;
    if (InventoryVM && PanelState.TryGetSelectedEntry(InventoryVM, Entry))
    {
        TArray<FProjectUIActionDescriptor> ActionDescriptors;
        InventoryVM->BuildActionDescriptors(Entry, ActionDescriptors);
        if (UInventoryViewModel::IsActionEnabled(ActionDescriptors, UInventoryViewModel::GetActionIdUse()))
        {
            InventoryVM->RequestUseItem(Entry.InstanceId);
        }
    }
}

void UW_InventoryPanel::HandleDropClicked()
{
    FInventoryEntryView Entry;
    if (InventoryVM && PanelState.TryGetSelectedEntry(InventoryVM, Entry))
    {
        TArray<FProjectUIActionDescriptor> ActionDescriptors;
        InventoryVM->BuildActionDescriptors(Entry, ActionDescriptors);
        if (UInventoryViewModel::IsActionEnabled(ActionDescriptors, UInventoryViewModel::GetActionIdDrop()))
        {
            InventoryVM->RequestDropItem(Entry.InstanceId, FMath::Max(1, PanelState.SelectedQuantity));
        }
    }
}

void UW_InventoryPanel::HandleEquipClicked()
{
    FInventoryEntryView Entry;
    if (InventoryVM && PanelState.TryGetSelectedEntry(InventoryVM, Entry))
    {
        TArray<FProjectUIActionDescriptor> ActionDescriptors;
        InventoryVM->BuildActionDescriptors(Entry, ActionDescriptors);
        if (UInventoryViewModel::IsActionEnabled(ActionDescriptors, UInventoryViewModel::GetActionIdEquip()))
        {
            InventoryVM->RequestEquipItem(Entry.InstanceId, Entry.EquipSlotTag);
        }
    }
}

void UW_InventoryPanel::HandleQtyDownClicked()
{
    if (PanelState.SelectedQuantity > 1)
    {
        --PanelState.SelectedQuantity;
        TextUpdater.UpdateQuantityControls(PanelState);
    }
}

void UW_InventoryPanel::HandleQtyUpClicked()
{
    if (PanelState.SelectedMaxQuantity > 0 && PanelState.SelectedQuantity < PanelState.SelectedMaxQuantity)
    {
        ++PanelState.SelectedQuantity;
        TextUpdater.UpdateQuantityControls(PanelState);
    }
}

void UW_InventoryPanel::HandleRotateClicked()
{
    PanelState.bRotateNextDrop = !PanelState.bRotateNextDrop;
    TextUpdater.UpdateRotateState(PanelState.bRotateNextDrop);

    if (!InventoryVM)
    {
        return;
    }

    UInventoryDragDropOperation* DragOp =
        Cast<UInventoryDragDropOperation>(UWidgetBlueprintLibrary::GetDragDroppingContent());
    if (!DragOp)
    {
        return;
    }

    FInventoryEntryView Entry;
    if (!InventoryVM->TryGetEntryByInstanceId(DragOp->InstanceId, Entry))
    {
        return;
    }

    DragOp->ApplyRotationFromEntry(Entry, !DragOp->bRotated);
    if (bHasLastDragScreenPos)
    {
        UpdateDragPreviewFromSubsystem(DragOp, LastDragScreenPos);
        UpdateAllVisuals();
    }
}

// ============================================================================
// Context Menu adapters (popup lifecycle in ProjectUI presenter, commands
// dispatched to the ViewModel)
// ============================================================================

void UW_InventoryPanel::ShowContextMenuForCell(int32 CellIndex, bool bSecondary, const FVector2D& ScreenPos)
{
    if (!InventoryVM) { return; }
    // Hide tooltip when context menu opens - they share screen space
    HideTooltip();
    FInventoryEntryView Entry;
    const bool bFound = bSecondary
        ? InventoryVM->TryGetSecondaryEntryByCellIndex(CellIndex, Entry)
        : InventoryVM->TryGetEntryByCellIndex(CellIndex, Entry);
    if (!bFound)
    {
        HideContextMenu();
        return;
    }

    TArray<FProjectUIActionDescriptor> ActionDescriptors;
    InventoryVM->BuildActionDescriptors(Entry, ActionDescriptors);
    if (UInventoryViewModel::HasEnabledActions(ActionDescriptors))
    {
        ShowContextMenuForEntry(Entry, ActionDescriptors, ScreenPos);
    }
    else
    {
        HideContextMenu();
    }
}

void UW_InventoryPanel::ShowContextMenuForEntry(const FInventoryEntryView& Entry, const TArray<FProjectUIActionDescriptor>& ActionDescriptors, const FVector2D& AbsolutePos)
{
    if (Entry.InstanceId == INDEX_NONE || !UInventoryViewModel::HasEnabledActions(ActionDescriptors))
    {
        HideContextMenu();
        return;
    }

    UW_ItemContextMenu* Menu = ContextMenuPresenter.GetPopupWidget<UW_ItemContextMenu>();
    if (!Menu)
    {
        HideContextMenu();
        return;
    }

    Menu->SetViewModel(InventoryVM);
    HideTooltip();
    ContextMenuPresenter.ShowClickCatcher(true);
    Menu->ShowForItem(Entry, ActionDescriptors, AbsolutePos);
}

void UW_InventoryPanel::HideContextMenu()
{
    if (UW_ItemContextMenu* Menu = ContextMenuPresenter.GetPopupWidget<UW_ItemContextMenu>())
    {
        if (Menu->IsMenuVisible())
        {
            Menu->Hide();
        }
    }
    ContextMenuPresenter.ShowClickCatcher(false);
}

void UW_InventoryPanel::HandleClickCatcherClicked() { HideContextMenu(); }
void UW_InventoryPanel::HandleContextMenuUse(int32 Id)
{
    TArray<FProjectUIActionDescriptor> ActionDescriptors;
    if (InventoryVM
        && InventoryVM->TryGetActionDescriptorsByInstanceId(Id, ActionDescriptors)
        && UInventoryViewModel::IsActionEnabled(ActionDescriptors, UInventoryViewModel::GetActionIdUse()))
    {
        InventoryVM->RequestUseItem(Id);
    }
}

void UW_InventoryPanel::HandleContextMenuEquip(int32 Id)
{
    TArray<FProjectUIActionDescriptor> ActionDescriptors;
    if (InventoryVM
        && InventoryVM->TryGetActionDescriptorsByInstanceId(Id, ActionDescriptors)
        && UInventoryViewModel::IsActionEnabled(ActionDescriptors, UInventoryViewModel::GetActionIdEquip()))
    {
        // Check if item is currently equipped -> unequip, otherwise equip
        FInventoryEntryView Entry;
        if (InventoryVM->TryGetEntryByInstanceId(Id, Entry) && Entry.EquippedSlot.IsValid())
        {
            InventoryVM->RequestUnequipItem(Entry.EquippedSlot);
        }
        else
        {
            InventoryVM->RequestEquipItemAuto(Id);
        }
    }
}

void UW_InventoryPanel::HandleContextMenuDrop(int32 Id)
{
    TArray<FProjectUIActionDescriptor> ActionDescriptors;
    if (InventoryVM
        && InventoryVM->TryGetActionDescriptorsByInstanceId(Id, ActionDescriptors)
        && UInventoryViewModel::IsActionEnabled(ActionDescriptors, UInventoryViewModel::GetActionIdDrop()))
    {
        FInventoryEntryView Entry;
        const int32 DropQuantity = InventoryVM->TryGetEntryByInstanceId(Id, Entry)
            ? FMath::Max(1, Entry.Quantity)
            : 1;
        InventoryVM->RequestDropItem(Id, DropQuantity);
    }
}

void UW_InventoryPanel::HandleContextMenuSplit(int32 Id)
{
    TArray<FProjectUIActionDescriptor> ActionDescriptors;
    if (InventoryVM
        && InventoryVM->TryGetActionDescriptorsByInstanceId(Id, ActionDescriptors)
        && UInventoryViewModel::IsActionEnabled(ActionDescriptors, UInventoryViewModel::GetActionIdSplit()))
    {
        InventoryVM->RequestSplitStackHalf(Id);
    }
}

void UW_InventoryPanel::HandleContextMenuClosed() { ContextMenuPresenter.ShowClickCatcher(false); }

// ============================================================================
// Tooltip adapters
// ============================================================================

void UW_InventoryPanel::UpdateTooltipForHover(const FVector2D& MouseViewportPos)
{
    UW_ItemTooltip* Tooltip = TooltipPresenter.GetTooltipWidget<UW_ItemTooltip>();
    if (!Tooltip || !InventoryVM)
    {
        return;
    }

    Tooltip->SetViewModel(InventoryVM);

    FInventoryEntryView Entry;
    if (!PanelState.TryGetHoveredEntry(InventoryVM, Entry))
    {
        HideTooltip();
        return;
    }

    const uint32 TooltipHash = BuildTooltipContentHashLocal(Entry);
    if (!bHasCachedTooltipContentHash || TooltipHash != CachedTooltipContentHash)
    {
        Tooltip->SetItemData(Entry);
        CachedTooltipContentHash = TooltipHash;
        bHasCachedTooltipContentHash = true;
    }

    Tooltip->SetVisibility(ESlateVisibility::HitTestInvisible);
    FVector2D AnchorViewportPos;
    if (TryResolveTooltipAnchorViewportPos(Entry, AnchorViewportPos))
    {
        TooltipPresenter.PositionAtAnchor(
            AnchorViewportPos,
            FVector2D(0.5f, 1.0f),
            FVector2D(0.0f, -12.0f));
    }
    else
    {
        TooltipPresenter.PositionNearCursor(MouseViewportPos);
    }
}

void UW_InventoryPanel::HideTooltip()
{
    if (UW_ItemTooltip* Tooltip = TooltipPresenter.GetTooltipWidget<UW_ItemTooltip>())
    {
        Tooltip->Clear();
    }
    TooltipPresenter.Hide();
    CachedTooltipContentHash = 0;
    bHasCachedTooltipContentHash = false;
}

FVector2D UW_InventoryPanel::ScreenToViewportPos(const FVector2D& ScreenPos) const
{
    FVector2D ViewportPos = ScreenPos;
    USlateBlueprintLibrary::ScreenToViewport(this, ScreenPos, ViewportPos);
    return ViewportPos;
}
