// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Types/InventoryContainerConfig.h"

struct FInventoryEntry;
struct FItemDataView;
struct FLootEntry;

/**
 * SOLID helper: Pure loot fitting simulation.
 * Single responsibility - simulate whether items fit without modifying state.
 *
 * IMPORTANT: Keep under 200 lines. All methods are static and pure.
 */
struct PROJECTINVENTORY_API FInventoryLootHelper
{
    /** Simulation state for a container during CanFitItems check. */
    struct FContainerSim
    {
        FInventoryContainerConfig Config;
        float CurrentWeight = 0.f;
        float CurrentVolume = 0.f;
        TArray<bool> Occupied;
    };

    /** Input data for CanFitItems simulation. */
    struct FSimulationInput
    {
        TArray<FInventoryContainerConfig> ContainerOrder;
        TArray<FInventoryEntry> Entries;
        float CurrentWeight = 0.f;
        float CurrentVolume = 0.f;
        float MaxWeight = 0.f;
        float MaxVolume = 0.f;
    };

    /** Callbacks for resolving item data and placement (avoids component dependency). */
    struct FSimulationCallbacks
    {
        TFunction<bool(FPrimaryAssetId, FItemDataView&)> ResolveItemData;
        TFunction<bool(const FInventoryEntry&, FGameplayTag&, FIntPoint&, bool&)> GetEffectivePlacement;
        TFunction<FIntPoint(const FItemDataView&, bool)> GetItemGridSize;
        TFunction<bool(const FInventoryContainerConfig&, const FItemDataView&)> ContainerAllowsItem;
        // Required: helper uses container-contextual stack caps, not authored MaxStack directly.
        TFunction<int32(const FInventoryContainerConfig&, const FItemDataView&)> GetEffectiveMaxStack;
    };

    /**
     * Check if items can fit in inventory (pure simulation).
     * @param Items - Loot entries to check
     * @param Input - Current inventory state
     * @param Callbacks - Resolution callbacks
     * @return true if all items can fit
     */
    static bool CanFitItems(
        const TArray<FLootEntry>& Items,
        const FSimulationInput& Input,
        const FSimulationCallbacks& Callbacks);

private:
    /** Initialize simulation state from input. */
    static void InitContainerSims(
        const FSimulationInput& Input,
        const FSimulationCallbacks& Callbacks,
        TArray<FContainerSim>& OutSims,
        TMap<int32, int32>& OutQuantityByInstance);

    /** Find free position in simulated container. */
    static bool FindFreePosInSim(const FContainerSim& Sim, FIntPoint ItemSize, FIntPoint& OutPos);

    /** Mark rect as occupied in simulation. */
    static void MarkRectInSim(FContainerSim& Sim, FIntPoint GridPos, FIntPoint ItemSize);

    /** Try to stack incoming items with existing entries. */
    static int32 TryStackIncoming(
        FPrimaryAssetId ItemId,
        int32 Remaining,
        const FItemDataView& ItemData,
        const TArray<FInventoryEntry>& Entries,
        TArray<FContainerSim>& ContainerSims,
        TMap<int32, int32>& SimQuantityByInstance,
        const FSimulationCallbacks& Callbacks);

    /** Place new stacks for remaining items. */
    static bool PlaceNewStacks(
        int32& Remaining,
        const FItemDataView& ItemData,
        TArray<FContainerSim>& ContainerSims,
        const FSimulationCallbacks& Callbacks);
};
