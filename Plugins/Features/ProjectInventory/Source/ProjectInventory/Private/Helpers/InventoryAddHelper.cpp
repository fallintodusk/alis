// Copyright ALIS. All Rights Reserved.

#include "Helpers/InventoryAddHelper.h"
#include "Helpers/InventoryStackHelper.h"
#include "Helpers/InventoryGridPlacement.h"
#include "Inventory/InventoryTypes.h"
#include "Interfaces/IItemDataProvider.h"

int32 FInventoryAddHelper::CalculateStackTargets(
    FPrimaryAssetId ItemId,
    const FItemDataView& ItemData,
    int32 Quantity,
    int32 MaxStack,
    const TArray<FInventoryEntry>& Entries,
    TArray<FContainerState>& ContainerStates,
    const FAddCallbacks& Callbacks,
    TArray<FStackTarget>& OutTargets)
{
    OutTargets.Reset();
    int32 Remaining = Quantity;

    for (const FInventoryEntry& Entry : Entries)
    {
        if (Remaining <= 0) break;
        if (Entry.OverrideMagnitudes.Num() > 0) continue;
        if (Entry.ItemId != ItemId || Entry.Quantity >= MaxStack) continue;

        FGameplayTag EntryContainerId;
        FIntPoint EntryPos;
        bool bEntryRotated = false;
        if (!Callbacks.GetEffectivePlacement(Entry, EntryContainerId, EntryPos, bEntryRotated))
        {
            continue;
        }

        FContainerState* ContainerState = ContainerStates.FindByPredicate([&EntryContainerId](const FContainerState& S) {
            return S.Config.ContainerId == EntryContainerId;
        });
        if (!ContainerState || !Callbacks.ContainerAllowsItem(ContainerState->Config, ItemData))
        {
            continue;
        }

        const int32 ToAdd = FInventoryStackHelper::CalculateStackableQuantity(
            Entry, ItemData, MaxStack, ContainerState->Config,
            ContainerState->CurrentWeight, ContainerState->CurrentVolume, Remaining);

        if (ToAdd <= 0) continue;

        FStackTarget Target;
        Target.InstanceId = Entry.InstanceId;
        Target.Quantity = ToAdd;
        OutTargets.Add(Target);

        ContainerState->CurrentWeight += ItemData.Weight * ToAdd;
        ContainerState->CurrentVolume += ItemData.Volume * ToAdd;
        Remaining -= ToAdd;
    }

    return Remaining;
}

int32 FInventoryAddHelper::CalculateNewPlacements(
    const FItemDataView& ItemData,
    int32 Remaining,
    int32 MaxStack,
    TArray<FContainerState>& ContainerStates,
    const FAddCallbacks& Callbacks,
    TArray<FNewStackPlacement>& OutPlacements)
{
    OutPlacements.Reset();
    const bool bIsStackable = MaxStack > 1;

    while (Remaining > 0)
    {
        const int32 DesiredStackSize = bIsStackable ? FMath::Min(Remaining, MaxStack) : 1;
        bool bPlaced = false;

        for (FContainerState& ContainerState : ContainerStates)
        {
            if (!Callbacks.ContainerAllowsItem(ContainerState.Config, ItemData)) continue;

            const int32 StackSize = FInventoryStackHelper::CalculateNewStackQuantity(
                ItemData, ContainerState.Config, ContainerState.CurrentWeight,
                ContainerState.CurrentVolume, DesiredStackSize);

            if (StackSize <= 0) continue;

            FIntPoint GridPos(-1, -1);
            bool bRotated = false;
            FIntPoint ItemSize = FInventoryGridPlacement::GetItemGridSize(ItemData.GridSize, false);

            if (!Callbacks.FindFreeGridPos(ContainerState.Config, ItemSize, INDEX_NONE, GridPos)
                && ContainerState.Config.bAllowRotation)
            {
                const FIntPoint RotatedSize = FInventoryGridPlacement::GetItemGridSize(ItemData.GridSize, true);
                if (RotatedSize != ItemSize && Callbacks.FindFreeGridPos(ContainerState.Config, RotatedSize, INDEX_NONE, GridPos))
                {
                    bRotated = true;
                }
            }

            if (GridPos.X < 0 || GridPos.Y < 0) continue;

            FNewStackPlacement Placement;
            Placement.ContainerId = ContainerState.Config.ContainerId;
            Placement.GridPos = GridPos;
            Placement.bRotated = bRotated;
            Placement.Quantity = StackSize;
            OutPlacements.Add(Placement);

            ContainerState.CurrentWeight += ItemData.Weight * StackSize;
            ContainerState.CurrentVolume += ItemData.Volume * StackSize;
            Remaining -= StackSize;
            bPlaced = true;
            break;
        }

        if (!bPlaced) break;
    }

    return Remaining;
}
