// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "IInventorySurfacePolicyProvider.generated.h"

struct FProjectUIGridDragPayload;

/**
 * Tag-keyed validation policy for drop surfaces registered with
 * UInventoryUIDragHostSubsystem.
 *
 * Slice 18 intent:
 *   - Replace the widget-owned OccupantAllowedChecker lambdas with a
 *     stable provider resolved by the subsystem. The previous closures
 *     captured widget-lifetime state and (pre-Slice 13 fix) reached into
 *     global Slate state via UWidgetBlueprintLibrary::GetDragDroppingContent
 *     - which returned null in lifecycle edge frames and made every empty
 *     cell appear unavailable. The IInventorySurfacePolicyProvider
 *     interface moves that decision to the domain owner (the VM) and
 *     keeps registration paths closure-free.
 *   - The surface tag doubles as the policy key. Implementers dispatch
 *     on SurfaceTag to cover primary, secondary, pocket, hand, and nearby
 *     surfaces with distinct rules.
 *
 * Fail-closed contract:
 *   - When no provider is set on the subsystem OR the provider rejects,
 *     the dispatcher emits DropRejected with RejectReason=NoPolicyProvider
 *     or RejectReason=<provider-specific>. Silent rejects are forbidden.
 *
 * Lifetime:
 *   - Subsystem stores a weak reference. A destroyed provider is treated
 *     identically to a missing provider (fail-closed).
 */
UINTERFACE(MinimalAPI, Meta = (CannotImplementInterfaceInBlueprint))
class UInventorySurfacePolicyProvider : public UInterface
{
    GENERATED_BODY()
};

class PROJECTINVENTORYUI_API IInventorySurfacePolicyProvider
{
    GENERATED_BODY()

public:
    /**
     * Return true iff Payload is allowed to land on (SurfaceTag, CellIndex)
     * whose current occupant is OccupantId (INDEX_NONE for empty cells).
     *
     * CellIndex is a flat row-major index (Y * Width + X) resolved by the
     * controller; implementers convert to grid-local coordinates through
     * surface-local width they already own (e.g. GetSecondaryGridWidth()
     * on the VM). That keeps the controller generic and the grid-math
     * local to the domain owner.
     *
     * Must be self-contained: implementers MUST NOT reach into global
     * Slate state (UWidgetBlueprintLibrary::GetDragDroppingContent, the
     * focused widget path, etc.). Everything the rule needs is in the
     * arguments plus any domain state owned by the implementer (the VM).
     *
     * Called on:
     *   - every preview frame the cursor is over a cell
     *   - the final drop resolve
     * so it must be cheap and side-effect-free.
     */
    virtual bool IsPayloadAllowedOnOccupant(
        FGameplayTag SurfaceTag,
        const FProjectUIGridDragPayload& Payload,
        int32 OccupantId,
        int32 CellIndex) const = 0;

    /**
     * Return true iff the (SurfaceTag, CellIndex) cell is currently
     * enabled for drop consideration. Controller skips disabled cells.
     *
     * Follow-up #2 (post-Slice 18): replaces widget-owned
     * EnabledChecker closures so RegisterSurface callers hand only data,
     * never capture widget/VM state. Default is "true" - i.e. the
     * surface is considered fully enabled when the provider has no
     * surface-specific rule.
     */
    virtual bool IsCellEnabledForSurface(FGameplayTag SurfaceTag, int32 CellIndex) const
    {
        (void)SurfaceTag;
        (void)CellIndex;
        return true;
    }

    /**
     * Resolve the occupant instance id at (SurfaceTag, CellIndex) or
     * return INDEX_NONE for an empty cell. Controller passes the
     * returned id to IsPayloadAllowedOnOccupant.
     *
     * Follow-up #2 (post-Slice 18): replaces widget-owned OccupantChecker
     * closures. Default is INDEX_NONE - an empty surface - which is
     * safe because IsPayloadAllowedOnOccupant then resolves via the
     * "empty or self" default.
     */
    virtual int32 GetCellOccupant(FGameplayTag SurfaceTag, int32 CellIndex) const
    {
        (void)SurfaceTag;
        (void)CellIndex;
        return INDEX_NONE;
    }
};
