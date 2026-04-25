// Copyright ALIS. All Rights Reserved.

#include "Diagnostics/InventoryDragLogger.h"

#if !UE_BUILD_SHIPPING

#include "HAL/IConsoleManager.h"
#include "Logging/LogMacros.h"
#include "MVVM/InventoryDragEvent.h"
#include "Subsystems/InventoryUIDragHostSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogInventoryDragHost, Log, All);

namespace
{
    static TAutoConsoleVariable<int32> CVarInventoryDragLog(
        TEXT("inv.drag.log"),
        0,
        TEXT("When 1, installs a Verbose-level logger on the inventory drag event stream. 0 disables."),
        ECVF_Default);

    const TCHAR* EventKindToString(EInventoryDragEventKind Kind)
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

    /**
     * Per-subsystem tracking so Install() can rebind without piling up
     * delegate handles. Weak map keyed by subsystem pointer; entries
     * auto-expire when the subsystem goes away (we detect via weak ptr
     * on the next Install call).
     */
    struct FLoggerBinding
    {
        TWeakObjectPtr<UInventoryUIDragHostSubsystem> Subsystem;
        FDelegateHandle Handle;
    };

    TArray<FLoggerBinding>& GetBindings()
    {
        static TArray<FLoggerBinding> Bindings;
        return Bindings;
    }

    void PruneBindings()
    {
        TArray<FLoggerBinding>& Bindings = GetBindings();
        for (int32 i = Bindings.Num() - 1; i >= 0; --i)
        {
            if (!Bindings[i].Subsystem.IsValid())
            {
                Bindings.RemoveAt(i);
            }
        }
    }
}

void FInventoryDragLogger::Install(UInventoryUIDragHostSubsystem* Subsystem)
{
    if (!Subsystem)
    {
        return;
    }
    if (CVarInventoryDragLog.GetValueOnAnyThread() == 0)
    {
        return;
    }

    PruneBindings();

    // Drop any prior binding for this subsystem so re-install is
    // idempotent.
    TArray<FLoggerBinding>& Bindings = GetBindings();
    for (int32 i = Bindings.Num() - 1; i >= 0; --i)
    {
        if (Bindings[i].Subsystem.Get() == Subsystem)
        {
            Subsystem->OnDragEvent.Remove(Bindings[i].Handle);
            Bindings.RemoveAt(i);
        }
    }

    FLoggerBinding NewBinding;
    NewBinding.Subsystem = Subsystem;
    NewBinding.Handle = Subsystem->OnDragEvent.AddLambda(
        [](const FInventoryDragEvent& Event)
        {
            UE_LOG(LogInventoryDragHost, Verbose,
                TEXT("drag kind=%s src=%s@(%d,%d) tgt=%s@(%d,%d) instance=%d qty=%d reject=%s vm=%s t=%.3f"),
                EventKindToString(Event.Kind),
                *Event.SourceTag.ToString(), Event.SourceCell.X, Event.SourceCell.Y,
                *Event.TargetTag.ToString(), Event.TargetCell.X, Event.TargetCell.Y,
                Event.InstanceId, Event.Quantity,
                *Event.RejectReason.ToString(), *Event.VMMethod.ToString(),
                Event.TimestampSeconds);
        });
    Bindings.Add(NewBinding);
}

#endif // !UE_BUILD_SHIPPING
