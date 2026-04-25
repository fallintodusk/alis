// Copyright ALIS. All Rights Reserved.

#include "MVVM/InventoryDropRouter.h"

#include "Interaction/IInventoryDropCommandTarget.h"
#include "ProjectGameplayTags.h"

FInventoryDropRouteResolution FInventoryDropRouter::Resolve(
    const FInventoryDragContext& Ctx,
    const FInventoryDropTarget& Target)
{
    FInventoryDropRouteResolution Out;

    if (Ctx.InstanceId == INDEX_NONE || Ctx.Quantity <= 0)
    {
        return Out;
    }

    const FGameplayTag& Container = ProjectTags::Item_Container;
    const FGameplayTag& EquipmentSlot = ProjectTags::Item_EquipmentSlot;
    const bool bSourceInContainer = Ctx.SourceSurfaceTag.IsValid() && Ctx.SourceSurfaceTag.MatchesTag(Container);
    const bool bTargetInContainer = Target.TargetSurfaceTag.IsValid() && Target.TargetSurfaceTag.MatchesTag(Container);
    const bool bTargetInEquip = Target.TargetSurfaceTag.IsValid() && Target.TargetSurfaceTag.MatchesTag(EquipmentSlot);
    if (!bSourceInContainer)
    {
        return Out;
    }
    if (!bTargetInContainer && !bTargetInEquip)
    {
        return Out;
    }

    if (bTargetInEquip)
    {
        Out.Command = EInventoryDropRouteCommand::EquipItem;
        Out.VMMethod = FName(TEXT("RequestEquipItem"));
        return Out;
    }

    const FGameplayTag& WorldStorage = ProjectTags::Item_Container_WorldStorage;
    const bool bTargetIsWorld = Target.TargetSurfaceTag.MatchesTag(WorldStorage);
    const bool bSourceIsWorld = Ctx.SourceSurfaceTag.MatchesTag(WorldStorage);

    if (bTargetIsWorld && !bSourceIsWorld)
    {
        Out.Command = EInventoryDropRouteCommand::StoreItemInNearbyContainerAt;
        Out.VMMethod = FName(TEXT("RequestStoreItemInNearbyContainerAt"));
        return Out;
    }

    if (bSourceIsWorld && !bTargetIsWorld)
    {
        Out.Command = EInventoryDropRouteCommand::TakeNearbyItemToContainer;
        Out.VMMethod = FName(TEXT("RequestTakeNearbyItemToContainer"));
        return Out;
    }

    if (bSourceIsWorld && bTargetIsWorld)
    {
        // Rearrange within the same nearby world container. Previously
        // returned silently-empty, which left in-container drag/drop
        // dead even though drag-start succeeded - a real UX break the
        // synthetic drop tests did not catch because they only exercised
        // cross-surface pairs.
        Out.Command = EInventoryDropRouteCommand::MoveItemInNearbyContainer;
        Out.VMMethod = FName(TEXT("RequestMoveItemInNearbyContainer"));
        return Out;
    }

    Out.Command = EInventoryDropRouteCommand::MoveItem;
    Out.VMMethod = FName(TEXT("RequestMoveItem"));
    return Out;
}

bool FInventoryDropRouter::Route(
    IInventoryDropCommandTarget& CommandTarget,
    const FInventoryDragContext& Ctx,
    const FInventoryDropTarget& Target,
    FInventoryDropRouteResolution* OutResolution)
{
    const FInventoryDropRouteResolution Resolution = Resolve(Ctx, Target);
    if (OutResolution)
    {
        *OutResolution = Resolution;
    }
    if (!Resolution.IsValid())
    {
        return false;
    }

    switch (Resolution.Command)
    {
    case EInventoryDropRouteCommand::EquipItem:
        CommandTarget.RequestEquipItem(Ctx.InstanceId, Target.TargetSurfaceTag);
        return true;

    case EInventoryDropRouteCommand::StoreItemInNearbyContainerAt:
        CommandTarget.RequestStoreItemInNearbyContainerAt(
            Ctx.InstanceId,
            Target.TargetPos,
            Target.bTargetRotated,
            Ctx.Quantity);
        return true;

    case EInventoryDropRouteCommand::MoveItemInNearbyContainer:
        CommandTarget.RequestMoveItemInNearbyContainer(
            Ctx.InstanceId,
            Target.TargetPos,
            Target.bTargetRotated,
            Ctx.Quantity);
        return true;

    case EInventoryDropRouteCommand::TakeNearbyItemToContainer:
        CommandTarget.RequestTakeNearbyItemToContainer(
            Ctx.InstanceId,
            Target.TargetSurfaceTag,
            Target.TargetPos,
            Target.bTargetRotated,
            Ctx.Quantity);
        return true;

    case EInventoryDropRouteCommand::MoveItem:
        CommandTarget.RequestMoveItem(
            Ctx.InstanceId,
            Ctx.SourceSurfaceTag,
            Ctx.SourcePos,
            Target.TargetSurfaceTag,
            Target.TargetPos,
            Ctx.Quantity,
            Target.bTargetRotated);
        return true;

    default:
        return false;
    }
}
