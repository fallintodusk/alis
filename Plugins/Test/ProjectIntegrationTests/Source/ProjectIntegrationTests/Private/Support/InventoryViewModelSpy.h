// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MVVM/InventoryViewModel.h"
#include "InventoryViewModelSpy.generated.h"

/**
 * Test-only spy over UInventoryViewModel that intercepts the three
 * methods FInventoryDropRouter dispatches to and records which one was
 * called plus the arguments received. Never forwards to Super:: because
 * the router tests run in-process without a live inventory source and
 * we do not want the VM attempting real transfers or RPCs.
 */
UCLASS()
class UInventoryViewModelSpy : public UInventoryViewModel
{
    GENERATED_BODY()

public:
    enum class ELastCall : uint8
    {
        None,
        MoveItem,
        TakeNearbyItemToContainer,
        StoreItemInNearbyContainerAt,
        MoveItemInNearbyContainer,
        EquipItem,
    };

    ELastCall LastCall = ELastCall::None;

    int32 LastInstanceId = INDEX_NONE;
    int32 LastQuantity = 0;
    bool LastRotated = false;

    FGameplayTag LastFromContainer;
    FIntPoint LastFromPos = FIntPoint(-1, -1);

    FGameplayTag LastToContainer;
    FIntPoint LastToPos = FIntPoint(-1, -1);

    virtual void RequestMoveItem(
        int32 InstanceId,
        FGameplayTag FromContainer,
        FIntPoint FromPos,
        FGameplayTag ToContainer,
        FIntPoint ToPos,
        int32 Quantity,
        bool bRotated) override;

    virtual void RequestTakeNearbyItemToContainer(
        int32 InstanceId,
        FGameplayTag TargetContainerId,
        FIntPoint TargetGridPos,
        bool bTargetRotated,
        int32 Quantity = 1) override;

    virtual void RequestStoreItemInNearbyContainerAt(
        int32 InstanceId,
        FIntPoint TargetGridPos,
        bool bTargetRotated,
        int32 Quantity = 1) override;

    virtual void RequestMoveItemInNearbyContainer(
        int32 InstanceId,
        FIntPoint TargetGridPos,
        bool bTargetRotated,
        int32 Quantity = 1) override;

    virtual void RequestEquipItem(int32 InstanceId, FGameplayTag EquipSlot) override;

    // Follow-up #2: the spy does not carry real VM cell state, so it
    // answers the tag-keyed policy provider queries with permissive
    // defaults. Tests that need specific occupant/enabled behavior can
    // still register a custom OccupantChecker/EnabledChecker on their
    // FProjectUIGridSurface and the subsystem will respect the explicit
    // value (InstallPolicyCheckerIfNeeded only fills missing slots).
    virtual bool IsCellEnabledForSurface(FGameplayTag /*SurfaceTag*/, int32 /*CellIndex*/) const override
    {
        return true;
    }

    virtual int32 GetCellOccupant(FGameplayTag /*SurfaceTag*/, int32 /*CellIndex*/) const override
    {
        return INDEX_NONE;
    }
};
