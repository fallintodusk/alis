// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class IInventoryDropCommandTarget;

/**
 * Context describing the source of a drag operation that is about to
 * become a drop. Widgets fill this in at drag-start time; the router
 * uses only the tag + position + quantity - it has no knowledge of which
 * widget initiated the drag.
 */
struct FInventoryDragContext
{
    int32 InstanceId = INDEX_NONE;
    FGameplayTag SourceSurfaceTag;
    FIntPoint SourcePos = FIntPoint(-1, -1);
    bool bSourceRotated = false;
    int32 Quantity = 0;
};

/**
 * Context describing the drop destination resolved by the drag controller.
 */
struct FInventoryDropTarget
{
    FGameplayTag TargetSurfaceTag;
    FIntPoint TargetPos = FIntPoint(-1, -1);
    bool bTargetRotated = false;
};

/**
 * Tag-driven dispatcher that converts a {Source, Target} pair into the
 * correct IInventoryDropCommandTarget method call. Single source of truth
 * for which command runs on a drop, so widgets never pick commands inline.
 * The router depends on the interface, not on UInventoryViewModel, so
 * tests and future command buses can supply their own implementation.
 *
 * Routing (parent-tag matches so Item.Container.Hands covers both hands,
 * Item.Container.Pockets covers Pockets1..4):
 *   - Target matches Item.EquipmentSlot.*         -> RequestEquipItem
 *   - Target matches Item.Container.WorldStorage  -> RequestStoreItemInNearbyContainerAt
 *   - Source matches Item.Container.WorldStorage  -> RequestTakeNearbyItemToContainer
 *   - Both player-side                            -> RequestMoveItem
 *
 * Returns true iff a command was dispatched. False means the router
 * could not map the pair (invalid tag, zero quantity, etc.) - callers
 * may treat that as a silent no-op.
 */
enum class EInventoryDropRouteCommand : uint8
{
    None,
    MoveItem,
    TakeNearbyItemToContainer,
    StoreItemInNearbyContainerAt,
    MoveItemInNearbyContainer,
    EquipItem,
};

struct FInventoryDropRouteResolution
{
    EInventoryDropRouteCommand Command = EInventoryDropRouteCommand::None;
    FName VMMethod = NAME_None;

    bool IsValid() const
    {
        return Command != EInventoryDropRouteCommand::None && !VMMethod.IsNone();
    }
};

struct PROJECTINVENTORYUI_API FInventoryDropRouter
{
    /** Resolve command identity without side effects. Used for diagnostics/events. */
    static FInventoryDropRouteResolution Resolve(
        const FInventoryDragContext& Ctx,
        const FInventoryDropTarget& Target);

    static bool Route(
        IInventoryDropCommandTarget& CommandTarget,
        const FInventoryDragContext& Ctx,
        const FInventoryDropTarget& Target,
        FInventoryDropRouteResolution* OutResolution = nullptr);
};
