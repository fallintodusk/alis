// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

struct FInventoryEntry;
struct FItemDataView;
struct FInventoryContainerConfig;

/**
 * SOLID helper: Pure move validation and overlap detection.
 * Single responsibility - move calculations without inventory state.
 *
 * IMPORTANT: Keep under 120 lines. All methods are static and pure.
 */
struct PROJECTINVENTORY_API FInventoryMoveHelper
{
    /** Result of move validation. */
    enum class EMoveResult : uint8
    {
        Success,
        InvalidEntry,
        InvalidContainer,
        ItemNotAllowed,
        OutOfBounds,
        SelfOverlap,
        MultipleOverlaps,
        WeightExceeded,
        VolumeExceeded,
        StackFull,
        NoOp
    };

    /** Overlap detection result. */
    struct FOverlapResult
    {
        bool bHasOverlap = false;
        bool bMultipleOverlaps = false;
        int32 OverlapInstanceId = 0;
    };

    /** Callbacks for resolving data. */
    struct FMoveCallbacks
    {
        TFunction<bool(const FInventoryEntry&, FGameplayTag&, FIntPoint&, bool&)> GetEffectivePlacement;
        TFunction<bool(FPrimaryAssetId, FItemDataView&)> GetItemDataView;
        TFunction<FIntPoint(const FItemDataView&, bool)> GetItemGridSize;
    };

    /**
     * Find overlapping entry at target position.
     * @param Entries - All inventory entries
     * @param TargetContainer - Target container ID
     * @param TargetPos - Target grid position
     * @param ItemSize - Size of item being moved
     * @param IgnoreInstanceId - Instance ID to ignore (self)
     * @param Callbacks - Resolution callbacks
     * @return Overlap result
     */
    static FOverlapResult FindOverlapAtTarget(
        const TArray<FInventoryEntry>& Entries,
        FGameplayTag TargetContainer,
        FIntPoint TargetPos,
        FIntPoint ItemSize,
        int32 IgnoreInstanceId,
        const FMoveCallbacks& Callbacks);

    /**
     * Check if move would result in self-overlap when splitting.
     * @param CurrentPos - Current position
     * @param CurrentSize - Current item size
     * @param TargetPos - Target position
     * @param TargetSize - Target item size (may differ if rotated)
     * @return true if overlaps with self
     */
    static bool CheckSelfOverlap(
        FIntPoint CurrentPos, FIntPoint CurrentSize,
        FIntPoint TargetPos, FIntPoint TargetSize);

    /**
     * Check if stacking is possible with target entry.
     * @param SourceEntry - Source entry
     * @param TargetEntry - Target entry to stack onto
     * @param MaxStack - Max stack size
     * @param Quantity - Quantity to move
     * @return true if can stack
     */
    static bool CanStackWith(
        const FInventoryEntry& SourceEntry,
        const FInventoryEntry& TargetEntry,
        int32 MaxStack,
        int32 Quantity);
};
