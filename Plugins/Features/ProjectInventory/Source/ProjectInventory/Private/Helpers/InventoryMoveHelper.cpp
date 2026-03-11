// Copyright ALIS. All Rights Reserved.

#include "Helpers/InventoryMoveHelper.h"
#include "Helpers/InventoryGridPlacement.h"
#include "Inventory/InventoryTypes.h"
#include "Interfaces/IItemDataProvider.h"

FInventoryMoveHelper::FOverlapResult FInventoryMoveHelper::FindOverlapAtTarget(
    const TArray<FInventoryEntry>& Entries,
    FGameplayTag TargetContainer,
    FIntPoint TargetPos,
    FIntPoint ItemSize,
    int32 IgnoreInstanceId,
    const FMoveCallbacks& Callbacks)
{
    FOverlapResult Result;

    for (const FInventoryEntry& Other : Entries)
    {
        if (Other.InstanceId == IgnoreInstanceId)
        {
            continue;
        }

        FGameplayTag OtherContainerId;
        FIntPoint OtherPos;
        bool bOtherRotated = false;
        if (!Callbacks.GetEffectivePlacement(Other, OtherContainerId, OtherPos, bOtherRotated))
        {
            continue;
        }

        if (OtherContainerId != TargetContainer)
        {
            continue;
        }

        FItemDataView OtherData;
        if (!Callbacks.GetItemDataView(Other.ItemId, OtherData))
        {
            continue;
        }

        const FIntPoint OtherSize = Callbacks.GetItemGridSize(OtherData, bOtherRotated);

        if (FInventoryGridPlacement::DoRectsOverlap(TargetPos, ItemSize, OtherPos, OtherSize))
        {
            if (Result.bHasOverlap)
            {
                Result.bMultipleOverlaps = true;
                return Result;
            }
            Result.bHasOverlap = true;
            Result.OverlapInstanceId = Other.InstanceId;
        }
    }

    return Result;
}

bool FInventoryMoveHelper::CheckSelfOverlap(
    FIntPoint CurrentPos, FIntPoint CurrentSize,
    FIntPoint TargetPos, FIntPoint TargetSize)
{
    return FInventoryGridPlacement::DoRectsOverlap(CurrentPos, CurrentSize, TargetPos, TargetSize);
}

bool FInventoryMoveHelper::CanStackWith(
    const FInventoryEntry& SourceEntry,
    const FInventoryEntry& TargetEntry,
    int32 MaxStack,
    int32 Quantity)
{
    // Can't stack items with different IDs
    if (SourceEntry.ItemId != TargetEntry.ItemId)
    {
        return false;
    }

    // Can't stack non-stackable items
    if (MaxStack <= 1)
    {
        return false;
    }

    // Can't stack items with overrides
    if (SourceEntry.OverrideMagnitudes.Num() > 0 || TargetEntry.OverrideMagnitudes.Num() > 0)
    {
        return false;
    }

    // Check space in target stack
    const int32 SpaceInStack = MaxStack - TargetEntry.Quantity;
    return SpaceInStack >= Quantity;
}
