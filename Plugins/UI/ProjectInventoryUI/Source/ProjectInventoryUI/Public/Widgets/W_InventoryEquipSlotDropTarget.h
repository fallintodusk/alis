// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "Interaction/IInventoryDropTarget.h"
#include "W_InventoryEquipSlotDropTarget.generated.h"

class UProjectGridCell;
class UInventoryUIDragHostSubsystem;
class UInventoryDragDropOperation;
class UWidget;

/**
 * Smallest-semantic drop target for a single equip slot (chest, legs,
 * head, etc.). Slice 19 counterpart to UW_InventoryCellDropTarget.
 *
 * Equip slots are NOT laid out as UUniformGridPanel children, so they
 * cannot piggyback on the controller's surface-registration path the
 * way grid cells do. Instead, each slot wrapper holds its SlotTag
 * identity directly and forwards drag-over / drop events to dedicated
 * subsystem methods (UpdateEquipPreview / CompleteEquipDrop) that skip
 * geometry-based target resolution and use the wrapper's tag as the
 * target.
 *
 * Why a wrapper instead of overriding on UProjectGridCell or the
 * outer UBorder:
 *   - NativeOnDragOver / NativeOnDrop live on UUserWidget in UE 5.7.
 *   - UProjectGridCell derives from UBorder; moving it up to
 *     UUserWidget would force inventory-only concerns into ProjectUI
 *     (domain-agnostic framework). Keeping the wrapper inside
 *     ProjectInventoryUI preserves plugin boundaries.
 *
 * Drop contract (Slice 19):
 *   - Construction: owning builder calls SetSlotIdentity(SlotTag) once
 *     at build time.
 *   - NativeOnDragOver: forwards the screen pos + DragOp payload to
 *     UInventoryUIDragHostSubsystem::UpdateEquipPreview and returns
 *     FReply::Handled so Slate keeps routing moves to this slot.
 *   - NativeOnDrop: forwards to UInventoryUIDragHostSubsystem::CompleteEquipDrop.
 *     Returns FReply::Handled iff the subsystem dispatched a VM command.
 *
 * Implements IInventoryDropTarget so the Slice 20 fitness test marks
 * this class as allowed to override drag handlers.
 *
 * Layout contract: content is the wrapped UProjectGridCell (which holds
 * the slot icon). The wrapper itself is pass-through visible and adds
 * no padding, so the equip-slots grid layout is identical to the
 * pre-Slice-19 visual state.
 */
UCLASS()
class PROJECTINVENTORYUI_API UW_InventoryEquipSlotDropTarget
    : public UUserWidget
    , public IInventoryDropTarget
{
    GENERATED_BODY()

public:
    UW_InventoryEquipSlotDropTarget(const FObjectInitializer& ObjectInitializer);

    /**
     * Seed the slot tag identity. Called once by
     * FInventoryPanelGridBuilder after constructing the wrapper and
     * before adding it to the layout. The slot tag IS the target tag
     * used by the subsystem/router to resolve RequestEquipItem.
     */
    void SetSlotIdentity(const FGameplayTag& InSlotTag);

    /**
     * Set the hosted content (the outer UBorder that already frames
     * the inner UProjectGridCell). Builder calls this with the
     * already-constructed visual tree so the wrapper's root content
     * slot hosts it. The wrapper takes no ownership beyond parenting
     * through the widget tree.
     */
    void SetHostedContent(UWidget* InContent);

    const FGameplayTag& GetSlotTag() const { return SlotTag; }

    virtual bool NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual void NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

private:
    /**
     * Resolve the drag host subsystem via the owning local player.
     * Returns nullptr outside a real player context (e.g. archetype
     * evaluation) so callers must null-check.
     */
    UInventoryUIDragHostSubsystem* ResolveDragHost() const;

    /** Slot tag this wrapper represents (e.g. Item.EquipmentSlot.Chest). */
    UPROPERTY()
    FGameplayTag SlotTag;

    /** Hosted content (outer UBorder framing the inner UProjectGridCell). */
    UPROPERTY()
    TObjectPtr<UWidget> HostedContent;
};
