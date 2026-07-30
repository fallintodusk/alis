// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Widgets/W_InventoryEquipSlotDropTarget.h"

#include "Blueprint/WidgetTree.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Input/Reply.h"
#include "Logging/LogMacros.h"
#include "MVVM/InventoryDragEvent.h"
#include "Subsystems/InventoryUIDragHostSubsystem.h"
#include "Widgets/InventoryDragDropOperation.h"

DEFINE_LOG_CATEGORY_STATIC(LogInventoryEquipSlotDropTarget, Log, All);

namespace
{
    bool IsInventoryEquipSlotDropTargetDragDiagEnabled()
    {
#if !UE_BUILD_SHIPPING
        if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("inv.drag.diag")))
        {
            return Var->GetInt() != 0;
        }
#endif
        return false;
    }

    const TCHAR* InventoryEquipSlotDropTargetVisibilityToString(const ESlateVisibility Visibility)
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

UW_InventoryEquipSlotDropTarget::UW_InventoryEquipSlotDropTarget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Pass-through hit-test target. Visible (not SelfHitTestInvisible)
    // so Slate actually dispatches DragOver/Drop here when the cursor
    // lands on the slot. Layout is driven entirely by hosted content.
    SetVisibility(ESlateVisibility::Visible);
    SetIsFocusable(false);
}

void UW_InventoryEquipSlotDropTarget::SetSlotIdentity(const FGameplayTag& InSlotTag)
{
    SlotTag = InSlotTag;
}

void UW_InventoryEquipSlotDropTarget::SetHostedContent(UWidget* InContent)
{
    HostedContent = InContent;

    // Place the visual tree as this user-widget's root content. The
    // widget tree owns the content after this call.
    if (WidgetTree && InContent)
    {
        WidgetTree->RootWidget = InContent;
    }
}

UInventoryUIDragHostSubsystem* UW_InventoryEquipSlotDropTarget::ResolveDragHost() const
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

bool UW_InventoryEquipSlotDropTarget::NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    UInventoryDragDropOperation* DragOp = Cast<UInventoryDragDropOperation>(InOperation);
    if (!DragOp)
    {
        if (IsInventoryEquipSlotDropTargetDragDiagEnabled())
        {
            UE_LOG(
                LogInventoryEquipSlotDropTarget,
                Log,
                TEXT("NativeOnDragOver ignored non-inventory payload Slot=%s"),
                *SlotTag.ToString());
        }
        return Super::NativeOnDragOver(InGeometry, InDragDropEvent, InOperation);
    }

    UInventoryUIDragHostSubsystem* Subsystem = ResolveDragHost();
    if (!Subsystem || !SlotTag.IsValid())
    {
        UE_LOG(
            LogInventoryEquipSlotDropTarget,
            Warning,
            TEXT("NativeOnDragOver invalid slot or no host Slot=%s HasHost=%d"),
            *SlotTag.ToString(),
            Subsystem ? 1 : 0);
        return false;
    }

    if (IsInventoryEquipSlotDropTargetDragDiagEnabled())
    {
        const FVector2D LocalSize = InGeometry.GetLocalSize();
        UE_LOG(
            LogInventoryEquipSlotDropTarget,
            Log,
            TEXT("NativeOnDragOver Slot=%s Screen=(%.1f,%.1f) WrapperVis=%s Enabled=%d Size=(%.1f,%.1f) Instance=%d Src=%s Pos=(%d,%d) Qty=%d Item=%dx%d Equip=%s"),
            *SlotTag.ToString(),
            InDragDropEvent.GetScreenSpacePosition().X,
            InDragDropEvent.GetScreenSpacePosition().Y,
            InventoryEquipSlotDropTargetVisibilityToString(GetVisibility()),
            GetIsEnabled() ? 1 : 0,
            LocalSize.X,
            LocalSize.Y,
            DragOp->InstanceId,
            *DragOp->FromContainer.ToString(),
            DragOp->FromPos.X,
            DragOp->FromPos.Y,
            DragOp->Quantity,
            DragOp->ItemSize.X,
            DragOp->ItemSize.Y,
            *DragOp->EquipSlotTag.ToString());
    }

    FInventoryCellCandidate Candidate;
    Candidate.InstanceId = DragOp->InstanceId;
    Candidate.SourceSurfaceTag = DragOp->FromContainer;
    Candidate.SourcePos = DragOp->FromPos;
    Candidate.Quantity = DragOp->Quantity;
    Candidate.bRotated = DragOp->bRotated;
    Candidate.ItemSize = DragOp->ItemSize;

    Subsystem->UpdateEquipPreview(SlotTag, DragOp->EquipSlotTag, Candidate);
    return true;
}

