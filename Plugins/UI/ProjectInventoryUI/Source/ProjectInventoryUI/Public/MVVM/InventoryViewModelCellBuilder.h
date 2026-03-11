// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IInventoryReadOnly.h"

/**
 * SOLID helper: Builds cell text arrays from inventory entries.
 * Single responsibility - transform entries to grid cell data.
 *
 * IMPORTANT: Keep under 100 lines. Pure computation, no state.
 */
struct PROJECTINVENTORYUI_API FInventoryViewModelCellBuilder
{
    /**
     * Build cell texts and instance IDs for a container grid.
     * @param Entries - All inventory entries
     * @param ContainerId - Target container to filter by
     * @param GridWidth - Grid width in cells
     * @param GridHeight - Grid height in cells
     * @param OutCellInstanceIds - Output: instance ID per cell (INDEX_NONE = empty)
     * @param OutCellTexts - Output: display text per cell (empty = no label)
     */
    static void Build(
        const TArray<FInventoryEntryView>& Entries,
        FGameplayTag ContainerId,
        int32 GridWidth,
        int32 GridHeight,
        TArray<int32>& OutCellInstanceIds,
        TArray<FText>& OutCellTexts);

    /** Build label string for an entry (name + quantity). */
    static FString BuildEntryLabel(const FText& DisplayName, int32 Quantity, const FPrimaryAssetId& ItemId);
};
