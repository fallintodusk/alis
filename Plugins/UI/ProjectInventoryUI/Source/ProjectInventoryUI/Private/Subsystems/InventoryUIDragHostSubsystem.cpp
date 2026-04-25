// Copyright ALIS. All Rights Reserved.

#include "Subsystems/InventoryUIDragHostSubsystem.h"

#include "Diagnostics/InventoryDragLogger.h"
#include "GameplayTagContainer.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Interaction/IInventorySurfacePolicyProvider.h"
#include "Logging/LogMacros.h"
#include "Interaction/IInventoryDropCommandTarget.h"
#include "MVVM/InventoryDropRouter.h"
#include "ProjectGameplayTags.h"
#include "Slice20SabotageToggle.h"

DEFINE_LOG_CATEGORY_STATIC(LogInventoryUIDragHost, Log, All);

// Single-source-of-truth dispatch: Subsystem::CompleteDrop owns the only
// grid-drop path. It calls FInventoryDropRouter::Route, which returns the
// actually-invoked VM method via FInventoryDropRouteResolution. Event
// stream reports what truly happened, not a predicted label.

namespace
{
    bool IsInventoryDragDiagEnabled()
    {
#if !UE_BUILD_SHIPPING
        if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("inv.drag.diag")))
        {
            return Var->GetInt() != 0;
        }
#endif
        return false;
    }

    FString DescribeObject(const UObject* Object)
    {
        if (!Object)
        {
            return TEXT("null");
        }

        return FString::Printf(
            TEXT("%s (%s)"),
            *Object->GetPathName(),
            *Object->GetClass()->GetName());
    }

    FString DescribeCandidate(const FInventoryCellCandidate& Candidate)
    {
        return FString::Printf(
            TEXT("Instance=%d Src=%s Pos=(%d,%d) Qty=%d Rot=%d Size=%dx%d"),
            Candidate.InstanceId,
            *Candidate.SourceSurfaceTag.ToString(),
            Candidate.SourcePos.X,
            Candidate.SourcePos.Y,
            Candidate.Quantity,
            Candidate.bRotated ? 1 : 0,
            Candidate.ItemSize.X,
            Candidate.ItemSize.Y);
    }

    FProjectUIGridDragPayload MakePayload(const FInventoryCellCandidate& Candidate)
    {
        FProjectUIGridDragPayload Payload;
        Payload.InstanceId = Candidate.InstanceId;
        Payload.ItemSize = Candidate.ItemSize;
        Payload.Quantity = Candidate.Quantity;
        Payload.SourceSurfaceTag = Candidate.SourceSurfaceTag;
        return Payload;
    }

    void InstallOptionalDragLogger(UInventoryUIDragHostSubsystem* Subsystem)
    {
#if !UE_BUILD_SHIPPING
        FInventoryDragLogger::Install(Subsystem);
#else
        (void)Subsystem;
#endif
    }
}

void UInventoryUIDragHostSubsystem::RegisterSurface(FProjectUIGridSurface Surface)
{
    InstallOptionalDragLogger(this);
    InstallPolicyCheckerIfNeeded(Surface);
    Controller.RegisterSurface(MoveTemp(Surface));
}

void UInventoryUIDragHostSubsystem::SetPolicyProvider(
    TScriptInterface<IInventorySurfacePolicyProvider> InProvider)
{
    InstallOptionalDragLogger(this);

    UObject* OldProvider = PolicyProviderObject.Get();
    UObject* NewProvider = InProvider.GetObject();
    PolicyProviderObject = NewProvider;

    if (IsInventoryDragDiagEnabled() || (OldProvider && NewProvider && OldProvider != NewProvider))
    {
        UE_LOG(
            LogInventoryUIDragHost,
            Log,
            TEXT("SetPolicyProvider: %s -> %s"),
            *DescribeObject(OldProvider),
            *DescribeObject(NewProvider));
    }

    if (OldProvider && NewProvider && OldProvider != NewProvider
        && OldProvider->GetClass() == NewProvider->GetClass())
    {
        UE_LOG(
            LogInventoryUIDragHost,
            Warning,
            TEXT("SetPolicyProvider: switched between distinct provider objects of the same class. If drag starts in one widget and drops in another, confirm both panels share the same global InventoryViewModel instance."));
    }
}

IInventorySurfacePolicyProvider* UInventoryUIDragHostSubsystem::GetPolicyProvider() const
{
    UObject* Object = PolicyProviderObject.Get();
    if (!Object)
    {
        return nullptr;
    }
    return Cast<IInventorySurfacePolicyProvider>(Object);
}

