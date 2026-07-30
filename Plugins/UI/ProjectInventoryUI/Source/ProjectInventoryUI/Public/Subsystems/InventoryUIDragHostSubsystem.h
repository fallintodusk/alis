// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Interaction/IInventorySurfacePolicyProvider.h"
#include "Interaction/ProjectUIGridDragDropController.h"
#include "MVVM/InventoryDragEvent.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "UObject/ScriptInterface.h"
#include "UObject/WeakInterfacePtr.h"
#include "InventoryUIDragHostSubsystem.generated.h"

struct FGameplayTag;

/**
 * Per-player rendezvous for inventory-family widgets sharing a single
 * grid drag/drop controller. Both W_InventoryPanel and
 * W_NearbyContainerPanel (and any future sibling inventory surface)
 * register their grids here with stable FGameplayTag keys.
 *
 * Key architectural properties:
 *   - Subsystem outlives any single widget, so build order across layer
 *     hosts doesn't matter; there is no "register on next tick" hack.
 *   - Widgets never hold direct references to each other.
 *   - Surface identity is tag-based (Item.Container.*), not boolean.
 *   - Controller lives here once per local player - not duplicated per
 *     widget.
 *
 * Lifecycle (per widget):
 *   - NativeConstruct: RegisterSurface(...) for each owned grid.
 *   - NativeDestruct:  UnregisterSurface(Tag) for each.
 *
 * Slice 16 extensions (observability, no behavior change):
 *   - Owns the FInventoryDragSession domain state (widget-free).
 *   - BeginCellDrag / UpdatePreview / CompleteDrop / CancelDrag emit
 *     exactly one FInventoryDragEvent per transition, consumed by the
 *     test-side recorder and the optional Verbose logger. The existing
 *     dispatcher / router paths continue to run as-is; these methods
 *     wrap them so the subsystem becomes the SOT for drag decisions.
 */
UCLASS()
class PROJECTINVENTORYUI_API UInventoryUIDragHostSubsystem : public ULocalPlayerSubsystem
{
    GENERATED_BODY()

public:
    /** Underlying drag controller shared across all inventory widgets for this player. */
    FProjectUIGridDragDropController& GetController() { return Controller; }
    const FProjectUIGridDragDropController& GetController() const { return Controller; }

    /** Register (or replace) a surface keyed by SurfaceTag. Moves ownership of callbacks. */
    void RegisterSurface(FProjectUIGridSurface Surface);

    /** Remove a surface registration. No-op if not present. */
    void UnregisterSurface(const FGameplayTag& SurfaceTag);

    /** Clear all surface registrations (teardown / scope change). */
    void ClearSurfaces();

    /** Query helpers - exposed mainly for tests and diagnostics. */
    bool HasSurface(const FGameplayTag& SurfaceTag) const;
    int32 GetSurfaceCount() const;

    // -------------------------------------------------------------------
    // Slice 16 - drag session API (observability, behavior-preserving).
    // -------------------------------------------------------------------

    /**
     * Begin a drag session. Emits EInventoryDragEventKind::Started with
     * the supplied payload. Safe to call before the Slate decorator window
     * opens - the session is a pure domain object.
     */
    void BeginCellDrag(const FInventoryDragStartParams& Params);

    /**
     * Update the preview snapshot for the current session. Attempts to
     * resolve the candidate against registered surfaces via the shared
     * controller. Emits PreviewUpdated on a valid resolve, PreviewCleared
     * otherwise. No-op if no session is active.
     */
    void UpdatePreview(const FInventoryCellCandidate& Candidate, FVector2D ScreenPos);

    /**
     * Update preview for an explicit per-cell target that already received
     * the Slate drag event. Used by UW_InventoryCellDropTarget so a sibling
     * surface with stale or overlapping geometry cannot override the routed
     * wrapper identity during the same drag tick.
     */
    void UpdatePreviewAtTarget(
        const FInventoryCellCandidate& Candidate,
        const FGameplayTag& TargetSurfaceTag,
        int32 TargetCellIndex);

    /**
     * Complete the drag at the supplied screen position with the given
     * candidate context. On a valid target emits DropResolved, Routed,
     * VMInvoked (VMMethod filled from router), Completed. On invalid
     * target emits DropRejected (named reason) then Cancelled.
     *
     * Returns true iff a VM command was dispatched. Safe to hand back
     * from Slate handlers via FReply::Handled/Unhandled.
     */
    bool CompleteDrop(const FInventoryCellCandidate& Candidate, FVector2D ScreenPos);

    /**
     * Complete a drop for an explicit per-cell target that already received
     * the Slate drop event. Validation still runs through the shared
     * controller/policy-provider pipeline, but geometry re-resolution is
     * skipped because the wrapper is the authoritative semantic target.
     */
    bool CompleteDropAtTarget(
        const FInventoryCellCandidate& Candidate,
        const FGameplayTag& TargetSurfaceTag,
        int32 TargetCellIndex);

    /** Cancel the active session. Emits Cancelled. No-op if inactive. */
    void CancelDrag();

