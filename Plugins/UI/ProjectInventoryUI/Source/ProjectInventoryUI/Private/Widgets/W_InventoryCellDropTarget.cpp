// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Widgets/W_InventoryCellDropTarget.h"

#include "Blueprint/WidgetTree.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Input/Reply.h"
#include "Logging/LogMacros.h"
#include "MVVM/InventoryDragEvent.h"
#include "Slice20SabotageToggle.h"
#include "Subsystems/InventoryUIDragHostSubsystem.h"
#include "Widgets/InventoryDragDropOperation.h"
#include "Widgets/ProjectGridCell.h"

#if SLICE20_SABOTAGE
// Slice 20 fitness test 1 sabotage - see Public/Slice20SabotageToggle.h.
// Including the router header from widget code is exactly what the
// NoWidgetIncludesRouterOrDispatcher fitness test is designed to flag.
#include "MVVM/InventoryDropRouter.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogInventoryCellDropTarget, Log, All);

namespace
{
    bool IsInventoryCellDropTargetDragDiagEnabled()
    {
#if !UE_BUILD_SHIPPING
        if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("inv.drag.diag")))
        {
            return Var->GetInt() != 0;
        }
#endif
        return false;
    }

    const TCHAR* InventoryCellDropTargetVisibilityToString(const ESlateVisibility Visibility)
    {
        switch (Visibility)
        {
        case ESlateVisibility::Visible:
            return TEXT("Visible");
        case ESlateVisibility::Collapsed:
            return TEXT("Collapsed");
        case ESlateVisibility::Hidden:
            return TEXT("Hidden");
        case ESlateVisibility::HitTestInvisible:
            return TEXT("HitTestInvisible");
        case ESlateVisibility::SelfHitTestInvisible:
            return TEXT("SelfHitTestInvisible");
        default:
            return TEXT("Unknown");
        }
    }
}

UW_InventoryCellDropTarget::UW_InventoryCellDropTarget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Wrapper is a pass-through hit-test target. Visible (not
    // SelfHitTestInvisible) so Slate actually dispatches DragOver/Drop
    // here when the cursor lands on a cell. Layout is driven entirely by
    // the hosted UProjectGridCell; we add no padding of our own.
    SetVisibility(ESlateVisibility::Visible);
    SetIsFocusable(false);
}

void UW_InventoryCellDropTarget::SetCellIdentity(const FGameplayTag& InSurfaceTag, int32 InCellIndex, int32 InGridWidth)
{
    SurfaceTag = InSurfaceTag;
    CellIndex = InCellIndex;
    GridWidth = InGridWidth;
}

void UW_InventoryCellDropTarget::SetHostedCell(UProjectGridCell* InCell)
{
    HostedCell = InCell;

    // Place the visual cell as this user-widget's root content. The
    // widget tree owns the cell after this call; the builder does not
    // parent the cell into a SizeBox separately. Matches the pattern
    // used by UDragBusTestHostWidget in the drag-event-bus tests.
    if (WidgetTree && InCell)
    {
        WidgetTree->RootWidget = InCell;
    }
}

UInventoryUIDragHostSubsystem* UW_InventoryCellDropTarget::ResolveDragHost() const
{
    if (APlayerController* PC = GetOwningPlayer())
    {
        if (ULocalPlayer* LP = PC->GetLocalPlayer())
        {
            return LP->GetSubsystem<UInventoryUIDragHostSubsystem>();
        }
    }
    return nullptr;
}

bool UW_InventoryCellDropTarget::NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    UInventoryDragDropOperation* DragOp = Cast<UInventoryDragDropOperation>(InOperation);
    if (!DragOp)
    {
        if (IsInventoryCellDropTargetDragDiagEnabled())
        {
            UE_LOG(
                LogInventoryCellDropTarget,
                Log,
                TEXT("NativeOnDragOver ignored non-inventory payload Surface=%s Cell=%d"),
                *SurfaceTag.ToString(),
                CellIndex);
        }
        return Super::NativeOnDragOver(InGeometry, InDragDropEvent, InOperation);
    }

    UInventoryUIDragHostSubsystem* Subsystem = ResolveDragHost();
    if (!Subsystem)
    {
        UE_LOG(
            LogInventoryCellDropTarget,
            Warning,
            TEXT("NativeOnDragOver has no drag host Surface=%s Cell=%d"),
            *SurfaceTag.ToString(),
            CellIndex);
        return false;
    }

    if (IsInventoryCellDropTargetDragDiagEnabled())
    {
        const FVector2D LocalSize = InGeometry.GetLocalSize();
        UE_LOG(
            LogInventoryCellDropTarget,
            Log,
            TEXT("NativeOnDragOver Surface=%s Cell=%d Screen=(%.1f,%.1f) WrapperVis=%s Enabled=%d Size=(%.1f,%.1f) Instance=%d Src=%s Pos=(%d,%d) Qty=%d Item=%dx%d"),
            *SurfaceTag.ToString(),
            CellIndex,
            InDragDropEvent.GetScreenSpacePosition().X,
            InDragDropEvent.GetScreenSpacePosition().Y,
            InventoryCellDropTargetVisibilityToString(GetVisibility()),
            GetIsEnabled() ? 1 : 0,
            LocalSize.X,
            LocalSize.Y,
            DragOp->InstanceId,
            *DragOp->FromContainer.ToString(),
            DragOp->FromPos.X,
            DragOp->FromPos.Y,
            DragOp->Quantity,
            DragOp->ItemSize.X,
            DragOp->ItemSize.Y);
    }

    FInventoryCellCandidate Candidate;
    Candidate.InstanceId = DragOp->InstanceId;
    Candidate.SourceSurfaceTag = DragOp->FromContainer;
    Candidate.SourcePos = DragOp->FromPos;
    Candidate.Quantity = DragOp->Quantity;
    Candidate.bRotated = DragOp->bRotated;
    Candidate.ItemSize = DragOp->ItemSize;

    // This wrapper is the smallest semantic target. Once Slate routed the
    // event here, keep validation in the subsystem but do NOT re-resolve
    // against sibling surfaces by screen position.
    Subsystem->UpdatePreviewAtTarget(Candidate, SurfaceTag, CellIndex);
    return true;
}

