// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Types/InventoryContainerConfig.h"

struct FInventoryEntry;
struct FItemDataView;

/**
 * SOLID helper: Pure add item logic.
 * Single responsibility - calculate where to place items without modifying state.
 *
 * IMPORTANT: Keep under 150 lines. All methods are static and pure.
 */
struct PROJECTINVENTORY_API FInventoryAddHelper
{
    /** Result of calculating stack target. */
    struct FStackTarget
    {
        int32 InstanceId = 0;
        int32 Quantity = 0;
    };

    /** Result of calculating new stack placement. */
    struct FNewStackPlacement
    {
        FGameplayTag ContainerId;
        FIntPoint GridPos = FIntPoint(-1, -1);
        bool bRotated = false;
        int32 Quantity = 0;
    };

    /** Container state for add calculation. */
    struct FContainerState
    {
        FInventoryContainerConfig Config;
        float CurrentWeight = 0.f;
        float CurrentVolume = 0.f;
    };

    /** Callbacks for resolving data. */
    struct FAddCallbacks
    {
        TFunction<bool(const FInventoryEntry&, FGameplayTag&, FIntPoint&, bool&)> GetEffectivePlacement;
        TFunction<bool(const FInventoryContainerConfig&, FIntPoint, int32, FIntPoint&)> FindFreeGridPos;
        TFunction<bool(const FInventoryContainerConfig&, const FItemDataView&)> ContainerAllowsItem;
        // Required: helper uses container-contextual stack caps, not authored MaxStack directly.
        TFunction<int32(const FInventoryContainerConfig&, const FItemDataView&)> GetEffectiveMaxStack;
    };

    /**
     * Calculate all stack targets for incoming items.
     * @param ItemId - Item to add
     * @param ItemData - Item data
     * @param Quantity - Quantity to add
     * @param Entries - Existing entries
     * @param ContainerStates - Container states
     * @param Callbacks - Resolution callbacks
     * @param OutTargets - Stack targets to fill
     * @return Remaining quantity after stacking
     */
    static int32 CalculateStackTargets(
        FPrimaryAssetId ItemId,
        const FItemDataView& ItemData,
        int32 Quantity,
        const TArray<FInventoryEntry>& Entries,
        TArray<FContainerState>& ContainerStates,
        const FAddCallbacks& Callbacks,
        TArray<FStackTarget>& OutTargets);

    /**
     * Calculate new stack placements for remaining items.
     * @param ItemData - Item data
     * @param Remaining - Remaining quantity to place
     * @param ContainerStates - Container states (will be modified with weight/volume)
     * @param Callbacks - Resolution callbacks
     * @param OutPlacements - Placements to create
     * @return Remaining quantity that couldn't be placed
     */
    static int32 CalculateNewPlacements(
        const FItemDataView& ItemData,
        int32 Remaining,
        TArray<FContainerState>& ContainerStates,
        const FAddCallbacks& Callbacks,
        TArray<FNewStackPlacement>& OutPlacements);
};