void UInventoryUIDragHostSubsystem::InstallPolicyCheckerIfNeeded(FProjectUIGridSurface& Surface)
{
    // Slice 18 + Follow-up #2: widgets supply data only on surface
    // registration - the subsystem fills in all three controller checkers
    // that previously captured widget/VM state. Tests that register
    // custom checkers keep their overrides because we never overwrite an
    // explicitly-supplied lambda.
    const FGameplayTag SurfaceTag = Surface.SurfaceTag;
    TWeakObjectPtr<UInventoryUIDragHostSubsystem> WeakSelf(this);

    // Note: all three closures below live in the subsystem's .cpp and
    // capture only (a) a weak pointer to this subsystem and (b) the
    // surface tag. They do NOT capture any widget state. Follow-up #2's
    // tightened fitness test (captures anywhere near RegisterSurface in
    // widget code) is satisfied because widgets set no lambda at all.
    if (!Surface.OccupantAllowedChecker)
    {
        Surface.OccupantAllowedChecker =
            [WeakSelf, SurfaceTag](int32 CellIndex, int32 OccupantId, const FProjectUIGridDragPayload& Payload) -> bool
            {
                UInventoryUIDragHostSubsystem* Self = WeakSelf.Get();
                IInventorySurfacePolicyProvider* Provider = Self ? Self->GetPolicyProvider() : nullptr;
                if (!Provider)
                {
                    // Fail-closed for occupied cells; empty/self still accepted
                    // so widgets whose tabbed VM is not yet bound do not lose
                    // drag-to-empty during startup.
                    return OccupantId == INDEX_NONE || OccupantId == Payload.InstanceId;
                }

                return Provider->IsPayloadAllowedOnOccupant(
                    SurfaceTag, Payload, OccupantId, CellIndex);
            };
    }

    if (!Surface.EnabledChecker)
    {
        Surface.EnabledChecker =
            [WeakSelf, SurfaceTag](int32 CellIndex) -> bool
            {
                UInventoryUIDragHostSubsystem* Self = WeakSelf.Get();
                IInventorySurfacePolicyProvider* Provider = Self ? Self->GetPolicyProvider() : nullptr;
                if (!Provider)
                {
                    // No provider: fall back to the provider interface's
                    // default ("enabled") - preserves drag-to-empty during
                    // early lifecycle frames before VM bind.
                    return true;
                }
                return Provider->IsCellEnabledForSurface(SurfaceTag, CellIndex);
            };
    }

    if (!Surface.OccupantChecker)
    {
        Surface.OccupantChecker =
            [WeakSelf, SurfaceTag](int32 CellIndex) -> int32
            {
                UInventoryUIDragHostSubsystem* Self = WeakSelf.Get();
                IInventorySurfacePolicyProvider* Provider = Self ? Self->GetPolicyProvider() : nullptr;
                if (!Provider)
                {
                    return INDEX_NONE;
                }
                return Provider->GetCellOccupant(SurfaceTag, CellIndex);
            };
    }
}

void UInventoryUIDragHostSubsystem::UnregisterSurface(const FGameplayTag& SurfaceTag)
{
    if (SurfaceTag.IsValid() && !Controller.HasSurface(SurfaceTag))
    {
        // Double-unregister or typo in the tag. Surface a warning in dev so
        // widget lifecycle bugs do not hide behind "safe" no-ops.
        UE_LOG(LogInventoryUIDragHost, Warning,
            TEXT("UnregisterSurface called for '%s' but it is not registered."),
            *SurfaceTag.ToString());
    }
    Controller.UnregisterSurface(SurfaceTag);
}

void UInventoryUIDragHostSubsystem::ClearSurfaces()
{
    Controller.ClearSurfaces();
}

bool UInventoryUIDragHostSubsystem::HasSurface(const FGameplayTag& SurfaceTag) const
{
    return Controller.HasSurface(SurfaceTag);
}

int32 UInventoryUIDragHostSubsystem::GetSurfaceCount() const
{
    return Controller.GetSurfaceCount();
}

void UInventoryUIDragHostSubsystem::Deinitialize()
{
    Controller.ClearSurfaces();
    Session = FInventoryDragSession();
    Super::Deinitialize();
}

