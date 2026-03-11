// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Interfaces/IInventoryReadOnly.h"

/**
 * SOLID helper: Builds equip slot display data from inventory entries.
 * Single responsibility - transform entries to equip slot labels.
 *
 * IMPORTANT: Keep under 100 lines. Pure computation, no state.
 */
struct PROJECTINVENTORYUI_API FInventoryViewModelEquipSlotBuilder
{
    /** Output data from Build(). */
    struct FResult
    {
        TArray<FGameplayTag> SlotTags;
        TArray<int32> InstanceIds;
        TArray<FText> Labels;
        TArray<FText> ShortLabels;
        TArray<FText> ItemLabels;
        TArray<FString> ItemIconCodes;
    };

    /**
     * Build equip slot display data.
     * @param Entries - All inventory entries (checks EquippedSlot field)
     * @param OutResult - Output: all equip slot arrays
     */
    static void Build(const TArray<FInventoryEntryView>& Entries, FResult& OutResult);

    /** Get the standard equip slot tags (MainHand, OffHand, Head, etc.). */
    static void GetStandardSlotTags(TArray<FGameplayTag>& OutTags);
};
