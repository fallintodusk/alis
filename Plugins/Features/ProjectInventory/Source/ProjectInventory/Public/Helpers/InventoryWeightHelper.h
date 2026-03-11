// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Types/WeightState.h"

struct FInventoryEntry;
struct FItemDataView;

/**
 * SOLID helper: Pure weight/volume state computation.
 * Single responsibility - weight calculations without inventory state.
 */
struct PROJECTINVENTORY_API FInventoryWeightHelper
{
    /** Thresholds for weight states. */
    static constexpr float LIGHT_THRESHOLD = 60.f;
    static constexpr float MEDIUM_THRESHOLD = 80.f;
    static constexpr float HEAVY_THRESHOLD = 100.f;
    static constexpr float HYSTERESIS = 5.f;

    /** Callbacks for data resolution. */
    struct FWeightCallbacks
    {
        TFunction<bool(const FInventoryEntry&, FGameplayTag&, FIntPoint&, bool&)> GetEffectivePlacement;
        TFunction<bool(FPrimaryAssetId, FItemDataView&)> GetItemDataView;
    };

    /**
     * Compute weight state with hysteresis.
     * @param WeightPercent - Current weight as percentage of max (0-100+)
     * @param PreviousState - Previous weight state for hysteresis
     * @return New weight state
     */
    static EWeightState ComputeWeightState(float WeightPercent, EWeightState PreviousState);

    /**
     * Single step in weight state transition (internal).
     * @param WeightPercent - Current weight percentage
     * @param CurrentState - Current state to evaluate
     * @return Next state (may be same as current)
     */
    static EWeightState StepWeightState(float WeightPercent, EWeightState CurrentState);

    /**
     * Calculate container weight using callbacks.
     * @param Entries - All inventory entries
     * @param ContainerId - Target container
     * @param ItemDataCache - Cache for item data (will be populated)
     * @param Callbacks - Resolution callbacks
     * @return Total weight in container
     */
    static float CalculateContainerWeight(
        const TArray<FInventoryEntry>& Entries,
        FGameplayTag ContainerId,
        TMap<FPrimaryAssetId, FItemDataView>& ItemDataCache,
        const FWeightCallbacks& Callbacks);

    /**
     * Calculate container volume using callbacks.
     * @param Entries - All inventory entries
     * @param ContainerId - Target container
     * @param ItemDataCache - Cache for item data (will be populated)
     * @param Callbacks - Resolution callbacks
     * @return Total volume in container
     */
    static float CalculateContainerVolume(
        const TArray<FInventoryEntry>& Entries,
        FGameplayTag ContainerId,
        TMap<FPrimaryAssetId, FItemDataView>& ItemDataCache,
        const FWeightCallbacks& Callbacks);
};
