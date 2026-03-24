// Copyright ALIS. All Rights Reserved.

#include "Helpers/InventoryLootHelper.h"
#include "Helpers/InventoryContainerHelper.h"
#include "Inventory/InventoryTypes.h"
#include "Interfaces/IItemDataProvider.h"
#include "Loot/LootTypes.h"
#include "ProjectInventory.h"

namespace
{
int32 ResolveEffectiveMaxStack(
    const FInventoryLootHelper::FSimulationCallbacks& Callbacks,
    const FInventoryContainerConfig& Container,
    const FItemDataView& ItemData)
{
    checkf(Callbacks.GetEffectiveMaxStack,
        TEXT("FInventoryLootHelper requires GetEffectiveMaxStack to be bound."));
    return FMath::Max(1, Callbacks.GetEffectiveMaxStack(Container, ItemData));
}
}

bool FInventoryLootHelper::CanFitItems(
    const TArray<FLootEntry>& Items,
    const FSimulationInput& Input,
    const FSimulationCallbacks& Callbacks)
{
    if (Items.Num() == 0)
    {
        return true;
    }

    if (Input.ContainerOrder.Num() == 0)
    {
        return false;
    }

    // Aggregate incoming by ItemId
    TMap<FPrimaryAssetId, int32> IncomingById;
    for (const FLootEntry& LootEntry : Items)
    {
        if (!LootEntry.IsValid())
        {
            continue;
        }
        IncomingById.FindOrAdd(LootEntry.ObjectId) += LootEntry.Quantity;
    }

    // Calculate total incoming weight/volume
    float TotalIncomingWeight = 0.f;
    float TotalIncomingVolume = 0.f;
    TMap<FPrimaryAssetId, FItemDataView> ItemDataCache;

    for (const auto& Pair : IncomingById)
    {
        FItemDataView ItemData;
        if (!Callbacks.ResolveItemData(Pair.Key, ItemData))
        {
            return false;
        }
        ItemDataCache.Add(Pair.Key, ItemData);
        TotalIncomingWeight += ItemData.Weight * Pair.Value;
        TotalIncomingVolume += ItemData.Volume * Pair.Value;
    }

    // Check aggregate weight constraint
    if (Input.MaxWeight > 0.f && Input.CurrentWeight + TotalIncomingWeight > Input.MaxWeight)
    {
        return false;
    }

    // Check aggregate volume constraint
    if (Input.MaxVolume > 0.f && Input.CurrentVolume + TotalIncomingVolume > Input.MaxVolume)
    {
        return false;
    }

    // Initialize simulation
    TArray<FContainerSim> ContainerSims;
    TMap<int32, int32> SimQuantityByInstance;
    InitContainerSims(Input, Callbacks, ContainerSims, SimQuantityByInstance);

    // Process each item type
    for (const auto& Pair : IncomingById)
    {
        int32 Remaining = Pair.Value;
        const FItemDataView& ItemData = ItemDataCache.FindChecked(Pair.Key);
        const bool bCanPotentiallyStack = FMath::Max(1, ItemData.MaxStack) > 1;

        // Try stacking first
        if (bCanPotentiallyStack)
        {
            Remaining = TryStackIncoming(Pair.Key, Remaining, ItemData, Input.Entries,
                ContainerSims, SimQuantityByInstance, Callbacks);
        }

        // Place new stacks
        if (Remaining > 0)
        {
            if (!PlaceNewStacks(Remaining, ItemData, ContainerSims, Callbacks))
            {
                return false;
            }
        }
    }

    return true;
}

