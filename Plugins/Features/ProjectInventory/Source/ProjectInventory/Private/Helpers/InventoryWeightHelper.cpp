// Copyright ALIS. All Rights Reserved.

#include "Helpers/InventoryWeightHelper.h"
#include "Inventory/InventoryTypes.h"
#include "Interfaces/IItemDataProvider.h"

namespace
{
    // Generic container metric calculation
    float CalculateContainerMetric(
        const TArray<FInventoryEntry>& Entries,
        FGameplayTag ContainerId,
        TMap<FPrimaryAssetId, FItemDataView>& ItemDataCache,
        const FInventoryWeightHelper::FWeightCallbacks& Callbacks,
        TFunction<float(const FItemDataView&, int32)> MetricGetter)
    {
        float Total = 0.f;

        for (const FInventoryEntry& Entry : Entries)
        {
            FGameplayTag EntryContainerId;
            FIntPoint EntryPos;
            bool bEntryRotated = false;
            if (!Callbacks.GetEffectivePlacement(Entry, EntryContainerId, EntryPos, bEntryRotated))
            {
                continue;
            }

            if (EntryContainerId != ContainerId)
            {
                continue;
            }

            // Check cache first
            FItemDataView ItemData;
            if (const FItemDataView* Cached = ItemDataCache.Find(Entry.ItemId))
            {
                ItemData = *Cached;
            }
            else
            {
                if (!Callbacks.GetItemDataView(Entry.ItemId, ItemData))
                {
                    continue;
                }
                ItemDataCache.Add(Entry.ItemId, ItemData);
            }

            Total += MetricGetter(ItemData, Entry.Quantity);
        }

        return Total;
    }
}

EWeightState FInventoryWeightHelper::ComputeWeightState(float WeightPercent, EWeightState PreviousState)
{
    // Iterate until stable (handles big jumps like 10% -> 130%)
    EWeightState State = PreviousState;
    for (int32 i = 0; i < 4; ++i)
    {
        const EWeightState Next = StepWeightState(WeightPercent, State);
        if (Next == State)
        {
            break;
        }
        State = Next;
    }
    return State;
}

EWeightState FInventoryWeightHelper::StepWeightState(float WeightPercent, EWeightState CurrentState)
{
    switch (CurrentState)
    {
    case EWeightState::Light:
        if (WeightPercent >= LIGHT_THRESHOLD)
        {
            return EWeightState::Medium;
        }
        break;

    case EWeightState::Medium:
        if (WeightPercent < LIGHT_THRESHOLD - HYSTERESIS)
        {
            return EWeightState::Light;
        }
        if (WeightPercent >= MEDIUM_THRESHOLD)
        {
            return EWeightState::Heavy;
        }
        break;

    case EWeightState::Heavy:
        if (WeightPercent < MEDIUM_THRESHOLD - HYSTERESIS)
        {
            return EWeightState::Medium;
        }
        if (WeightPercent > HEAVY_THRESHOLD)
        {
            return EWeightState::Overweight;
        }
        break;

    case EWeightState::Overweight:
        if (WeightPercent <= HEAVY_THRESHOLD - HYSTERESIS)
        {
            return EWeightState::Heavy;
        }
        break;
    }

    return CurrentState;
}

float FInventoryWeightHelper::CalculateContainerWeight(
    const TArray<FInventoryEntry>& Entries,
    FGameplayTag ContainerId,
    TMap<FPrimaryAssetId, FItemDataView>& ItemDataCache,
    const FWeightCallbacks& Callbacks)
{
    return CalculateContainerMetric(Entries, ContainerId, ItemDataCache, Callbacks,
        [](const FItemDataView& Data, int32 Qty) { return Data.Weight * Qty; });
}

float FInventoryWeightHelper::CalculateContainerVolume(
    const TArray<FInventoryEntry>& Entries,
    FGameplayTag ContainerId,
    TMap<FPrimaryAssetId, FItemDataView>& ItemDataCache,
    const FWeightCallbacks& Callbacks)
{
    return CalculateContainerMetric(Entries, ContainerId, ItemDataCache, Callbacks,
        [](const FItemDataView& Data, int32 Qty) { return Data.Volume * Qty; });
}
