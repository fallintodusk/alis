// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Inventory/InventorySaveData.h"

struct FInventoryEntry;
struct FEquippedItemData;
class UProjectSaveSubsystem;

/**
 * SOLID helper: Pure save/load operations for inventory.
 * Single responsibility - persistence logic without component state.
 *
 * IMPORTANT: Keep under 100 lines. All methods are static and pure.
 */
struct PROJECTINVENTORY_API FInventorySaveHelper
{
    /** Default save key for inventory data. */
    static FName GetDefaultSaveKey() { return FName(TEXT("ProjectInventory.Inventory")); }

    /** Build save data from inventory entries and equipped items. */
    static void BuildSaveData(
        const TArray<FInventoryEntry>& Entries,
        const TMap<FGameplayTag, FEquippedItemData>& EquippedItems,
        FInventorySaveData& OutData);

    /** Callbacks for load operation - component provides state access. */
    struct FLoadCallbacks
    {
        TFunction<void()> ResetState;
        TFunction<int32(FGameplayTag, FIntPoint)> ComputeSlotIndex;
        TFunction<void(const FInventoryEntrySaveData&, int32)> AddEntry;
        TFunction<void(int32, FGameplayTag, bool)> RestoreEquipped;
        TFunction<void()> BroadcastChange;
    };

    /** Apply save data to inventory state via callbacks. */
    static void ApplyLoadData(
        const FInventorySaveData& Data,
        bool bApplyEquippedAbilities,
        const FLoadCallbacks& Callbacks);

    /** Save to subsystem (uses existing SerializeInventorySaveData). */
    static bool SaveToSubsystem(
        UProjectSaveSubsystem* Subsystem,
        FName SaveKey,
        const FInventorySaveData& Data);

    /** Load from subsystem (uses existing DeserializeInventorySaveData). */
    static bool LoadFromSubsystem(
        UProjectSaveSubsystem* Subsystem,
        FName SaveKey,
        FInventorySaveData& OutData);

    /**
     * Check if should use local save for this owner.
     * @param Owner - Actor that owns the inventory
     * @param World - World context
     * @return true if should persist to local save
     */
    static bool ShouldUseLocalSave(AActor* Owner, UWorld* World);
};