void FInventoryLootHelper::InitContainerSims(
    const FSimulationInput& Input,
    const FSimulationCallbacks& Callbacks,
    TArray<FContainerSim>& OutSims,
    TMap<int32, int32>& OutQuantityByInstance)
{
    OutSims.Reserve(Input.ContainerOrder.Num());

    for (const FInventoryContainerConfig& Container : Input.ContainerOrder)
    {
        FContainerSim Sim;
        Sim.Config = Container;

        // Use TotalCells for array size (not clamped by MaxCells)
        const int32 TotalCells = FInventoryContainerHelper::GetContainerTotalCells(Container);
        Sim.Occupied.Init(false, TotalCells);

        // Mark cells beyond MaxCells as occupied (forbidden)
        if (Container.MaxCells > 0 && Container.MaxCells < TotalCells)
        {
            for (int32 i = Container.MaxCells; i < TotalCells; ++i)
            {
                Sim.Occupied[i] = true;
            }
        }
        OutSims.Add(Sim);
    }

    // Mark existing items
    for (const FInventoryEntry& Entry : Input.Entries)
    {
        FGameplayTag ContainerId;
        FIntPoint GridPos;
        bool bRotated = false;
        if (!Callbacks.GetEffectivePlacement(Entry, ContainerId, GridPos, bRotated))
        {
            continue;
        }

        const int32 SimIndex = OutSims.IndexOfByPredicate([&ContainerId](const FContainerSim& Sim)
        {
            return Sim.Config.ContainerId == ContainerId;
        });
        if (SimIndex == INDEX_NONE)
        {
            continue;
        }

        FItemDataView EntryData;
        if (!Callbacks.ResolveItemData(Entry.ItemId, EntryData))
        {
            continue;
        }

        const FIntPoint EntrySize = Callbacks.GetItemGridSize(EntryData, bRotated);
        MarkRectInSim(OutSims[SimIndex], GridPos, EntrySize);
        OutSims[SimIndex].CurrentWeight += EntryData.Weight * Entry.Quantity;
        OutSims[SimIndex].CurrentVolume += EntryData.Volume * Entry.Quantity;
        OutQuantityByInstance.Add(Entry.InstanceId, Entry.Quantity);
    }
}

bool FInventoryLootHelper::FindFreePosInSim(const FContainerSim& Sim, FIntPoint ItemSize, FIntPoint& OutPos)
{
    if (Sim.Config.GridSize.X <= 0 || Sim.Config.GridSize.Y <= 0)
    {
        return false;
    }
    if (ItemSize.X <= 0 || ItemSize.Y <= 0)
    {
        return false;
    }
    if (ItemSize.X > Sim.Config.GridSize.X || ItemSize.Y > Sim.Config.GridSize.Y)
    {
        return false;
    }

    const int32 MaxX = Sim.Config.GridSize.X - ItemSize.X;
    const int32 MaxY = Sim.Config.GridSize.Y - ItemSize.Y;

    for (int32 Y = 0; Y <= MaxY; ++Y)
    {
        for (int32 X = 0; X <= MaxX; ++X)
        {
            bool bBlocked = false;
            for (int32 CellY = 0; CellY < ItemSize.Y && !bBlocked; ++CellY)
            {
                for (int32 CellX = 0; CellX < ItemSize.X; ++CellX)
                {
                    const int32 Index = (Y + CellY) * Sim.Config.GridSize.X + (X + CellX);
                    if (Sim.Occupied.IsValidIndex(Index) && Sim.Occupied[Index])
                    {
                        bBlocked = true;
                        break;
                    }
                }
            }
            if (!bBlocked)
            {
                OutPos = FIntPoint(X, Y);
                return true;
            }
        }
    }
    return false;
}

void FInventoryLootHelper::MarkRectInSim(FContainerSim& Sim, FIntPoint GridPos, FIntPoint ItemSize)
{
    for (int32 Y = 0; Y < ItemSize.Y; ++Y)
    {
        for (int32 X = 0; X < ItemSize.X; ++X)
        {
            const int32 Index = (GridPos.Y + Y) * Sim.Config.GridSize.X + (GridPos.X + X);
            if (Sim.Occupied.IsValidIndex(Index))
            {
                Sim.Occupied[Index] = true;
            }
        }
    }
}