    // -------------------------------------------------------------------
    // Slice 19 - equip slots through the pipeline.
    //
    // Equip slots are not UUniformGridPanel surfaces (they sit in a
    // UVerticalBox of body-positioned cells), so they cannot piggyback
    // on the controller's geometry-based surface resolve. Each equip
    // slot is wrapped by UW_InventoryEquipSlotDropTarget, which carries
    // the target slot tag directly and calls the methods below with the
    // wrapper's identity as the authoritative target.
    //
    // The methods emit the same FInventoryDragEvent sequence that grid
    // drops do, so test-side FInventoryDragEventRecorder sees a uniform
    // stream across container and equip flows. Routing goes through the
    // same FInventoryDropRouter which, as of Slice 19, also accepts
    // Item.EquipmentSlot.* as a target.
    // -------------------------------------------------------------------

    /**
     * Update the equip preview against the active session using the
     * wrapper's slot tag as target. No controller/geometry resolution;
     * the wrapper IS the smallest semantic target.
     *
     * If the drag payload's RequiredEquipSlotTag is valid but does not
     * match SlotTag, emits PreviewCleared (reject reason stamped on the
     * session snapshot's RejectReason = EquipSlotMismatch). If the
     * required tag is unset (item is not equippable), treats as cleared
     * with reason=NotEquippable.
     */
    void UpdateEquipPreview(
        const FGameplayTag& SlotTag,
        const FGameplayTag& RequiredEquipSlotTag,
        const FInventoryCellCandidate& Candidate);

    /**
     * Complete a drop over an equip slot wrapper. Returns true iff a VM
     * command was dispatched.
     *
     * Validation is fail-closed:
     *   - Item must carry a RequiredEquipSlotTag (i.e. be equippable).
     *   - RequiredEquipSlotTag must match SlotTag.
     *   - Source surface tag must be in Item.Container.* (router rule).
     * On any failure the subsystem emits DropRejected (named reason)
     * followed by Cancelled, and returns false so the event may bubble
     * - but no further handler consumes it today because equip wrappers
     * are the only widgets registered for equip tags.
     */
    bool CompleteEquipDrop(
        const FGameplayTag& SlotTag,
        const FGameplayTag& RequiredEquipSlotTag,
        const FInventoryCellCandidate& Candidate);

    /** Read-only snapshot of the preview state (for pull-side presentation). */
    FInventoryPreviewSnapshot GetPreviewSnapshot() const;

    /** Structured event stream. See FInventoryDragEvent for contract. */
    FOnInventoryDragEvent OnDragEvent;

    // -------------------------------------------------------------------
    // Slice 18 - tag-keyed policy provider (widget closures banned).
    //
    // Widgets used to supply per-surface OccupantAllowedChecker lambdas
    // that captured weak-VM state (and, pre-Slice 13, reached into global
    // Slate state via UWidgetBlueprintLibrary::GetDragDroppingContent).
    // The subsystem now resolves drop validity through an
    // IInventorySurfacePolicyProvider (typically the active VM). Surface
    // registration becomes a data-only operation.
    //
    // When a surface is registered without a pre-supplied
    // OccupantAllowedChecker and a provider is set, the subsystem injects
    // a closure (inside its own .cpp, NOT widget code) that dispatches to
    // IInventorySurfacePolicyProvider::IsPayloadAllowedOnOccupant. The
    // closure captures a weak reference to the provider UObject and the
    // surface tag - zero widget-owned state.
    //
    // Fail-closed contract: if no provider is wired when a surface is
    // registered, the surface keeps the generic "empty-or-self" default.
    // If the drop dispatcher cannot resolve a target, it emits
    // DropRejected with RejectReason = NoPolicyProvider so silent rejects
    // are impossible to swallow.
    // -------------------------------------------------------------------

    /**
     * Bind the tag-keyed policy provider used by dispatcher-driven drop
     * validation. Pass nullptr to clear. Safe to call multiple times
     * (e.g. on VM swap) - the subsystem re-reads the provider lazily, so
     * surfaces registered before the provider is set pick it up.
     */
    void SetPolicyProvider(TScriptInterface<IInventorySurfacePolicyProvider> InProvider);

    /**
     * Live accessor. Returns null if no provider is bound OR if the
     * previously-bound provider has been destroyed. Both cases must be
     * handled fail-closed by callers.
     */
    IInventorySurfacePolicyProvider* GetPolicyProvider() const;

protected:
    virtual void Deinitialize() override;

private:
    /** Broadcast an event and stamp the timestamp on the way out. */
    void EmitEvent(FInventoryDragEvent Event);

    /** Populate event common fields from the active session. */
    void FillEventFromSession(FInventoryDragEvent& OutEvent) const;

    /** Apply the current preview snapshot and emit PreviewUpdated/Cleared. */
    void ApplyPreviewResolution(
        bool bResolved,
        const FGameplayTag& TargetTag,
        int32 Col,
        int32 Row);

    /** Route a previously-resolved container-cell target through the VM. */
    bool CompleteResolvedDrop(
        const FInventoryCellCandidate& Candidate,
        bool bResolved,
        const FGameplayTag& TargetTag,
        int32 Col,
        int32 Row,
        const FName& RejectReasonWhenUnresolved);

    /**
     * When Surface lacks an OccupantAllowedChecker, install a checker that
     * forwards to the policy provider. The closure lives in the subsystem
     * .cpp (NOT widget code) and captures only the surface tag plus a weak
     * pointer to this subsystem - it never closes over widget state.
     */
    void InstallPolicyCheckerIfNeeded(FProjectUIGridSurface& Surface);

    FProjectUIGridDragDropController Controller;

    FInventoryDragSession Session;

    /** Weak; destroyed provider is equivalent to "no provider". */
    TWeakObjectPtr<UObject> PolicyProviderObject;
};
