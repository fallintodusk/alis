// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Support/InventoryDragEventRecorder.h"

#include "Subsystems/InventoryUIDragHostSubsystem.h"

namespace
{
    const TCHAR* KindToString(EInventoryDragEventKind Kind)
    {
        switch (Kind)
        {
            case EInventoryDragEventKind::Started:        return TEXT("Started");
            case EInventoryDragEventKind::PreviewUpdated: return TEXT("PreviewUpdated");
            case EInventoryDragEventKind::PreviewCleared: return TEXT("PreviewCleared");
            case EInventoryDragEventKind::DropResolved:   return TEXT("DropResolved");
            case EInventoryDragEventKind::DropRejected:   return TEXT("DropRejected");
            case EInventoryDragEventKind::Routed:         return TEXT("Routed");
            case EInventoryDragEventKind::VMInvoked:      return TEXT("VMInvoked");
            case EInventoryDragEventKind::Completed:      return TEXT("Completed");
            case EInventoryDragEventKind::Cancelled:      return TEXT("Cancelled");
            default:                                      return TEXT("?");
        }
    }
}

FInventoryDragEventRecorder::FInventoryDragEventRecorder(UInventoryUIDragHostSubsystem* InSubsystem)
    : Subsystem(InSubsystem)
{
    if (InSubsystem)
    {
        Handle = InSubsystem->OnDragEvent.AddLambda(
            [this](const FInventoryDragEvent& Event)
            {
                Events.Add(Event);
            });
    }
}

FInventoryDragEventRecorder::~FInventoryDragEventRecorder()
{
    if (UInventoryUIDragHostSubsystem* LiveSubsystem = Subsystem.Get())
    {
        LiveSubsystem->OnDragEvent.Remove(Handle);
    }
}

void FInventoryDragEventRecorder::Clear()
{
    Events.Reset();
}

bool FInventoryDragEventRecorder::AssertEventCount(FAutomationTestBase& Test, int32 ExpectedCount) const
{
    if (Events.Num() != ExpectedCount)
    {
        Test.AddError(FString::Printf(
            TEXT("AssertEventCount: expected %d events, got %d"),
            ExpectedCount, Events.Num()));
        return false;
    }
    return true;
}

