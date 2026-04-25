// Copyright ALIS. All Rights Reserved.

#include "Support/InventoryViewModelSpy.h"

void UInventoryViewModelSpy::RequestMoveItem(
    int32 InstanceId,
    FGameplayTag FromContainer,
    FIntPoint FromPos,
    FGameplayTag ToContainer,
    FIntPoint ToPos,
    int32 Quantity,
    bool bRotated)
{
    LastCall = ELastCall::MoveItem;
    LastInstanceId = InstanceId;
    LastFromContainer = FromContainer;
    LastFromPos = FromPos;
    LastToContainer = ToContainer;
    LastToPos = ToPos;
    LastQuantity = Quantity;
    LastRotated = bRotated;
    // Intentionally does NOT call Super:: - the spy suppresses real
    // inventory writes to keep the router test hermetic.
}

void UInventoryViewModelSpy::RequestTakeNearbyItemToContainer(
    int32 InstanceId,
    FGameplayTag TargetContainerId,
    FIntPoint TargetGridPos,
    bool bTargetRotated,
    int32 Quantity)
{
    LastCall = ELastCall::TakeNearbyItemToContainer;
    LastInstanceId = InstanceId;
    LastToContainer = TargetContainerId;
    LastToPos = TargetGridPos;
    LastRotated = bTargetRotated;
    LastQuantity = Quantity;
}

void UInventoryViewModelSpy::RequestStoreItemInNearbyContainerAt(
    int32 InstanceId,
    FIntPoint TargetGridPos,
    bool bTargetRotated,
    int32 Quantity)
{
    LastCall = ELastCall::StoreItemInNearbyContainerAt;
    LastInstanceId = InstanceId;
    LastToPos = TargetGridPos;
    LastRotated = bTargetRotated;
    LastQuantity = Quantity;
}

void UInventoryViewModelSpy::RequestMoveItemInNearbyContainer(
    int32 InstanceId,
    FIntPoint TargetGridPos,
    bool bTargetRotated,
    int32 Quantity)
{
    LastCall = ELastCall::MoveItemInNearbyContainer;
    LastInstanceId = InstanceId;
    LastToPos = TargetGridPos;
    LastRotated = bTargetRotated;
    LastQuantity = Quantity;
}

void UInventoryViewModelSpy::RequestEquipItem(int32 InstanceId, FGameplayTag EquipSlot)
{
    LastCall = ELastCall::EquipItem;
    LastInstanceId = InstanceId;
    LastToContainer = EquipSlot;
}
