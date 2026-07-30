// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "IInventoryDropTarget.generated.h"

/**
 * Marker interface declared by every UWidget class in ProjectInventoryUI
 * that is allowed to override `NativeOnDragOver` / `NativeOnDrop`.
 *
 * Purpose:
 *   - Slice 17 rule: only the smallest semantic drop target under the
 *     cursor owns the drop event. For inventory grids that is an
 *     `UW_InventoryCellDropTarget` (one per grid cell). For equip slots
 *     that will (Slice 19) be an equip-slot wrapper. Adding more drop
 *     targets in the future does not require changing the fitness test:
 *     the class just declares this interface.
 *   - Slice 20 adds a reflection-based fitness test that walks all
 *     widget classes in this module and fails if any class overrides
 *     `NativeOnDragOver` or `NativeOnDrop` without implementing this
 *     interface. That prevents silent regressions where a user-widget
 *     root sneaks back a drop override.
 *
 * The interface is intentionally empty - it is a role tag, not a vtable
 * contract. The drop dispatch contract lives in `NativeOnDragOver` /
 * `NativeOnDrop` signatures on UUserWidget / UWidget, which are engine-
 * owned.
 */
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UInventoryDropTarget : public UInterface
{
    GENERATED_BODY()
};

class PROJECTINVENTORYUI_API IInventoryDropTarget
{
    GENERATED_BODY()
};