void UW_InventoryEquipSlotDropTarget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    if (IsInventoryEquipSlotDropTargetDragDiagEnabled())
    {
        UE_LOG(
            LogInventoryEquipSlotDropTarget,
            Log,
            TEXT("NativeOnDragLeave Slot=%s Screen=(%.1f,%.1f) WrapperVis=%s Enabled=%d Op=%s"),
            *SlotTag.ToString(),
            InDragDropEvent.GetScreenSpacePosition().X,
            InDragDropEvent.GetScreenSpacePosition().Y,
            InventoryEquipSlotDropTargetVisibilityToString(GetVisibility()),
            GetIsEnabled() ? 1 : 0,
            InOperation ? *InOperation->GetPathName() : TEXT("null"));
    }

    Super::NativeOnDragLeave(InDragDropEvent, InOperation);
}

void UW_InventoryEquipSlotDropTarget::NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    if (IsInventoryEquipSlotDropTargetDragDiagEnabled())
    {
        UE_LOG(
            LogInventoryEquipSlotDropTarget,
            Warning,
            TEXT("NativeOnDragCancelled Slot=%s Screen=(%.1f,%.1f) WrapperVis=%s Enabled=%d Op=%s"),
            *SlotTag.ToString(),
            InDragDropEvent.GetScreenSpacePosition().X,
            InDragDropEvent.GetScreenSpacePosition().Y,
            InventoryEquipSlotDropTargetVisibilityToString(GetVisibility()),
            GetIsEnabled() ? 1 : 0,
            InOperation ? *InOperation->GetPathName() : TEXT("null"));
    }

    Super::NativeOnDragCancelled(InDragDropEvent, InOperation);
}

bool UW_InventoryEquipSlotDropTarget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    UInventoryDragDropOperation* DragOp = Cast<UInventoryDragDropOperation>(InOperation);
    if (!DragOp)
    {
        if (IsInventoryEquipSlotDropTargetDragDiagEnabled())
        {
            UE_LOG(
                LogInventoryEquipSlotDropTarget,
                Log,
                TEXT("NativeOnDrop ignored non-inventory payload Slot=%s"),
                *SlotTag.ToString());
        }
        return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
    }

    UInventoryUIDragHostSubsystem* Subsystem = ResolveDragHost();
    if (!Subsystem || !SlotTag.IsValid())
    {
        UE_LOG(
            LogInventoryEquipSlotDropTarget,
            Warning,
            TEXT("NativeOnDrop invalid slot or no host Slot=%s HasHost=%d"),
            *SlotTag.ToString(),
            Subsystem ? 1 : 0);
        return false;
    }

    if (IsInventoryEquipSlotDropTargetDragDiagEnabled())
    {
        const FVector2D LocalSize = InGeometry.GetLocalSize();
        UE_LOG(
            LogInventoryEquipSlotDropTarget,
            Log,
            TEXT("NativeOnDrop Slot=%s Screen=(%.1f,%.1f) WrapperVis=%s Enabled=%d Size=(%.1f,%.1f) Instance=%d Src=%s Pos=(%d,%d) Qty=%d Item=%dx%d Equip=%s"),
            *SlotTag.ToString(),
            InDragDropEvent.GetScreenSpacePosition().X,
            InDragDropEvent.GetScreenSpacePosition().Y,
            InventoryEquipSlotDropTargetVisibilityToString(GetVisibility()),
            GetIsEnabled() ? 1 : 0,
            LocalSize.X,
            LocalSize.Y,
            DragOp->InstanceId,
            *DragOp->FromContainer.ToString(),
            DragOp->FromPos.X,
            DragOp->FromPos.Y,
            DragOp->Quantity,
            DragOp->ItemSize.X,
            DragOp->ItemSize.Y,
            *DragOp->EquipSlotTag.ToString());
    }

    FInventoryCellCandidate Candidate;
    Candidate.InstanceId = DragOp->InstanceId;
    Candidate.SourceSurfaceTag = DragOp->FromContainer;
    Candidate.SourcePos = DragOp->FromPos;
    Candidate.Quantity = DragOp->Quantity;
    Candidate.bRotated = DragOp->bRotated;
    Candidate.ItemSize = DragOp->ItemSize;

    const bool bDispatched = Subsystem->CompleteEquipDrop(SlotTag, DragOp->EquipSlotTag, Candidate);

    UE_LOG(LogInventoryEquipSlotDropTarget, Verbose,
        TEXT("NativeOnDrop Slot=%s Dispatched=%d"),
        *SlotTag.ToString(), bDispatched ? 1 : 0);

    return bDispatched;
}
