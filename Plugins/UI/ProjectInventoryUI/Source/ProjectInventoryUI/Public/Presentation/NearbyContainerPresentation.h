// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

class UInventoryViewModel;

/**
 * Pure projection helper for nearby-container UI text and control state.
 * Consumed by W_NearbyContainerPanel so presentation rules live in one
 * place instead of being copy-pasted between the old text updater and
 * the new widget.
 *
 * All methods tolerate a null VM and return safe defaults. Callers are
 * responsible for applying results to widgets.
 */
struct PROJECTINVENTORYUI_API FNearbyContainerPresentation
{
    /** Title text: container label if set, fallback to "Nearby Loot". */
    static FText BuildTitle(const UInventoryViewModel* VM);

    /** Single-line stats summary (weight + volume + optional depth hint). */
    static FText BuildStats(const UInventoryViewModel* VM);

    /** True iff TakeAll should be visible for the current VM state. */
    static bool ShouldShowTakeAll(const UInventoryViewModel* VM);

    /** True iff TakeAll should be interactable (visible AND has entries). */
    static bool IsTakeAllEnabled(const UInventoryViewModel* VM);

    /** True iff the "approach a container" hint should be shown (no active session). */
    static bool ShouldShowHint(const UInventoryViewModel* VM);
};
