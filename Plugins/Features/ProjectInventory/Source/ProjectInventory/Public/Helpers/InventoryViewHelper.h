// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Interfaces/IInventoryReadOnly.h"

struct FInventoryEntry;
struct FItemDataView;
struct FInventoryContainerConfig;
struct FEquippedItemData;

/**
 * SOLID helper: Pure view building for inventory data.
 * Single responsibility - build UI views without inventory component dependencies.
 *
 * IMPORTANT: Keep under 150 lines. All methods are static and pure.
 */
struct PROJECTINVENTORY_API FInventoryViewHelper
{
    /** Callbacks for resolving data (avoids component dependency). */
    struct FViewCallbacks
    {
        TFunction<bool(FPrimaryAssetId, FItemDataView&)> GetItemDataView;
        TFunction<bool(const FInventoryEntry&, FGameplayTag&, FIntPoint&, bool&)> GetEffectivePlacement;
        TFunction<int32(FGameplayTag, FIntPoint)> ComputeSlotIndex;
        TFunction<bool(FGameplayTag, FInventoryContainerConfig&)> GetContainerConfig;
        TFunction<bool(FGameplayTag, TArray<FInventoryContainerConfig>&)> GetEquipSlotGrants;
        TFunction<float(FGameplayTag, TMap<FPrimaryAssetId, FItemDataView>&)> GetContainerWeight;
        TFunction<float(FGameplayTag, TMap<FPrimaryAssetId, FItemDataView>&)> GetContainerVolume;
    };

    /**
     * Build entry view from inventory entry.
     * @param Entry - Source entry
     * @param EquippedItems - Map of equipped items
     * @param Callbacks - Resolution callbacks
     * @return Built view
     */
    static FInventoryEntryView BuildEntryView(
        const FInventoryEntry& Entry,
        const TMap<FGameplayTag, FEquippedItemData>& EquippedItems,
        const FViewCallbacks& Callbacks);

    /**
     * Build all entry views.
     * @param Entries - Source entries
     * @param EquippedItems - Map of equipped items
     * @param Callbacks - Resolution callbacks
     * @param OutViews - Output views
     */
    static void BuildEntriesView(
        const TArray<FInventoryEntry>& Entries,
        const TMap<FGameplayTag, FEquippedItemData>& EquippedItems,
        const FViewCallbacks& Callbacks,
        TArray<FInventoryEntryView>& OutViews);

    /**
     * Build container view from config.
     * @param Container - Source config
     * @param Callbacks - Resolution callbacks
     * @return Built view
     */
    static FInventoryContainerView BuildContainerView(
        const FInventoryContainerConfig& Container,
        const FViewCallbacks& Callbacks);

    /**
     * Build all container views.
     * @param Containers - Source configs
     * @param Callbacks - Resolution callbacks
     * @param OutViews - Output views
     */
    static void BuildContainersView(
        const TArray<FInventoryContainerConfig>& Containers,
        const FViewCallbacks& Callbacks,
        TArray<FInventoryContainerView>& OutViews);
};
