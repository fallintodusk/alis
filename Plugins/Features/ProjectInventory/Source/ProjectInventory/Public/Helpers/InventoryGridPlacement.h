// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

struct FInventoryContainerConfig;

/**
 * SOLID helper: Pure grid placement computation.
 * Single responsibility - grid math without inventory state.
 *
 * IMPORTANT: Keep under 150 lines. All methods are static and pure.
 */
struct PROJECTINVENTORY_API FInventoryGridPlacement
{
    /** Ensure grid size is at least 1x1. */
    static FIntPoint SanitizeGridSize(FIntPoint InSize);

    /** Get item size accounting for rotation. */
    static FIntPoint GetItemGridSize(FIntPoint BaseSize, bool bRotated);

    /** Check if rect fits within container bounds. */
    static bool IsRectWithinContainer(
        const FInventoryContainerConfig& Container,
        FIntPoint GridPos,
        FIntPoint ItemSize);

    /** Pure AABB overlap test between two rects. */
    static bool DoRectsOverlap(
        FIntPoint Pos1, FIntPoint Size1,
        FIntPoint Pos2, FIntPoint Size2);

    /** Clamp item size for width-only containers. */
    static FIntPoint ClampSizeForContainer(
        const FInventoryContainerConfig& Container,
        FIntPoint ItemSize);

    /**
     * Find free grid position using occupancy callback.
     * @param Container - Container config
     * @param ItemSize - Item size to place
     * @param IsOccupied - Callback: returns true if position is occupied
     * @param OutPos - Output: free position found
     * @return true if free position found
     */
    static bool FindFreeGridPos(
        const FInventoryContainerConfig& Container,
        FIntPoint ItemSize,
        const TFunction<bool(FIntPoint TestPos, FIntPoint TestSize)>& IsOccupied,
        FIntPoint& OutPos);

    /**
     * Find free slot in slot-based container.
     * @param SlotCount - Number of slots
     * @param IsSlotOccupied - Callback: returns true if slot is occupied
     * @param OutSlotIndex - Output: free slot index
     * @return true if free slot found
     */
    static bool FindFreeSlot(
        int32 SlotCount,
        const TFunction<bool(int32 SlotIndex)>& IsSlotOccupied,
        int32& OutSlotIndex);
};