void UInventoryUIDragHostSubsystem::BeginCellDrag(const FInventoryDragStartParams& Params)
{
    InstallOptionalDragLogger(this);

    Session = FInventoryDragSession();
    Session.SourceSurfaceTag = Params.SourceTag;
    Session.SourceCell = Params.SourceCell;
    Session.InstanceId = Params.InstanceId;
    Session.Quantity = Params.Quantity;
    Session.bRotated = Params.bRotated;
    Session.bActive = true;

    if (IsInventoryDragDiagEnabled())
    {
        UE_LOG(
            LogInventoryUIDragHost,
            Log,
            TEXT("BeginCellDrag: Src=%s Pos=(%d,%d) Instance=%d Qty=%d Rot=%d Provider=%s"),
            *Params.SourceTag.ToString(),
            Params.SourceCell.X,
            Params.SourceCell.Y,
            Params.InstanceId,
            Params.Quantity,
            Params.bRotated ? 1 : 0,
            *DescribeObject(PolicyProviderObject.Get()));
    }

    FInventoryDragEvent Event;
    Event.Kind = EInventoryDragEventKind::Started;
    FillEventFromSession(Event);
    EmitEvent(MoveTemp(Event));
}

void UInventoryUIDragHostSubsystem::UpdatePreview(const FInventoryCellCandidate& Candidate, FVector2D ScreenPos)
{
    InstallOptionalDragLogger(this);

    if (!Session.bActive)
    {
        if (IsInventoryDragDiagEnabled())
        {
            UE_LOG(
                LogInventoryUIDragHost,
                Warning,
                TEXT("UpdatePreview ignored: no active session. Screen=(%.1f,%.1f) Candidate={%s}"),
                ScreenPos.X,
                ScreenPos.Y,
                *DescribeCandidate(Candidate));
        }
        return;
    }

    // Follow-up #3: candidate-driven rotation. Mid-drag rotation updates
    // flow in via Candidate.bRotated; CompleteDrop reads Session.bRotated
    // when building the router target, so keep them in sync here. The
    // session remains the SOT so an UpdatePreview with mismatched
    // bRotated cannot silently revert a prior rotation toggle.
    Session.bRotated = Candidate.bRotated;

    const FProjectUIGridDragPayload Payload = MakePayload(Candidate);

    FGameplayTag TargetTag;
    int32 Col = INDEX_NONE;
    int32 Row = INDEX_NONE;
    const bool bResolved = Controller.ResolveDropTargetOverSurfaces(ScreenPos, Payload, TargetTag, Col, Row);

    if (IsInventoryDragDiagEnabled())
    {
        UE_LOG(
            LogInventoryUIDragHost,
            Log,
            TEXT("UpdatePreview: Screen=(%.1f,%.1f) Candidate={%s} Resolved=%d Target=%s Cell=(%d,%d) Provider=%s"),
            ScreenPos.X,
            ScreenPos.Y,
            *DescribeCandidate(Candidate),
            bResolved ? 1 : 0,
            *TargetTag.ToString(),
            Col,
            Row,
            *DescribeObject(PolicyProviderObject.Get()));
    }

    ApplyPreviewResolution(bResolved, TargetTag, Col, Row);
}

void UInventoryUIDragHostSubsystem::UpdatePreviewAtTarget(
    const FInventoryCellCandidate& Candidate,
    const FGameplayTag& TargetSurfaceTag,
    int32 TargetCellIndex)
{
    InstallOptionalDragLogger(this);

    if (!Session.bActive)
    {
        if (IsInventoryDragDiagEnabled())
        {
            UE_LOG(
                LogInventoryUIDragHost,
                Warning,
                TEXT("UpdatePreviewAtTarget ignored: no active session. Target=%s CellIndex=%d Candidate={%s}"),
                *TargetSurfaceTag.ToString(),
                TargetCellIndex,
                *DescribeCandidate(Candidate));
        }
        return;
    }

    Session.bRotated = Candidate.bRotated;

    const FProjectUIGridDragPayload Payload = MakePayload(Candidate);

    Controller.UpdatePreviewOnSurface(TargetSurfaceTag, TargetCellIndex, Payload);

    int32 Col = INDEX_NONE;
    int32 Row = INDEX_NONE;
    const bool bResolved = Controller.ResolveDropTargetOnSurface(TargetSurfaceTag, TargetCellIndex, Payload, Col, Row);

    if (IsInventoryDragDiagEnabled())
    {
        UE_LOG(
            LogInventoryUIDragHost,
            Log,
            TEXT("UpdatePreviewAtTarget: Target=%s CellIndex=%d Candidate={%s} Resolved=%d Cell=(%d,%d) Provider=%s"),
            *TargetSurfaceTag.ToString(),
            TargetCellIndex,
            *DescribeCandidate(Candidate),
            bResolved ? 1 : 0,
            Col,
            Row,
            *DescribeObject(PolicyProviderObject.Get()));
    }

    ApplyPreviewResolution(bResolved, TargetSurfaceTag, Col, Row);
}

