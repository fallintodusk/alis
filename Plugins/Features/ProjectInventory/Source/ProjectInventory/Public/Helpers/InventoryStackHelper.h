// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Types/InventoryContainerConfig.h"

struct FInventoryEntry;
struct FItemDataView;

/**
 * SOLID helper: Pure stacking computation.
 * Single responsibility - stack calculations without modifying state.
 *
 * IMPORTANT: Keep under 100 lines. All methods are static and pure.
 */
struct PROJECTINVENTORY_API FInventoryStackHelper
{
    /** Container state for stacking simulation. */
    struct FContainerState
    {
        FInventoryContainerConfig Config;
        float CurrentWeight = 0.f;
        float CurrentVolume = 0.f;
    };

    /**
     * Calculate how many items can be added to existing stack.
     * @param Entry - Existing entry to stack onto
     * @param ItemData - Item data for incoming items
     * @param MaxStack - Max stack size
     * @param Container - Container config (for weight/volume limits)
     * @param ContainerWeight - Current container weight
     * @param ContainerVolume - Current container volume
     * @param Quantity - Quantity to add
     * @return Quantity that can actually stack
     */
    static int32 CalculateStackableQuantity(
        const FInventoryEntry& Entry,
        const FItemDataView& ItemData,
        int32 MaxStack,
        const FInventoryContainerConfig& Container,
        float ContainerWeight,
        float ContainerVolume,
        int32 Quantity);

    /**
     * Calculate how many items can fit in a new stack.
     * @param ItemData - Item data
     * @param Container - Container config
     * @param ContainerWeight - Current container weight
     * @param ContainerVolume - Current container volume
     * @param DesiredQuantity - Desired stack size
     * @return Quantity that can fit
     */
    static int32 CalculateNewStackQuantity(
        const FItemDataView& ItemData,
        const FInventoryContainerConfig& Container,
        float ContainerWeight,
        float ContainerVolume,
        int32 DesiredQuantity);

    /**
     * Calculate maximum allowed quantity by weight/volume constraints.
     * @param ItemData - Item data
     * @param MaxWeight - Max weight (0 = no limit)
     * @param MaxVolume - Max volume (0 = no limit)
     * @param CurrentWeight - Current weight
     * @param CurrentVolume - Current volume
     * @param RequestedQuantity - Requested quantity
     * @return Allowed quantity
     */
    static int32 CalculateAllowedQuantity(
        const FItemDataView& ItemData,
        float MaxWeight,
        float MaxVolume,
        float CurrentWeight,
        float CurrentVolume,
        int32 RequestedQuantity);
};
