// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IInventoryReadOnly.h"
#include "Presentation/ProjectUIActionDescriptor.h"
#include "UObject/NameTypes.h"

/**
 * Stateless action-rule helpers split out of UInventoryViewModel.cpp.
 *
 * Consumed by UInventoryViewModel methods (BuildActionDescriptors and the
 * static action-id / descriptor utilities). Keeps all action visibility
 * and enable rules in one pure namespace; the UClass methods are thin
 * forwards so public + BlueprintCallable API is unchanged.
 *
 * This file is Private; widgets must keep consuming the UClass API.
 */
namespace InventoryViewModelActionRules
{
    /**
     * VM-side context required to decide action visibility/enable. Pure
     * inputs; this namespace never reads VM state directly.
     */
    struct FActionBuildContext
    {
        bool bHasCommands = false;
        bool bIsNearbyEntry = false;
        bool bCanTakeNearby = false;
    };

    /** Canonical action ids. Exposed via getters on the UClass for BP + tests. */
    const FName& GetActionIdUse();
    const FName& GetActionIdEquip();
    const FName& GetActionIdDrop();
    const FName& GetActionIdSplit();

    /**
     * Build descriptors for a single inventory entry given the VM-side
     * context. Single source of truth for action rules.
     */
    void BuildActionDescriptors(
        const FInventoryEntryView& Entry,
        const FActionBuildContext& Context,
        TArray<FProjectUIActionDescriptor>& OutActions);

    /** Utility helpers used by widget adapters; all stateless. */
    const FProjectUIActionDescriptor* FindActionDescriptor(
        const TArray<FProjectUIActionDescriptor>& Actions,
        FName ActionId);
    bool IsActionEnabled(const TArray<FProjectUIActionDescriptor>& Actions, FName ActionId);
    bool HasEnabledActions(const TArray<FProjectUIActionDescriptor>& Actions);
}