int32 FInventoryLootHelper::TryStackIncoming(
    FPrimaryAssetId ItemId,
    int32 Remaining,
    const FItemDataView& ItemData,
    const TArray<FInventoryEntry>& Entries,
    TArray<FContainerSim>& ContainerSims,
    TMap<int32, int32>& SimQuantityByInstance,
    const FSimulationCallbacks& Callbacks)
{
    for (const FInventoryEntry& Entry : Entries)
    {
        if (Remaining <= 0) break;
        if (Entry.OverrideMagnitudes.Num() > 0) continue;
        if (Entry.ItemId != ItemId) continue;

        FGameplayTag ContainerId;
        FIntPoint GridPos;
        bool bRotated = false;
        if (!Callbacks.GetEffectivePlacement(Entry, ContainerId, GridPos, bRotated))
        {
            continue;
        }

        const int32 SimIndex = ContainerSims.IndexOfByPredicate([&ContainerId](const FContainerSim& Sim)
        {
            return Sim.Config.ContainerId == ContainerId;
        });
        if (SimIndex == INDEX_NONE) continue;
        if (!Callbacks.ContainerAllowsItem(ContainerSims[SimIndex].Config, ItemData)) continue;

        const int32 CurrentQty = SimQuantityByInstance.FindRef(Entry.InstanceId);
        const int32 EffectiveMaxStack = ResolveEffectiveMaxStack(Callbacks, ContainerSims[SimIndex].Config, ItemData);
        int32 SpaceInStack = EffectiveMaxStack - CurrentQty;
        if (SpaceInStack <= 0) continue;

        // Apply container constraints
        FContainerSim& Sim = ContainerSims[SimIndex];
        if (Sim.Config.MaxWeight > 0.f && ItemData.Weight > 0.f)
        {
            const float WeightLeft = Sim.Config.MaxWeight - Sim.CurrentWeight;
            if (WeightLeft <= 0.f) continue;
            SpaceInStack = FMath::Min(SpaceInStack, FMath::FloorToInt(WeightLeft / ItemData.Weight));
        }
        if (Sim.Config.MaxVolume > 0.f && ItemData.Volume > 0.f)
        {
            const float VolumeLeft = Sim.Config.MaxVolume - Sim.CurrentVolume;
            if (VolumeLeft <= 0.f) continue;
            SpaceInStack = FMath::Min(SpaceInStack, FMath::FloorToInt(VolumeLeft / ItemData.Volume));
        }

        const int32 ToAdd = FMath::Min(Remaining, SpaceInStack);
        if (ToAdd <= 0) continue;

        SimQuantityByInstance.Add(Entry.InstanceId, CurrentQty + ToAdd);
        Sim.CurrentWeight += ItemData.Weight * ToAdd;
        Sim.CurrentVolume += ItemData.Volume * ToAdd;
        Remaining -= ToAdd;
    }

    return Remaining;
}

bool FInventoryLootHelper::PlaceNewStacks(
    int32& Remaining,
    const FItemDataView& ItemData,
    TArray<FContainerSim>& ContainerSims,
    const FSimulationCallbacks& Callbacks)
{
    while (Remaining > 0)
    {
        bool bPlaced = false;

        for (FContainerSim& Sim : ContainerSims)
        {
            if (!Callbacks.ContainerAllowsItem(Sim.Config, ItemData)) continue;

            const int32 EffectiveMaxStack = ResolveEffectiveMaxStack(Callbacks, Sim.Config, ItemData);
            int32 StackSizeToPlace = FMath::Min(Remaining, EffectiveMaxStack);
            if (Sim.Config.MaxWeight > 0.f && ItemData.Weight > 0.f)
            {
                const float WeightLeft = Sim.Config.MaxWeight - Sim.CurrentWeight;
                if (WeightLeft <= 0.f) continue;
                StackSizeToPlace = FMath::Min(StackSizeToPlace, FMath::FloorToInt(WeightLeft / ItemData.Weight));
            }
            if (Sim.Config.MaxVolume > 0.f && ItemData.Volume > 0.f)
            {
                const float VolumeLeft = Sim.Config.MaxVolume - Sim.CurrentVolume;
                if (VolumeLeft <= 0.f) continue;
                StackSizeToPlace = FMath::Min(StackSizeToPlace, FMath::FloorToInt(VolumeLeft / ItemData.Volume));
            }
            if (StackSizeToPlace <= 0) continue;

            FIntPoint GridPos(-1, -1);
            FIntPoint ItemSize = Callbacks.GetItemGridSize(ItemData, false);

            if (!FindFreePosInSim(Sim, ItemSize, GridPos) && Sim.Config.bAllowRotation)
            {
                const FIntPoint RotatedSize = Callbacks.GetItemGridSize(ItemData, true);
                if (RotatedSize != ItemSize && FindFreePosInSim(Sim, RotatedSize, GridPos))
                {
                    ItemSize = RotatedSize;
                }
            }
            if (GridPos.X < 0 || GridPos.Y < 0) continue;

            MarkRectInSim(Sim, GridPos, ItemSize);
            Sim.CurrentWeight += ItemData.Weight * StackSizeToPlace;
            Sim.CurrentVolume += ItemData.Volume * StackSizeToPlace;
            Remaining -= StackSizeToPlace;
            bPlaced = true;
            break;
        }

        if (!bPlaced) return false;
    }

    return true;
}