void UW_InventoryCellDropTarget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    if (IsInventoryCellDropTargetDragDiagEnabled())
    {
        UE_LOG(
            LogInventoryCellDropTarget,
            Log,
            TEXT("NativeOnDragLeave Surface=%s Cell=%d Screen=(%.1f,%.1f) WrapperVis=%s Enabled=%d Op=%s"),
            *SurfaceTag.ToString(),
            CellIndex,
            InDragDropEvent.GetScreenSpacePosition().X,
            InDragDropEvent.GetScreenSpacePosition().Y,
            InventoryCellDropTargetVisibilityToString(GetVisibility()),
            GetIsEnabled() ? 1 : 0,
            InOperation ? *InOperation->GetPathName() : TEXT("null"));
    }

    Super::NativeOnDragLeave(InDragDropEvent, InOperation);
}

void UW_InventoryCellDropTarget::NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    if (IsInventoryCellDropTargetDragDiagEnabled())
    {
        UE_LOG(
            LogInventoryCellDropTarget,
            Warning,
            TEXT("NativeOnDragCancelled Surface=%s Cell=%d Screen=(%.1f,%.1f) WrapperVis=%s Enabled=%d Op=%s"),
            *SurfaceTag.ToString(),
            CellIndex,
            InDragDropEvent.GetScreenSpacePosition().X,
            InDragDropEvent.GetScreenSpacePosition().Y,
            InventoryCellDropTargetVisibilityToString(GetVisibility()),
            GetIsEnabled() ? 1 : 0,
            InOperation ? *InOperation->GetPathName() : TEXT("null"));
    }

    Super::NativeOnDragCancelled(InDragDropEvent, InOperation);
}

bool UW_InventoryCellDropTarget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    UInventoryDragDropOperation* DragOp = Cast<UInventoryDragDropOperation>(InOperation);
    if (!DragOp)
    {
        if (IsInventoryCellDropTargetDragDiagEnabled())
        {
            UE_LOG(
                LogInventoryCellDropTarget,
                Log,
                TEXT("NativeOnDrop ignored non-inventory payload Surface=%s Cell=%d"),
                *SurfaceTag.ToString(),
                CellIndex);
        }
        return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
    }

    UInventoryUIDragHostSubsystem* Subsystem = ResolveDragHost();
    if (!Subsystem)
    {
        UE_LOG(
            LogInventoryCellDropTarget,
            Warning,
            TEXT("NativeOnDrop has no drag host Surface=%s Cell=%d"),
            *SurfaceTag.ToString(),
            CellIndex);
        return false;
    }

    if (IsInventoryCellDropTargetDragDiagEnabled())
    {
        const FVector2D LocalSize = InGeometry.GetLocalSize();
        UE_LOG(
            LogInventoryCellDropTarget,
            Log,
            TEXT("NativeOnDrop Surface=%s Cell=%d Screen=(%.1f,%.1f) WrapperVis=%s Enabled=%d Size=(%.1f,%.1f) Instance=%d Src=%s Pos=(%d,%d) Qty=%d Item=%dx%d"),
            *SurfaceTag.ToString(),
            CellIndex,
            InDragDropEvent.GetScreenSpacePosition().X,
            InDragDropEvent.GetScreenSpacePosition().Y,
            InventoryCellDropTargetVisibilityToString(GetVisibility()),
            GetIsEnabled() ? 1 : 0,
            LocalSize.X,
            LocalSize.Y,
            DragOp->InstanceId,
            *DragOp->FromContainer.ToString(),
            DragOp->FromPos.X,
            DragOp->FromPos.Y,
            DragOp->Quantity,
            DragOp->ItemSize.X,
            DragOp->ItemSize.Y);
    }

    FInventoryCellCandidate Candidate;
    Candidate.InstanceId = DragOp->InstanceId;
    Candidate.SourceSurfaceTag = DragOp->FromContainer;
    Candidate.SourcePos = DragOp->FromPos;
    Candidate.Quantity = DragOp->Quantity;
    Candidate.bRotated = DragOp->bRotated;
    Candidate.ItemSize = DragOp->ItemSize;

    const bool bDispatched = Subsystem->CompleteDropAtTarget(Candidate, SurfaceTag, CellIndex);

    UE_LOG(LogInventoryCellDropTarget, Verbose,
        TEXT("NativeOnDrop Surface=%s Cell=%d Dispatched=%d"),
        *SurfaceTag.ToString(), CellIndex, bDispatched ? 1 : 0);

    // When the subsystem cannot route (e.g. target doesn't match, or
    // router refuses the pair like nearby->nearby), return Unhandled so
    // the event bubbles to the next handler. In Slice 17 that lets the
    // equip-slot path on UW_InventoryPanel still consume equip drops.
    return bDispatched;
}