bool UInventoryUIDragHostSubsystem::CompleteDrop(const FInventoryCellCandidate& Candidate, FVector2D ScreenPos)
{
    InstallOptionalDragLogger(this);

    if (!Session.bActive)
    {
        if (IsInventoryDragDiagEnabled())
        {
            UE_LOG(
                LogInventoryUIDragHost,
                Warning,
                TEXT("CompleteDrop ignored: no active session. Screen=(%.1f,%.1f) Candidate={%s}"),
                ScreenPos.X,
                ScreenPos.Y,
                *DescribeCandidate(Candidate));
        }
        return false;
    }

    const FProjectUIGridDragPayload Payload = MakePayload(Candidate);

    FGameplayTag TargetTag;
    int32 Col = INDEX_NONE;
    int32 Row = INDEX_NONE;
    const bool bResolved = Controller.ResolveDropTargetOverSurfaces(ScreenPos, Payload, TargetTag, Col, Row);

    if (IsInventoryDragDiagEnabled())
    {
        UE_LOG(
            LogInventoryUIDragHost,
            Log,
            TEXT("CompleteDrop: Screen=(%.1f,%.1f) Candidate={%s} Resolved=%d Target=%s Cell=(%d,%d) Provider=%s"),
            ScreenPos.X,
            ScreenPos.Y,
            *DescribeCandidate(Candidate),
            bResolved ? 1 : 0,
            *TargetTag.ToString(),
            Col,
            Row,
            *DescribeObject(PolicyProviderObject.Get()));
    }

    return CompleteResolvedDrop(
        Candidate,
        bResolved,
        TargetTag,
        Col,
        Row,
        FName(TEXT("NoTargetUnderCursor")));
}

bool UInventoryUIDragHostSubsystem::CompleteDropAtTarget(
    const FInventoryCellCandidate& Candidate,
    const FGameplayTag& TargetSurfaceTag,
    int32 TargetCellIndex)
{
    InstallOptionalDragLogger(this);

    if (!Session.bActive)
    {
        if (IsInventoryDragDiagEnabled())
        {
            UE_LOG(
                LogInventoryUIDragHost,
                Warning,
                TEXT("CompleteDropAtTarget ignored: no active session. Target=%s CellIndex=%d Candidate={%s}"),
                *TargetSurfaceTag.ToString(),
                TargetCellIndex,
                *DescribeCandidate(Candidate));
        }
        return false;
    }

    const FProjectUIGridDragPayload Payload = MakePayload(Candidate);
    int32 Col = INDEX_NONE;
    int32 Row = INDEX_NONE;
    const bool bResolved = Controller.ResolveDropTargetOnSurface(TargetSurfaceTag, TargetCellIndex, Payload, Col, Row);

    if (IsInventoryDragDiagEnabled())
    {
        UE_LOG(
            LogInventoryUIDragHost,
            Log,
            TEXT("CompleteDropAtTarget: Target=%s CellIndex=%d Candidate={%s} Resolved=%d Cell=(%d,%d) Provider=%s"),
            *TargetSurfaceTag.ToString(),
            TargetCellIndex,
            *DescribeCandidate(Candidate),
            bResolved ? 1 : 0,
            Col,
            Row,
            *DescribeObject(PolicyProviderObject.Get()));
    }

    return CompleteResolvedDrop(
        Candidate,
        bResolved,
        TargetSurfaceTag,
        Col,
        Row,
        FName(TEXT("DirectTargetRejected")));
}

void UInventoryUIDragHostSubsystem::ApplyPreviewResolution(
    bool bResolved,
    const FGameplayTag& TargetTag,
    int32 Col,
    int32 Row)
{
    if (bResolved)
    {
        Session.Preview.TargetTag = TargetTag;
        Session.Preview.TargetCell = FIntPoint(Col, Row);
        Session.Preview.bValid = true;
        Session.Preview.RejectReason = NAME_None;

        FInventoryDragEvent Event;
        Event.Kind = EInventoryDragEventKind::PreviewUpdated;
        FillEventFromSession(Event);
        Event.TargetTag = TargetTag;
        Event.TargetCell = FIntPoint(Col, Row);
        EmitEvent(MoveTemp(Event));
    }
    else
    {
        Session.Preview = FInventoryPreviewSnapshot();

        FInventoryDragEvent Event;
        Event.Kind = EInventoryDragEventKind::PreviewCleared;
        FillEventFromSession(Event);
        EmitEvent(MoveTemp(Event));
    }
}

