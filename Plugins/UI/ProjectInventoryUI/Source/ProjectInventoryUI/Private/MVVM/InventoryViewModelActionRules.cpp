// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "MVVM/InventoryViewModelActionRules.h"

DEFINE_LOG_CATEGORY_STATIC(LogInventoryVMActionRules, Log, All);

namespace InventoryViewModelActionRules
{
namespace
{
    const FName NAME_ActionUse(TEXT("Use"));
    const FName NAME_ActionEquip(TEXT("Equip"));
    const FName NAME_ActionDrop(TEXT("Drop"));
    const FName NAME_ActionSplit(TEXT("Split"));

    struct FInventoryActionCapabilityState
    {
        bool bCanUse = false;
        bool bCanEquip = false;
        bool bCanDrop = false;
        bool bCanSplit = false;
    };

    void AddActionDescriptor(
        TArray<FProjectUIActionDescriptor>& OutActions,
        FName ActionId,
        const FText& Label,
        bool bVisible,
        bool bEnabled,
        int32 Priority)
    {
        FProjectUIActionDescriptor Descriptor;
        Descriptor.ActionId = ActionId;
        Descriptor.Label = Label;
        Descriptor.bVisible = bVisible;
        Descriptor.bEnabled = bEnabled;
        Descriptor.Priority = Priority;
        OutActions.Add(MoveTemp(Descriptor));
    }

    FInventoryActionCapabilityState BuildActionCapabilityState(const FInventoryEntryView& Entry)
    {
        FInventoryActionCapabilityState CapabilityState;

        // SOT: action visibility/enabling must come from explicit producer flags only.
        // Do not reintroduce legacy inference from bIsConsumable or EquipSlotTag here.
        CapabilityState.bCanUse = Entry.bCanUse;
        CapabilityState.bCanEquip = Entry.bCanEquip;
        if (!Entry.bActionCapsPopulated)
        {
            static bool bLoggedMissingCapsMarker = false;
            if (!bLoggedMissingCapsMarker)
            {
                UE_LOG(
                    LogInventoryVMActionRules,
                    Warning,
                    TEXT("BuildActionCapabilityState: entry producer did not set bActionCapsPopulated; capabilities default to explicit values only."));
                bLoggedMissingCapsMarker = true;
            }
        }

        CapabilityState.bCanDrop = Entry.bCanBeDropped;
        CapabilityState.bCanSplit = Entry.MaxStack > 1 && Entry.Quantity > 1;

        // Current UX contract: "Use" and "Equip" are mutually exclusive in menu/buttons.
        if (CapabilityState.bCanUse && CapabilityState.bCanEquip)
        {
            CapabilityState.bCanEquip = false;
        }

        return CapabilityState;
    }
}

const FName& GetActionIdUse()
{
    return NAME_ActionUse;
}

const FName& GetActionIdEquip()
{
    return NAME_ActionEquip;
}

const FName& GetActionIdDrop()
{
    return NAME_ActionDrop;
}

const FName& GetActionIdSplit()
{
    return NAME_ActionSplit;
}

void BuildActionDescriptors(
    const FInventoryEntryView& Entry,
    const FActionBuildContext& Context,
    TArray<FProjectUIActionDescriptor>& OutActions)
{
    // SOT guardrail:
    // Action visibility and enabling rules must be authored only in this function.
    // Widgets consume descriptors and must not re-implement business rules.
    const FInventoryActionCapabilityState CapabilityState = BuildActionCapabilityState(Entry);

    OutActions.Reset();
    OutActions.Reserve(4);

    if (Context.bIsNearbyEntry)
    {
        AddActionDescriptor(
            OutActions,
            NAME_ActionUse,
            NSLOCTEXT("Inventory", "ActionTake", "Take"),
            true,
            Context.bCanTakeNearby,
            100);
        return;
    }

    AddActionDescriptor(
        OutActions,
        NAME_ActionUse,
        NSLOCTEXT("Inventory", "ActionUse", "Use"),
        CapabilityState.bCanUse,
        CapabilityState.bCanUse && Context.bHasCommands,
        100);

    // Show "Unequip" when item is currently equipped, "Equip" otherwise
    const bool bIsEquipped = Entry.EquippedSlot.IsValid();
    const bool bShowEquipAction = bIsEquipped || CapabilityState.bCanEquip;
    AddActionDescriptor(
        OutActions,
        NAME_ActionEquip,
        bIsEquipped ? NSLOCTEXT("Inventory", "ActionUnequip", "Unequip")
                    : NSLOCTEXT("Inventory", "ActionEquip", "Equip"),
        bShowEquipAction,
        bShowEquipAction && Context.bHasCommands,
        90);

    AddActionDescriptor(
        OutActions,
        NAME_ActionDrop,
        NSLOCTEXT("Inventory", "ActionDrop", "Drop"),
        CapabilityState.bCanDrop,
        CapabilityState.bCanDrop && Context.bHasCommands,
        80);

    AddActionDescriptor(
        OutActions,
        NAME_ActionSplit,
        NSLOCTEXT("Inventory", "ActionSplit", "Split"),
        CapabilityState.bCanSplit,
        CapabilityState.bCanSplit && Context.bHasCommands,
        70);
}

const FProjectUIActionDescriptor* FindActionDescriptor(
    const TArray<FProjectUIActionDescriptor>& Actions,
    FName ActionId)
{
    for (const FProjectUIActionDescriptor& Descriptor : Actions)
    {
        if (Descriptor.ActionId == ActionId)
        {
            return &Descriptor;
        }
    }
    return nullptr;
}

bool IsActionEnabled(const TArray<FProjectUIActionDescriptor>& Actions, FName ActionId)
{
    const FProjectUIActionDescriptor* Descriptor = FindActionDescriptor(Actions, ActionId);
    return Descriptor && Descriptor->bVisible && Descriptor->bEnabled;
}

bool HasEnabledActions(const TArray<FProjectUIActionDescriptor>& Actions)
{
    for (const FProjectUIActionDescriptor& Descriptor : Actions)
    {
        if (Descriptor.bVisible && Descriptor.bEnabled)
        {
            return true;
        }
    }
    return false;
}

} // namespace InventoryViewModelActionRules
