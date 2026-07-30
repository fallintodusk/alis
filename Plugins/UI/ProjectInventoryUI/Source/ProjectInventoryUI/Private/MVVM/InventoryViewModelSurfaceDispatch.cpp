// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "MVVM/InventoryViewModelSurfaceDispatch.h"

#include "Interaction/ProjectUIGridDragDropController.h"
#include "MVVM/InventoryViewModel.h"
#include "Policies/InventoryUIDropStackPolicy.h"
#include "ProjectGameplayTags.h"

namespace InventoryViewModelSurfaceDispatch
{

int32 FindPocketIndexByContainerTag(const UInventoryViewModel& ViewModel, FGameplayTag ContainerTag)
{
    if (!ContainerTag.IsValid())
    {
        return INDEX_NONE;
    }
    const TArray<FInventoryContainerView>& PocketContainers = ViewModel.GetCachedPocketContainers();
    for (int32 Index = 0; Index < PocketContainers.Num(); ++Index)
    {
        if (PocketContainers[Index].ContainerId == ContainerTag)
        {
            return Index;
        }
    }
    return INDEX_NONE;
}

bool IsCellEnabledForSurface(const UInventoryViewModel& ViewModel, FGameplayTag SurfaceTag, int32 CellIndex)
{
    // Follow-up #2: tag-keyed dispatch replaces per-surface closures. The
    // VM is the single domain owner for cell enabled state across hands,
    // primary tab, secondary tab, pockets, and the nearby/world surface.
    // Hands are always enabled (mirrors the original widget lambda that
    // returned true).
    if (!SurfaceTag.IsValid())
    {
        return true;
    }
    if (SurfaceTag == ProjectTags::Item_Container_LeftHand
        || SurfaceTag == ProjectTags::Item_Container_RightHand)
    {
        return true;
    }
    // Nearby/world widget registers under the parent tag and reads via
    // the secondary slot when bHasNearbyContainer is true.
    if (SurfaceTag.MatchesTag(ProjectTags::Item_Container_WorldStorage))
    {
        return ViewModel.IsSecondaryCellEnabled(CellIndex);
    }
    if (SurfaceTag == ViewModel.GetSelectedContainerId())
    {
        return ViewModel.IsCellEnabled(CellIndex);
    }
    if (SurfaceTag == ViewModel.GetSecondaryContainerId())
    {
        return ViewModel.IsSecondaryCellEnabled(CellIndex);
    }
    const int32 PocketIndex = FindPocketIndexByContainerTag(ViewModel, SurfaceTag);
    if (PocketIndex != INDEX_NONE)
    {
        return ViewModel.IsPocketCellEnabled(PocketIndex, CellIndex);
    }
    // Unknown surface: default open; the OccupantAllowedChecker still
    // gates the actual drop decision.
    return true;
}

int32 GetCellOccupant(const UInventoryViewModel& ViewModel, FGameplayTag SurfaceTag, int32 CellIndex)
{
    // Follow-up #2: tag-keyed dispatch replaces the old per-surface
    // widget closures. EmptyCellInstanceId is the VM's sentinel for an
    // empty cell; the controller contract uses INDEX_NONE, so remap.
    const auto Remap = [](int32 VMId) -> int32
    {
        return VMId == UInventoryViewModel::EmptyCellInstanceId ? INDEX_NONE : VMId;
    };

    if (!SurfaceTag.IsValid())
    {
        return INDEX_NONE;
    }
    if (SurfaceTag == ProjectTags::Item_Container_LeftHand)
    {
        return Remap(ViewModel.GetLeftHandInstanceId(CellIndex));
    }
    if (SurfaceTag == ProjectTags::Item_Container_RightHand)
    {
        return Remap(ViewModel.GetRightHandInstanceId(CellIndex));
    }
    if (SurfaceTag.MatchesTag(ProjectTags::Item_Container_WorldStorage))
    {
        return Remap(ViewModel.GetSecondaryCellInstanceId(CellIndex));
    }
    if (SurfaceTag == ViewModel.GetSelectedContainerId())
    {
        return Remap(ViewModel.GetCellInstanceId(CellIndex));
    }
    if (SurfaceTag == ViewModel.GetSecondaryContainerId())
    {
        return Remap(ViewModel.GetSecondaryCellInstanceId(CellIndex));
    }
    const int32 PocketIndex = FindPocketIndexByContainerTag(ViewModel, SurfaceTag);
    if (PocketIndex != INDEX_NONE)
    {
        return Remap(ViewModel.GetPocketCellInstanceId(PocketIndex, CellIndex));
    }
    return INDEX_NONE;
}

bool IsPayloadAllowedOnOccupant(
    const UInventoryViewModel& ViewModel,
    FGameplayTag SurfaceTag,
    const FProjectUIGridDragPayload& Payload,
    int32 OccupantId,
    int32 CellIndex)
{
    // Empty cell always wins. This short-circuit is the single line that
    // prevented the equip-backpack "every empty cell rejected" regression
    // when the old closure path returned null from GetDragDroppingContent.
    if (OccupantId == UInventoryViewModel::EmptyCellInstanceId || OccupantId == INDEX_NONE)
    {
        return true;
    }
    if (Payload.InstanceId == INDEX_NONE)
    {
        return false;
    }
    if (OccupantId == Payload.InstanceId)
    {
        // Self-occupied (dragged item still sits on its source cell).
        // Drop dispatcher decides the no-op vs split semantics.
        return true;
    }

    if (Payload.Quantity <= 0)
    {
        return false;
    }

    // Tabbed player-storage surfaces (primary + secondary) use the stack
    // merge rule. Any other surface (pockets, hands, nearby world) falls
    // through to the generic default above and we have already consumed
    // the empty / self cases; for those surfaces we currently reject
    // foreign occupants. This matches the Slice 13 pre-extraction rule set.
    const FGameplayTag PrimaryTag = ViewModel.GetSelectedContainerId();
    const FGameplayTag SecondaryTag = ViewModel.GetSecondaryContainerId();

    const bool bIsPrimaryTab = PrimaryTag.IsValid() && SurfaceTag == PrimaryTag;
    const bool bIsSecondaryTab = SecondaryTag.IsValid() && SurfaceTag == SecondaryTag;
    if (!bIsPrimaryTab && !bIsSecondaryTab)
    {
        return false;
    }

    const FGameplayTag& WorldTag = ProjectTags::Item_Container_WorldStorage;
    const bool bSourceIsWorld = Payload.SourceSurfaceTag.IsValid()
        && Payload.SourceSurfaceTag.MatchesTag(WorldTag);

    FInventoryEntryView SourceEntry;
    const bool bHasSource = bSourceIsWorld
        ? ViewModel.TryGetNearbyEntryByInstanceId(Payload.InstanceId, SourceEntry)
        : ViewModel.TryGetEntryByInstanceId(Payload.InstanceId, SourceEntry);
    if (!bHasSource)
    {
        return false;
    }

    FInventoryEntryView TargetEntry;
    bool bHasTarget = false;
    if (bIsSecondaryTab)
    {
        // Secondary tab resolves target by flat cell index.
        bHasTarget = ViewModel.TryGetSecondaryEntryByCellIndex(CellIndex, TargetEntry);
    }
    else
    {
        // Primary tab: occupant id is the authoritative target.
        bHasTarget = ViewModel.TryGetEntryByInstanceId(OccupantId, TargetEntry);
    }

    if (!bHasTarget)
    {
        return false;
    }

    return FInventoryUIDropStackPolicy::CanPreviewStackOnto(SourceEntry, TargetEntry, Payload.Quantity);
}

} // namespace InventoryViewModelSurfaceDispatch