bool UInventoryUIDragHostSubsystem::CompleteResolvedDrop(
    const FInventoryCellCandidate& Candidate,
    bool bResolved,
    const FGameplayTag& TargetTag,
    int32 Col,
    int32 Row,
    const FName& RejectReasonWhenUnresolved)
{
    if (!bResolved)
    {
        {
            FInventoryDragEvent Event;
            Event.Kind = EInventoryDragEventKind::DropRejected;
            FillEventFromSession(Event);
            Event.RejectReason = RejectReasonWhenUnresolved;
            EmitEvent(MoveTemp(Event));
        }
        {
            FInventoryDragEvent Event;
            Event.Kind = EInventoryDragEventKind::Cancelled;
            FillEventFromSession(Event);
            EmitEvent(MoveTemp(Event));
        }
        Session = FInventoryDragSession();
        Controller.ClearPreview();
        return false;
    }

    Session.Preview.TargetTag = TargetTag;
    Session.Preview.TargetCell = FIntPoint(Col, Row);
    Session.Preview.bValid = true;
    Session.Preview.RejectReason = NAME_None;

    // DropResolved: we have target geometry. Emit BEFORE attempting
    // dispatch so the stream always records the resolve step.
    {
        FInventoryDragEvent Event;
        Event.Kind = EInventoryDragEventKind::DropResolved;
        FillEventFromSession(Event);
        Event.TargetTag = TargetTag;
        Event.TargetCell = FIntPoint(Col, Row);
        EmitEvent(MoveTemp(Event));
    }

    // Dispatch REAL routing via FInventoryDropRouter::Route. The command
    // target is the policy provider bound via SetPolicyProvider (Slice 18).
    // Subsystem no longer depends on UInventoryViewModel concretely --
    // only on the narrow IInventoryDropCommandTarget interface. Fail-loud
    // if nothing is bound.
    IInventoryDropCommandTarget* CommandTarget =
        Cast<IInventoryDropCommandTarget>(PolicyProviderObject.Get());
    if (!CommandTarget)
    {
        UE_LOG(
            LogInventoryUIDragHost,
            Warning,
            TEXT("CompleteDrop: resolved target %s (%d,%d) but active provider is not an IInventoryDropCommandTarget: %s"),
            *TargetTag.ToString(),
            Col,
            Row,
            *DescribeObject(PolicyProviderObject.Get()));
        {
            FInventoryDragEvent Event;
            Event.Kind = EInventoryDragEventKind::DropRejected;
            FillEventFromSession(Event);
            Event.TargetTag = TargetTag;
            Event.TargetCell = FIntPoint(Col, Row);
            Event.RejectReason = FName(TEXT("NoCommandTarget"));
            EmitEvent(MoveTemp(Event));
        }
        {
            FInventoryDragEvent Event;
            Event.Kind = EInventoryDragEventKind::Cancelled;
            FillEventFromSession(Event);
            EmitEvent(MoveTemp(Event));
        }
        Session = FInventoryDragSession();
        Controller.ClearPreview();
        return false;
    }

    FInventoryDragContext Ctx;
    Ctx.InstanceId = Candidate.InstanceId;
    Ctx.SourceSurfaceTag = Candidate.SourceSurfaceTag;
    Ctx.SourcePos = Session.SourceCell;
    Ctx.bSourceRotated = Session.bRotated;
    Ctx.Quantity = Candidate.Quantity;

    FInventoryDropTarget RouterTarget;
    RouterTarget.TargetSurfaceTag = TargetTag;
    RouterTarget.TargetPos = FIntPoint(Col, Row);
    RouterTarget.bTargetRotated = Session.bRotated;

    FInventoryDropRouteResolution Resolution;
    const bool bDispatched = FInventoryDropRouter::Route(*CommandTarget, Ctx, RouterTarget, &Resolution);

    if (IsInventoryDragDiagEnabled())
    {
        UE_LOG(
            LogInventoryUIDragHost,
            Log,
            TEXT("CompleteDrop: router dispatched=%d resolutionValid=%d vm=%s target=%s Cell=(%d,%d)"),
            bDispatched ? 1 : 0,
            Resolution.IsValid() ? 1 : 0,
            *Resolution.VMMethod.ToString(),
            *TargetTag.ToString(),
            Col,
            Row);
    }

    if (!bDispatched || !Resolution.IsValid())
    {
        // Router refused the pair (nearby->nearby, out-of-domain, zero qty, etc.).
        {
            FInventoryDragEvent Event;
            Event.Kind = EInventoryDragEventKind::DropRejected;
            FillEventFromSession(Event);
            Event.TargetTag = TargetTag;
            Event.TargetCell = FIntPoint(Col, Row);
            Event.RejectReason = FName(TEXT("RouterDomainReject"));
            EmitEvent(MoveTemp(Event));
        }
        {
            FInventoryDragEvent Event;
            Event.Kind = EInventoryDragEventKind::Cancelled;
            FillEventFromSession(Event);
            EmitEvent(MoveTemp(Event));
        }
        Session = FInventoryDragSession();
        Controller.ClearPreview();
        return false;
    }

    // Router dispatched: emit Routed -> VMInvoked{actual method} -> Completed.
    {
        FInventoryDragEvent Event;
        Event.Kind = EInventoryDragEventKind::Routed;
        FillEventFromSession(Event);
        Event.TargetTag = TargetTag;
        Event.TargetCell = FIntPoint(Col, Row);
        EmitEvent(MoveTemp(Event));
    }
    {
        FInventoryDragEvent Event;
        Event.Kind = EInventoryDragEventKind::VMInvoked;
        FillEventFromSession(Event);
        Event.TargetTag = TargetTag;
        Event.TargetCell = FIntPoint(Col, Row);
        Event.VMMethod = Resolution.VMMethod;
        EmitEvent(MoveTemp(Event));
    }
    {
        FInventoryDragEvent Event;
        Event.Kind = EInventoryDragEventKind::Completed;
        FillEventFromSession(Event);
        Event.TargetTag = TargetTag;
        Event.TargetCell = FIntPoint(Col, Row);
        EmitEvent(MoveTemp(Event));
    }

    Session = FInventoryDragSession();
    Controller.ClearPreview();
    return true;
}

