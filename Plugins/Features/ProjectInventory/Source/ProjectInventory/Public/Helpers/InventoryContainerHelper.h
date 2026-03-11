// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

struct FInventoryContainerConfig;

/**
 * SOLID helper: Pure container configuration queries.
 * Single responsibility - container math without inventory state.
 *
 * IMPORTANT: Keep under 100 lines. All methods are static and pure.
 */
struct PROJECTINVENTORY_API FInventoryContainerHelper
{
    /** Find container index by ID. Returns INDEX_NONE if not found. */
    static int32 GetContainerIndex(
        FGameplayTag ContainerId,
        const TArray<FInventoryContainerConfig>& Containers);

    /** Get cell count for a container (respects MaxCells). Use for slot math. */
    static int32 GetContainerCellCount(const FInventoryContainerConfig& Container);

    /** Get raw grid cell count (ignores MaxCells). Use for simulation arrays. */
    static int32 GetContainerTotalCells(const FInventoryContainerConfig& Container);

    /** Get slot offset for a container (sum of cell counts before it). */
    static int32 GetContainerSlotOffset(
        FGameplayTag ContainerId,
        const TArray<FInventoryContainerConfig>& Containers);

    /** Compute global slot index from container ID and grid position. */
    static int32 ComputeSlotIndex(
        FGameplayTag ContainerId,
        FIntPoint GridPos,
        const TArray<FInventoryContainerConfig>& Containers);

    /** Convert global slot index to grid position within container. */
    static bool TryGetGridPosFromSlot(
        FGameplayTag ContainerId,
        int32 SlotIndex,
        const TArray<FInventoryContainerConfig>& Containers,
        FIntPoint& OutGridPos);

    /** Upsert container config into list (update existing or add new). */
    static void UpsertContainerConfig(
        const FInventoryContainerConfig& Config,
        TArray<FInventoryContainerConfig>& OutContainers);
};
