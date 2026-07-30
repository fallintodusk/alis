// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "IInventoryDropCommandTarget.generated.h"

UINTERFACE(MinimalAPI, Meta = (CannotImplementInterfaceInBlueprint))
class UInventoryDropCommandTarget : public UInterface
{
    GENERATED_BODY()
};

/**
 * Narrow command surface the router dispatches through. Exposes ONLY the
 * four methods FInventoryDropRouter decides between, decoupling router +
 * subsystem from UInventoryViewModel as a concrete type. Tests can mock
 * via this interface without subclassing the VM; future non-VM dispatch
 * targets (e.g. a crafting command bus) can implement it without
 * inheriting inventory VM state.
 *
 * Implemented by UInventoryViewModel (production) and
 * UInventoryViewModelSpy (tests, via inheritance).
 */
class PROJECTINVENTORYUI_API IInventoryDropCommandTarget
{
    GENERATED_BODY()
public:
    virtual void RequestMoveItem(
        int32 InstanceId,
        FGameplayTag FromContainer,
        FIntPoint FromPos,
        FGameplayTag ToContainer,
        FIntPoint ToPos,
        int32 Quantity,
        bool bRotated) = 0;

    virtual void RequestTakeNearbyItemToContainer(
        int32 InstanceId,
        FGameplayTag TargetContainerId,
        FIntPoint TargetGridPos,
        bool bTargetRotated,
        int32 Quantity = 1) = 0;

    virtual void RequestStoreItemInNearbyContainerAt(
        int32 InstanceId,
        FIntPoint TargetGridPos,
        bool bTargetRotated,
        int32 Quantity = 1) = 0;

    /**
     * Rearrange an entry already in the open nearby world-container to a
     * new grid pos/rotation. Dispatched by FInventoryDropRouter when both
     * source and target surface tags match Item.Container.WorldStorage -
     * i.e. the player drops a world-container entry on a different cell
     * inside the same nearby container. Implementations forward to the
     * world-container transfer bridge's atomic MoveWithinWorldContainer.
     */
    virtual void RequestMoveItemInNearbyContainer(
        int32 InstanceId,
        FIntPoint TargetGridPos,
        bool bTargetRotated,
        int32 Quantity = 1) = 0;

    virtual void RequestEquipItem(int32 InstanceId, FGameplayTag EquipSlot) = 0;
};