void UInventoryUIDragHostSubsystem::CancelDrag()
{
    if (!Session.bActive)
    {
        if (IsInventoryDragDiagEnabled())
        {
            UE_LOG(LogInventoryUIDragHost, Warning, TEXT("CancelDrag ignored: no active session"));
        }
        return;
    }

    if (IsInventoryDragDiagEnabled())
    {
        UE_LOG(
            LogInventoryUIDragHost,
            Warning,
            TEXT("CancelDrag: Source=%s Cell=(%d,%d) Instance=%d Qty=%d PreviewValid=%d Target=%s Cell=(%d,%d) Reject=%s"),
            *Session.SourceSurfaceTag.ToString(),
            Session.SourceCell.X,
            Session.SourceCell.Y,
            Session.InstanceId,
            Session.Quantity,
            Session.Preview.bValid ? 1 : 0,
            *Session.Preview.TargetTag.ToString(),
            Session.Preview.TargetCell.X,
            Session.Preview.TargetCell.Y,
            *Session.Preview.RejectReason.ToString());
    }

#if SLICE20_SABOTAGE
    // Slice 20 fitness test 4 sabotage - see Public/Slice20SabotageToggle.h.
    // Skipping the terminal Cancelled emit while still clearing the session
    // is exactly the class of bug the EveryDragSessionEmitsCompletedOrCancelled
    // fuzz test must detect. The fuzz test must flag "session had zero
    // terminal events" when it randomly picks Cancel-termination mid-run.
    Session = FInventoryDragSession();
    Controller.ClearPreview();
    return;
#else
    FInventoryDragEvent Event;
    Event.Kind = EInventoryDragEventKind::Cancelled;
    FillEventFromSession(Event);
    EmitEvent(MoveTemp(Event));

    Session = FInventoryDragSession();
    Controller.ClearPreview();
#endif
}

// ---------------------------------------------------------------------------
// Slice 19 - equip-slot drops.
//
// Equip slots do not live in a UniformGridPanel, so we skip geometry
// resolution and trust the wrapper's SlotTag identity. The required
// equip slot tag carried on the drag op decides whether the item can
// actually equip there. Router dispatch goes through the same
// FInventoryDropRouter which (Slice 19) also maps
// {Item.Container.* -> Item.EquipmentSlot.*} to RequestEquipItem.
// ---------------------------------------------------------------------------

