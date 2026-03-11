// Copyright ALIS. All Rights Reserved.

#include "Helpers/InventoryViewHelper.h"
#include "Inventory/InventoryTypes.h"
#include "Types/InventoryContainerConfig.h"
#include "Interfaces/IItemDataProvider.h"
#include "Types/EquippedItemData.h"

FInventoryEntryView FInventoryViewHelper::BuildEntryView(
    const FInventoryEntry& Entry,
    const TMap<FGameplayTag, FEquippedItemData>& EquippedItems,
    const FViewCallbacks& Callbacks)
{
    FInventoryEntryView View;
    View.ItemId = Entry.ItemId;
    View.InstanceId = Entry.InstanceId;
    View.Quantity = Entry.Quantity;

    // Get effective placement
    FGameplayTag EffectiveContainerId;
    FIntPoint EffectiveGridPos;
    bool bEffectiveRotated = false;
    const bool bHasPlacement = Callbacks.GetEffectivePlacement(Entry, EffectiveContainerId, EffectiveGridPos, bEffectiveRotated);

    View.ContainerId = bHasPlacement ? EffectiveContainerId : Entry.ContainerId;
    View.GridPos = bHasPlacement ? EffectiveGridPos : Entry.GridPos;
    View.bRotated = bHasPlacement ? bEffectiveRotated : Entry.bRotated;

    const int32 DerivedSlotIndex = bHasPlacement ? Callbacks.ComputeSlotIndex(View.ContainerId, View.GridPos) : Entry.SlotIndex;
    View.SlotIndex = (DerivedSlotIndex >= 0) ? DerivedSlotIndex : Entry.SlotIndex;

    // Get item data
    FItemDataView ItemData;
    const bool bHasItemData = Callbacks.GetItemDataView(Entry.ItemId, ItemData);
    if (bHasItemData)
    {
        View.DisplayName = ItemData.DisplayName;
        View.Description = ItemData.Description;
        View.IconCode = ItemData.IconCode;
        View.EquipSlotTag = ItemData.EquipSlotTag;
        View.Weight = ItemData.Weight;
        View.Volume = ItemData.Volume;
        View.GridSize = ItemData.GridSize;
        View.MaxStack = ItemData.MaxStack;
        View.bCanBeDropped = ItemData.bCanBeDropped;
        View.bIsConsumable = ItemData.bIsConsumable;
        // Explicit action capability contract for UI: do not infer these in widgets/viewmodel.
        View.bCanUse = ItemData.bIsConsumable;
        View.bCanEquip = ItemData.EquipSlotTag.IsValid();
        // Must be set whenever explicit capabilities are populated by producers.
        View.bActionCapsPopulated = true;
        View.UseMagnitudes = ItemData.Magnitudes;
    }

    // Find equipped slot
    for (const auto& Pair : EquippedItems)
    {
        if (Pair.Value.InstanceId == Entry.InstanceId)
        {
            View.EquippedSlot = Pair.Key;
            break;
        }
    }

    // Build granted container IDs
    if (View.EquippedSlot.IsValid())
    {
        bool bHasItemGrants = false;
        if (bHasItemData && ItemData.ContainerGrants.Num() > 0)
        {
            for (const FInventoryContainerGrantView& Grant : ItemData.ContainerGrants)
            {
                if (Grant.ContainerId.IsValid())
                {
                    View.GrantedContainerIds.AddUnique(Grant.ContainerId);
                    bHasItemGrants = true;
                }
            }
        }

        if (!bHasItemGrants)
        {
            TArray<FInventoryContainerConfig> SlotGrants;
            if (Callbacks.GetEquipSlotGrants(View.EquippedSlot, SlotGrants))
            {
                for (const FInventoryContainerConfig& Grant : SlotGrants)
                {
                    if (Grant.ContainerId.IsValid())
                    {
                        View.GrantedContainerIds.AddUnique(Grant.ContainerId);
                    }
                }
            }
        }
    }

    // Copy instance data
    View.Durability = Entry.InstanceData.Durability;
    View.MaxDurability = 1000;
    View.Ammo = Entry.InstanceData.Ammo;
    View.Modifiers = Entry.InstanceData.Modifiers;

    // Apply per-instance overrides to match actual use-time values.
    for (const FMagnitudeOverride& Override : Entry.OverrideMagnitudes)
    {
        if (Override.Tag.IsValid())
        {
            View.UseMagnitudes.Add(Override.Tag, Override.Value);
        }
    }

    return View;
}

void FInventoryViewHelper::BuildEntriesView(
    const TArray<FInventoryEntry>& Entries,
    const TMap<FGameplayTag, FEquippedItemData>& EquippedItems,
    const FViewCallbacks& Callbacks,
    TArray<FInventoryEntryView>& OutViews)
{
    OutViews.Reset();
    OutViews.Reserve(Entries.Num());

    for (const FInventoryEntry& Entry : Entries)
    {
        OutViews.Add(BuildEntryView(Entry, EquippedItems, Callbacks));
    }
}

FInventoryContainerView FInventoryViewHelper::BuildContainerView(
    const FInventoryContainerConfig& Container,
    const FViewCallbacks& Callbacks)
{
    TMap<FPrimaryAssetId, FItemDataView> ItemDataCache;

    FInventoryContainerView View;
    View.ContainerId = Container.ContainerId;
    View.GridSize = Container.GridSize;
    View.CurrentWeight = Callbacks.GetContainerWeight(Container.ContainerId, ItemDataCache);
    View.MaxWeight = Container.MaxWeight;
    View.CurrentVolume = Callbacks.GetContainerVolume(Container.ContainerId, ItemDataCache);
    View.MaxVolume = Container.MaxVolume;
    View.MaxCells = Container.MaxCells;
    View.bSlotBased = Container.bSlotBased;
    return View;
}

void FInventoryViewHelper::BuildContainersView(
    const TArray<FInventoryContainerConfig>& Containers,
    const FViewCallbacks& Callbacks,
    TArray<FInventoryContainerView>& OutViews)
{
    OutViews.Reset();
    OutViews.Reserve(Containers.Num());

    // Build cache once for all containers
    TMap<FPrimaryAssetId, FItemDataView> ItemDataCache;

    for (const FInventoryContainerConfig& Container : Containers)
    {
        FInventoryContainerView View;
        View.ContainerId = Container.ContainerId;
        View.GridSize = Container.GridSize;
        View.CurrentWeight = Callbacks.GetContainerWeight(Container.ContainerId, ItemDataCache);
        View.MaxWeight = Container.MaxWeight;
        View.CurrentVolume = Callbacks.GetContainerVolume(Container.ContainerId, ItemDataCache);
        View.MaxVolume = Container.MaxVolume;
        View.MaxCells = Container.MaxCells;
        View.bSlotBased = Container.bSlotBased;
        OutViews.Add(View);
    }
}
