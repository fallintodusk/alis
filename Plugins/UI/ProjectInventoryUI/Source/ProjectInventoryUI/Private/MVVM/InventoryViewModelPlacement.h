// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Interfaces/IInventoryReadOnly.h"

/**
 * Stateless placement helpers split out of UInventoryViewModel.cpp.
 *
 * Consumed by UInventoryViewModel's hand/placement/overlap APIs. All
 * helpers operate on cached view arrays passed in by the UClass and read
 * no ViewModel member state directly. The UClass methods remain one-line
 * forwards so public + BlueprintPure API is unchanged.
 *
 * Private; widgets keep consuming the UClass API.
 */
namespace InventoryViewModelPlacement
{
    /**
     * Does an inventory placement of ItemSize at (ContainerId, GridPos)
     * overlap any non-ignored cached entry?
     */
    bool DoesCachedInventoryPlacementOverlap(
        const TArray<FInventoryEntryView>& CachedEntries,
        const FGameplayTag& ContainerId,
        FIntPoint GridPos,
        FIntPoint ItemSize,
        int32 IgnoreInstanceId);

    /** True if no cached entry (other than IgnoreInstanceId) lives in ContainerId. */
    bool IsContainerEmpty(
        const TArray<FInventoryEntryView>& CachedEntries,
        FGameplayTag ContainerId,
        int32 IgnoreInstanceId);

    /**
     * Resolve where a dropped item should land when the user aimed at a
     * generic hand grid. Prefers dedicated LeftHand/RightHand containers,
     * falls back to the shared Hands container.
     */
    bool ResolveHandDropTarget(
        const TArray<FInventoryContainerView>& CachedAllContainers,
        const TArray<FInventoryEntryView>& CachedEntries,
        bool bLeftHand,
        FGameplayTag& OutContainerId,
        FIntPoint& OutGridPos);

    /**
     * Find the first free grid position that fits ItemSize inside ContainerId.
     * Returns false if ContainerId is unknown or nothing fits.
     */
    bool TryResolveFreePlacementInContainer(
        const TArray<FInventoryContainerView>& CachedAllContainers,
        const TArray<FInventoryEntryView>& CachedEntries,
        const FGameplayTag& ContainerId,
        FIntPoint ItemSize,
        FIntPoint& OutGridPos,
        TOptional<FIntPoint> ExcludedGridPos = TOptional<FIntPoint>());

    /**
     * For a drop that would collide on the same hand, find an alternate
     * hand (or a free slot on the same dedicated hand excluding the
     * current position).
     */
    bool TryResolveAlternateHandDropTarget(
        const TArray<FInventoryContainerView>& CachedAllContainers,
        const TArray<FInventoryEntryView>& CachedEntries,
        const FGameplayTag& CurrentContainerId,
        FIntPoint CurrentGridPos,
        FIntPoint ItemSize,
        FGameplayTag& OutContainerId,
        FIntPoint& OutGridPos);
}
