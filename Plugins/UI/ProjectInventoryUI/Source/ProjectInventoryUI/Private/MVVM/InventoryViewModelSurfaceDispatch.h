// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class UInventoryViewModel;
struct FProjectUIGridDragPayload;

/**
 * Stateless surface-dispatch helpers split out of UInventoryViewModel.cpp.
 *
 * Consumed by UInventoryViewModel's IInventorySurfacePolicyProvider
 * overrides (IsCellEnabledForSurface, GetCellOccupant,
 * IsPayloadAllowedOnOccupant) and by FindPocketIndexByContainerTag. The
 * UClass virtual override methods stay on the UClass (UE interface
 * dispatch requires it) and are thin one-line forwards into this
 * namespace. This namespace reads VM state through the UClass's public
 * query API - it never writes and never captures state.
 *
 * Private; widgets keep consuming the UClass API.
 */
namespace InventoryViewModelSurfaceDispatch
{
    /**
     * Reverse lookup: find the pocket index whose cached container tag
     * equals ContainerTag, or INDEX_NONE.
     */
    int32 FindPocketIndexByContainerTag(const UInventoryViewModel& ViewModel, FGameplayTag ContainerTag);

    /** Tag-keyed cell enabled query - see IInventorySurfacePolicyProvider. */
    bool IsCellEnabledForSurface(const UInventoryViewModel& ViewModel, FGameplayTag SurfaceTag, int32 CellIndex);

    /** Tag-keyed cell occupant query - see IInventorySurfacePolicyProvider. */
    int32 GetCellOccupant(const UInventoryViewModel& ViewModel, FGameplayTag SurfaceTag, int32 CellIndex);

    /** Tag-keyed payload-vs-occupant validation - see IInventorySurfacePolicyProvider. */
    bool IsPayloadAllowedOnOccupant(
        const UInventoryViewModel& ViewModel,
        FGameplayTag SurfaceTag,
        const FProjectUIGridDragPayload& Payload,
        int32 OccupantId,
        int32 CellIndex);
}