namespace
{
    /**
     * Validate that the drag op's required equip slot (if any) matches
     * the wrapper's slot tag. Emits the reason name the session should
     * stamp when the pair is rejected; returns NAME_None when the pair
     * is acceptable.
     */
    FName ResolveEquipRejectReason(
        const FGameplayTag& SlotTag,
        const FGameplayTag& RequiredEquipSlotTag,
        const FGameplayTag& SourceSurfaceTag,
        int32 InstanceId,
        int32 Quantity)
    {
        if (!SlotTag.IsValid())
        {
            return FName(TEXT("InvalidSlotTag"));
        }
        if (InstanceId == INDEX_NONE || Quantity <= 0)
        {
            return FName(TEXT("EmptyDragPayload"));
        }
        if (!RequiredEquipSlotTag.IsValid())
        {
            return FName(TEXT("NotEquippable"));
        }
        if (RequiredEquipSlotTag != SlotTag)
        {
            return FName(TEXT("EquipSlotMismatch"));
        }
        const FGameplayTag& Container = ProjectTags::Item_Container;
        if (!SourceSurfaceTag.IsValid() || !SourceSurfaceTag.MatchesTag(Container))
        {
            return FName(TEXT("SourceOutOfDomain"));
        }
        return NAME_None;
    }
}

void UInventoryUIDragHostSubsystem::UpdateEquipPreview(
    const FGameplayTag& SlotTag,
    const FGameplayTag& RequiredEquipSlotTag,
    const FInventoryCellCandidate& Candidate)
{
    if (!Session.bActive)
    {
        return;
    }

    const FName Reason = ResolveEquipRejectReason(
        SlotTag, RequiredEquipSlotTag, Candidate.SourceSurfaceTag,
        Candidate.InstanceId, Candidate.Quantity);

    if (Reason.IsNone())
    {
        Session.Preview.TargetTag = SlotTag;
        Session.Preview.TargetCell = FIntPoint(0, 0);
        Session.Preview.bValid = true;
        Session.Preview.RejectReason = NAME_None;

        FInventoryDragEvent Event;
        Event.Kind = EInventoryDragEventKind::PreviewUpdated;
        FillEventFromSession(Event);
        Event.TargetTag = SlotTag;
        Event.TargetCell = FIntPoint(0, 0);
        EmitEvent(MoveTemp(Event));
    }
    else
    {
        Session.Preview = FInventoryPreviewSnapshot();
        Session.Preview.RejectReason = Reason;

        FInventoryDragEvent Event;
        Event.Kind = EInventoryDragEventKind::PreviewCleared;
        FillEventFromSession(Event);
        EmitEvent(MoveTemp(Event));
    }
}

