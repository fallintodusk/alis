// Copyright ALIS. All Rights Reserved.

#include "MVVM/InventoryViewModelPlacement.h"

#include "Geometry/InventoryGridGeometry.h"
#include "ProjectGameplayTags.h"

namespace InventoryViewModelPlacement
{

bool DoesCachedInventoryPlacementOverlap(
    const TArray<FInventoryEntryView>& CachedEntries,
    const FGameplayTag& ContainerId,
    FIntPoint GridPos,
    FIntPoint ItemSize,
    int32 IgnoreInstanceId)
{
    if (!ContainerId.IsValid() || GridPos.X < 0 || GridPos.Y < 0 || ItemSize.X <= 0 || ItemSize.Y <= 0)
    {
        return false;
    }

    for (const FInventoryEntryView& Entry : CachedEntries)
    {
        if (Entry.InstanceId == IgnoreInstanceId || Entry.ContainerId != ContainerId)
        {
            continue;
        }

        const FIntPoint EntrySize = Entry.bRotated
            ? FIntPoint(FMath::Max(1, Entry.GridSize.Y), FMath::Max(1, Entry.GridSize.X))
            : FIntPoint(FMath::Max(1, Entry.GridSize.X), FMath::Max(1, Entry.GridSize.Y));

        if (FInventoryGridGeometry::DoRectsOverlap(GridPos, ItemSize, Entry.GridPos, EntrySize))
        {
            return true;
        }
    }

    return false;
}

bool IsContainerEmpty(
    const TArray<FInventoryEntryView>& CachedEntries,
    FGameplayTag ContainerId,
    int32 IgnoreInstanceId)
{
    if (!ContainerId.IsValid())
    {
        return true;
    }

    for (const FInventoryEntryView& Entry : CachedEntries)
    {
        if (Entry.InstanceId == IgnoreInstanceId)
        {
            continue;
        }

        if (Entry.ContainerId == ContainerId)
        {
            return false;
        }
    }

    return true;
}

bool ResolveHandDropTarget(
    const TArray<FInventoryContainerView>& CachedAllContainers,
    const TArray<FInventoryEntryView>& CachedEntries,
    bool bLeftHand,
    FGameplayTag& OutContainerId,
    FIntPoint& OutGridPos)
{
    OutContainerId = FGameplayTag();
    OutGridPos = FIntPoint(-1, -1);

    const FGameplayTag DedicatedHandContainer = bLeftHand
        ? ProjectTags::Item_Container_LeftHand
        : ProjectTags::Item_Container_RightHand;

    for (const FInventoryContainerView& Container : CachedAllContainers)
    {
        if (Container.ContainerId == DedicatedHandContainer)
        {
            OutContainerId = DedicatedHandContainer;
            for (int32 Y = 0; Y < FMath::Max(1, Container.GridSize.Y); ++Y)
            {
                for (int32 X = 0; X < FMath::Max(1, Container.GridSize.X); ++X)
                {
                    const FIntPoint CandidatePos(X, Y);
                    if (!DoesCachedInventoryPlacementOverlap(CachedEntries, DedicatedHandContainer, CandidatePos, FIntPoint(1, 1), INDEX_NONE))
                    {
                        OutGridPos = CandidatePos;
                        return true;
                    }
                }
            }

            OutGridPos = FIntPoint::ZeroValue;
            return true;
        }
    }

    for (const FInventoryContainerView& Container : CachedAllContainers)
    {
        if (Container.ContainerId == ProjectTags::Item_Container_Hands)
        {
            OutContainerId = ProjectTags::Item_Container_Hands;
            OutGridPos = FIntPoint(bLeftHand ? 0 : 1, 0);
            return true;
        }
    }

    return false;
}

bool TryResolveFreePlacementInContainer(
    const TArray<FInventoryContainerView>& CachedAllContainers,
    const TArray<FInventoryEntryView>& CachedEntries,
    const FGameplayTag& ContainerId,
    FIntPoint ItemSize,
    FIntPoint& OutGridPos,
    TOptional<FIntPoint> ExcludedGridPos)
{
    OutGridPos = FIntPoint(-1, -1);

    if (!ContainerId.IsValid() || ItemSize.X <= 0 || ItemSize.Y <= 0)
    {
        return false;
    }

    const FInventoryContainerView* TargetContainer = CachedAllContainers.FindByPredicate(
        [&ContainerId](const FInventoryContainerView& Container)
        {
            return Container.ContainerId == ContainerId;
        });
    if (!TargetContainer)
    {
        return false;
    }

    const int32 MaxWidth = FMath::Max(1, TargetContainer->GridSize.X);
    const int32 MaxHeight = FMath::Max(1, TargetContainer->GridSize.Y);
    for (int32 Y = 0; Y <= MaxHeight - ItemSize.Y; ++Y)
    {
        for (int32 X = 0; X <= MaxWidth - ItemSize.X; ++X)
        {
            const FIntPoint CandidatePos(X, Y);
            if (ExcludedGridPos.IsSet() && CandidatePos == ExcludedGridPos.GetValue())
            {
                continue;
            }

            if (!DoesCachedInventoryPlacementOverlap(CachedEntries, ContainerId, CandidatePos, ItemSize, INDEX_NONE))
            {
                OutGridPos = CandidatePos;
                return true;
            }
        }
    }

    return false;
}

bool TryResolveAlternateHandDropTarget(
    const TArray<FInventoryContainerView>& CachedAllContainers,
    const TArray<FInventoryEntryView>& CachedEntries,
    const FGameplayTag& CurrentContainerId,
    FIntPoint CurrentGridPos,
    FIntPoint ItemSize,
    FGameplayTag& OutContainerId,
    FIntPoint& OutGridPos)
{
    OutContainerId = FGameplayTag();
    OutGridPos = FIntPoint(-1, -1);

    if (CurrentContainerId == ProjectTags::Item_Container_LeftHand
        || CurrentContainerId == ProjectTags::Item_Container_RightHand)
    {
        FIntPoint SameHandGridPos = FIntPoint(-1, -1);
        if (TryResolveFreePlacementInContainer(CachedAllContainers, CachedEntries, CurrentContainerId, ItemSize, SameHandGridPos, CurrentGridPos))
        {
            OutContainerId = CurrentContainerId;
            OutGridPos = SameHandGridPos;
            return true;
        }
    }

    FGameplayTag AlternateContainerId;
    if (CurrentContainerId == ProjectTags::Item_Container_LeftHand)
    {
        AlternateContainerId = ProjectTags::Item_Container_RightHand;
    }
    else if (CurrentContainerId == ProjectTags::Item_Container_RightHand)
    {
        AlternateContainerId = ProjectTags::Item_Container_LeftHand;
    }
    else
    {
        FGameplayTag LeftContainerId;
        FIntPoint LeftGridPos = FIntPoint(-1, -1);
        const bool bHasLeft = ResolveHandDropTarget(CachedAllContainers, CachedEntries, true, LeftContainerId, LeftGridPos);

        FGameplayTag RightContainerId;
        FIntPoint RightGridPos = FIntPoint(-1, -1);
        const bool bHasRight = ResolveHandDropTarget(CachedAllContainers, CachedEntries, false, RightContainerId, RightGridPos);

        if (bHasLeft && CurrentContainerId == LeftContainerId && CurrentGridPos == LeftGridPos)
        {
            OutContainerId = RightContainerId;
            OutGridPos = RightGridPos;
            return bHasRight;
        }

        if (bHasRight && CurrentContainerId == RightContainerId && CurrentGridPos == RightGridPos)
        {
            OutContainerId = LeftContainerId;
            OutGridPos = LeftGridPos;
            return bHasLeft;
        }

        return false;
    }

    FIntPoint AlternateGridPos = FIntPoint(-1, -1);
    if (TryResolveFreePlacementInContainer(CachedAllContainers, CachedEntries, AlternateContainerId, ItemSize, AlternateGridPos))
    {
        OutContainerId = AlternateContainerId;
        OutGridPos = AlternateGridPos;
        return true;
    }

    return false;
}

} // namespace InventoryViewModelPlacement