bool FInventoryDragEventRecorder::AssertSequence(FAutomationTestBase& Test, TArrayView<const FInventoryDragEvent> Expected) const
{
    if (Events.Num() != Expected.Num())
    {
        FString Got;
        for (const FInventoryDragEvent& E : Events)
        {
            Got += FString::Printf(TEXT("%s "), KindToString(E.Kind));
        }
        FString Want;
        for (const FInventoryDragEvent& E : Expected)
        {
            Want += FString::Printf(TEXT("%s "), KindToString(E.Kind));
        }
        Test.AddError(FString::Printf(
            TEXT("AssertSequence: length mismatch - expected %d [%s], got %d [%s]"),
            Expected.Num(), *Want, Events.Num(), *Got));
        return false;
    }

    const FInventoryDragEvent Default;
    bool bOk = true;

    for (int32 Index = 0; Index < Expected.Num(); ++Index)
    {
        const FInventoryDragEvent& E = Expected[Index];
        const FInventoryDragEvent& G = Events[Index];

        // Kind is always checked - it's the primary axis.
        if (G.Kind != E.Kind)
        {
            Test.AddError(FString::Printf(
                TEXT("AssertSequence: step %d expected kind %s, got %s"),
                Index, KindToString(E.Kind), KindToString(G.Kind)));
            bOk = false;
            continue;
        }

        // Partial-field matching: compare only fields the expected entry
        // set non-default. This lets tests pin only the axes they care
        // about (e.g. "VMMethod must be non-None on VMInvoked").
        if (E.SourceTag != Default.SourceTag && G.SourceTag != E.SourceTag)
        {
            Test.AddError(FString::Printf(
                TEXT("AssertSequence: step %d (%s) expected SourceTag=%s, got %s"),
                Index, KindToString(E.Kind), *E.SourceTag.ToString(), *G.SourceTag.ToString()));
            bOk = false;
        }
        if (E.TargetTag != Default.TargetTag && G.TargetTag != E.TargetTag)
        {
            Test.AddError(FString::Printf(
                TEXT("AssertSequence: step %d (%s) expected TargetTag=%s, got %s"),
                Index, KindToString(E.Kind), *E.TargetTag.ToString(), *G.TargetTag.ToString()));
            bOk = false;
        }
        if (E.SourceCell != Default.SourceCell && G.SourceCell != E.SourceCell)
        {
            Test.AddError(FString::Printf(
                TEXT("AssertSequence: step %d (%s) expected SourceCell=(%d,%d), got (%d,%d)"),
                Index, KindToString(E.Kind),
                E.SourceCell.X, E.SourceCell.Y, G.SourceCell.X, G.SourceCell.Y));
            bOk = false;
        }
        if (E.TargetCell != Default.TargetCell && G.TargetCell != E.TargetCell)
        {
            Test.AddError(FString::Printf(
                TEXT("AssertSequence: step %d (%s) expected TargetCell=(%d,%d), got (%d,%d)"),
                Index, KindToString(E.Kind),
                E.TargetCell.X, E.TargetCell.Y, G.TargetCell.X, G.TargetCell.Y));
            bOk = false;
        }
        if (E.InstanceId != Default.InstanceId && G.InstanceId != E.InstanceId)
        {
            Test.AddError(FString::Printf(
                TEXT("AssertSequence: step %d (%s) expected InstanceId=%d, got %d"),
                Index, KindToString(E.Kind), E.InstanceId, G.InstanceId));
            bOk = false;
        }
        if (E.Quantity != Default.Quantity && G.Quantity != E.Quantity)
        {
            Test.AddError(FString::Printf(
                TEXT("AssertSequence: step %d (%s) expected Quantity=%d, got %d"),
                Index, KindToString(E.Kind), E.Quantity, G.Quantity));
            bOk = false;
        }
        // RejectReason / VMMethod: match both "must be non-None" (expected
        // uses the sentinel name "__NonNone__") and "must equal a specific
        // name". Tests usually care about "VMMethod was populated" which
        // we express with the sentinel approach.
        static const FName NonNoneSentinel(TEXT("__NonNone__"));
        if (E.RejectReason == NonNoneSentinel)
        {
            if (G.RejectReason.IsNone())
            {
                Test.AddError(FString::Printf(
                    TEXT("AssertSequence: step %d (%s) expected RejectReason to be non-None, got None"),
                    Index, KindToString(E.Kind)));
                bOk = false;
            }
        }
        else if (!E.RejectReason.IsNone() && G.RejectReason != E.RejectReason)
        {
            Test.AddError(FString::Printf(
                TEXT("AssertSequence: step %d (%s) expected RejectReason=%s, got %s"),
                Index, KindToString(E.Kind), *E.RejectReason.ToString(), *G.RejectReason.ToString()));
            bOk = false;
        }
        if (E.VMMethod == NonNoneSentinel)
        {
            if (G.VMMethod.IsNone())
            {
                Test.AddError(FString::Printf(
                    TEXT("AssertSequence: step %d (%s) expected VMMethod to be non-None, got None"),
                    Index, KindToString(E.Kind)));
                bOk = false;
            }
        }
        else if (!E.VMMethod.IsNone() && G.VMMethod != E.VMMethod)
        {
            Test.AddError(FString::Printf(
                TEXT("AssertSequence: step %d (%s) expected VMMethod=%s, got %s"),
                Index, KindToString(E.Kind), *E.VMMethod.ToString(), *G.VMMethod.ToString()));
            bOk = false;
        }
    }

    return bOk;
}