bool UInventoryUIDragHostSubsystem::CompleteEquipDrop(
    const FGameplayTag& SlotTag,
    const FGameplayTag& RequiredEquipSlotTag,
    const FInventoryCellCandidate& Candidate)
{
    if (!Session.bActive)
    {
        return false;
    }

    const FName Reason = ResolveEquipRejectReason(
        SlotTag, RequiredEquipSlotTag, Candidate.SourceSurfaceTag,
        Candidate.InstanceId, Candidate.Quantity);

    if (!Reason.IsNone())
    {
        {
            FInventoryDragEvent Event;
            Event.Kind = EInventoryDragEventKind::DropRejected;
            FillEventFromSession(Event);
            Event.TargetTag = SlotTag;
            Event.TargetCell = FIntPoint(0, 0);
            Event.RejectReason = Reason;
            EmitEvent(MoveTemp(Event));
        }
        {
            FInventoryDragEvent Event;
            Event.Kind = EInventoryDragEventKind::Cancelled;
            FillEventFromSession(Event);
            EmitEvent(MoveTemp(Event));
        }
        Session = FInventoryDragSession();
        Controller.ClearPreview();
        return false;
    }

    // Resolve the command target via the policy-provider hook (same
    // object implements both IInventorySurfacePolicyProvider and
    // IInventoryDropCommandTarget). If nothing is wired, fail-closed
    // with a named rejection rather than silently dropping the command.
    IInventoryDropCommandTarget* CommandTarget =
        Cast<IInventoryDropCommandTarget>(PolicyProviderObject.Get());
    if (!CommandTarget)
    {
        {
            FInventoryDragEvent Event;
            Event.Kind = EInventoryDragEventKind::DropRejected;
            FillEventFromSession(Event);
            Event.TargetTag = SlotTag;
            Event.TargetCell = FIntPoint(0, 0);
            Event.RejectReason = FName(TEXT("NoCommandTarget"));
            EmitEvent(MoveTemp(Event));
        }
        {
            FInventoryDragEvent Event;
            Event.Kind = EInventoryDragEventKind::Cancelled;
            FillEventFromSession(Event);
            EmitEvent(MoveTemp(Event));
        }
        Session = FInventoryDragSession();
        Controller.ClearPreview();
        return false;
    }

    Session.Preview.TargetTag = SlotTag;
    Session.Preview.TargetCell = FIntPoint(0, 0);
    Session.Preview.bValid = true;
    Session.Preview.RejectReason = NAME_None;

    // DropResolved -> Routed -> VMInvoked -> Completed.
    {
        FInventoryDragEvent Event;
        Event.Kind = EInventoryDragEventKind::DropResolved;
        FillEventFromSession(Event);
        Event.TargetTag = SlotTag;
        Event.TargetCell = FIntPoint(0, 0);
        EmitEvent(MoveTemp(Event));
    }

    FInventoryDragContext Ctx;
    Ctx.InstanceId = Candidate.InstanceId;
    Ctx.SourceSurfaceTag = Candidate.SourceSurfaceTag;
    Ctx.SourcePos = Candidate.SourcePos;
    Ctx.bSourceRotated = Candidate.bRotated;
    Ctx.Quantity = Candidate.Quantity;

    FInventoryDropTarget RouterTarget;
    RouterTarget.TargetSurfaceTag = SlotTag;
    RouterTarget.TargetPos = FIntPoint(0, 0);
    RouterTarget.bTargetRotated = false;

    FInventoryDropRouteResolution Resolution;
    const bool bDispatched = FInventoryDropRouter::Route(*CommandTarget, Ctx, RouterTarget, &Resolution);

    if (!bDispatched || !Resolution.IsValid())
    {
        // Router refused the pair (should be rare for equip since the
        // wrapper pre-validates slot compatibility; still fail-loud).
        {
            FInventoryDragEvent Event;
            Event.Kind = EInventoryDragEventKind::DropRejected;
            FillEventFromSession(Event);
            Event.TargetTag = SlotTag;
            Event.TargetCell = FIntPoint(0, 0);
            Event.RejectReason = FName(TEXT("RouterDomainReject"));
            EmitEvent(MoveTemp(Event));
        }
        {
            FInventoryDragEvent Event;
            Event.Kind = EInventoryDragEventKind::Cancelled;
            FillEventFromSession(Event);
            EmitEvent(MoveTemp(Event));
        }
        Session = FInventoryDragSession();
        Controller.ClearPreview();
        return false;
    }

    {
        FInventoryDragEvent Event;
        Event.Kind = EInventoryDragEventKind::Routed;
        FillEventFromSession(Event);
        Event.TargetTag = SlotTag;
        Event.TargetCell = FIntPoint(0, 0);
        EmitEvent(MoveTemp(Event));
    }
    {
        FInventoryDragEvent Event;
        Event.Kind = EInventoryDragEventKind::VMInvoked;
        FillEventFromSession(Event);
        Event.TargetTag = SlotTag;
        Event.TargetCell = FIntPoint(0, 0);
        Event.VMMethod = Resolution.VMMethod;
        EmitEvent(MoveTemp(Event));
    }
    {
        FInventoryDragEvent Event;
        Event.Kind = EInventoryDragEventKind::Completed;
        FillEventFromSession(Event);
        Event.TargetTag = SlotTag;
        Event.TargetCell = FIntPoint(0, 0);
        EmitEvent(MoveTemp(Event));
    }

    Session = FInventoryDragSession();
    Controller.ClearPreview();
    return true;
}

FInventoryPreviewSnapshot UInventoryUIDragHostSubsystem::GetPreviewSnapshot() const
{
    return Session.Preview;
}

void UInventoryUIDragHostSubsystem::EmitEvent(FInventoryDragEvent Event)
{
    Event.TimestampSeconds = FPlatformTime::Seconds();
    OnDragEvent.Broadcast(Event);
}

void UInventoryUIDragHostSubsystem::FillEventFromSession(FInventoryDragEvent& OutEvent) const
{
    OutEvent.SourceTag = Session.SourceSurfaceTag;
    OutEvent.SourceCell = Session.SourceCell;
    OutEvent.InstanceId = Session.InstanceId;
    OutEvent.Quantity = Session.Quantity;
}
