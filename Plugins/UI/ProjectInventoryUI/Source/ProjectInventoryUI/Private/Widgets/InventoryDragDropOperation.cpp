// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Widgets/InventoryDragDropOperation.h"

#include "Components/Widget.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "Logging/LogMacros.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InventoryDragDropOperation)

DEFINE_LOG_CATEGORY_STATIC(LogInventoryDragDropOperation, Log, All);

// Named namespace (not anonymous) to avoid symbol collisions with
// other TUs in the ProjectInventoryUI unity build that also define
// IsInventoryDragDiagEnabled / DescribeObject in their own anonymous
// namespaces (e.g. InventoryUIDragHostSubsystem.cpp). UE concatenates
// participating .cpp files into one unity TU, which removes the
// internal-linkage isolation that anonymous namespaces normally
// provide. See docs/agents/canonical.md "Dev loop pitfalls" for the
// guideline.
namespace InventoryDragDropOperationLocal
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

    bool IsInventoryDragTraceEnabled()
    {
#if !UE_BUILD_SHIPPING
        if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("inv.drag.log")))
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

    FString DescribePointerEvent(const FPointerEvent& PointerEvent)
    {
        return FString::Printf(
            TEXT("Screen=(%.1f,%.1f) Last=(%.1f,%.1f) Delta=(%.1f,%.1f) Effecting=%s LDown=%d RDown=%d Touch=%d Pointer=%d"),
            PointerEvent.GetScreenSpacePosition().X,
            PointerEvent.GetScreenSpacePosition().Y,
            PointerEvent.GetLastScreenSpacePosition().X,
            PointerEvent.GetLastScreenSpacePosition().Y,
            PointerEvent.GetCursorDelta().X,
            PointerEvent.GetCursorDelta().Y,
            *PointerEvent.GetEffectingButton().ToString(),
            PointerEvent.IsMouseButtonDown(EKeys::LeftMouseButton) ? 1 : 0,
            PointerEvent.IsMouseButtonDown(EKeys::RightMouseButton) ? 1 : 0,
            PointerEvent.IsTouchEvent() ? 1 : 0,
            PointerEvent.GetPointerIndex());
    }

    FString DescribeDragOp(const UInventoryDragDropOperation& Operation)
    {
        return FString::Printf(
            TEXT("Op=%s Instance=%d Qty=%d Src=%s Pos=(%d,%d) Size=%dx%d Rot=%d Equip=%s Nearby=%d Payload=%s Visual=%s"),
            *Operation.GetPathName(),
            Operation.InstanceId,
            Operation.Quantity,
            *Operation.FromContainer.ToString(),
            Operation.FromPos.X,
            Operation.FromPos.Y,
            Operation.ItemSize.X,
            Operation.ItemSize.Y,
            Operation.bRotated ? 1 : 0,
            *Operation.EquipSlotTag.ToString(),
            Operation.bFromNearbyContainer ? 1 : 0,
            *DescribeObject(Operation.Payload.Get()),
            *DescribeObject(Operation.DefaultDragVisual.Get()));
    }
}

// IMPORTANT: do NOT add `using namespace InventoryDragDropOperationLocal;`
// at function or file scope. UE adaptive unity merges sibling .cpps into
// one TU, where any unqualified `IsInventoryDragDiagEnabled` call would
// see BOTH this file's named-namespace version AND a sibling file's
// anonymous-namespace version (e.g. InventoryUIDragHostSubsystem.cpp),
// producing a lookup ambiguity. Always qualify call sites with the
// namespace name. See Plugins/UI/ProjectInventoryUI/docs/pitfalls.md
// "Pattern G".
void UInventoryDragDropOperation::Drop_Implementation(const FPointerEvent& PointerEvent)
{
    if (InventoryDragDropOperationLocal::IsInventoryDragDiagEnabled())
    {
        UE_LOG(
            LogInventoryDragDropOperation,
            Log,
            TEXT("Drop: %s Event={%s}"),
            *InventoryDragDropOperationLocal::DescribeDragOp(*this),
            *InventoryDragDropOperationLocal::DescribePointerEvent(PointerEvent));
    }

    Super::Drop_Implementation(PointerEvent);
}

void UInventoryDragDropOperation::DragCancelled_Implementation(const FPointerEvent& PointerEvent)
{
    if (InventoryDragDropOperationLocal::IsInventoryDragDiagEnabled())
    {
        UE_LOG(
            LogInventoryDragDropOperation,
            Warning,
            TEXT("DragCancelled: %s Event={%s}"),
            *InventoryDragDropOperationLocal::DescribeDragOp(*this),
            *InventoryDragDropOperationLocal::DescribePointerEvent(PointerEvent));
    }

    Super::DragCancelled_Implementation(PointerEvent);
}

void UInventoryDragDropOperation::Dragged_Implementation(const FPointerEvent& PointerEvent)
{
    if (InventoryDragDropOperationLocal::IsInventoryDragTraceEnabled())
    {
        UE_LOG(
            LogInventoryDragDropOperation,
            Verbose,
            TEXT("Dragged: %s Event={%s}"),
            *InventoryDragDropOperationLocal::DescribeDragOp(*this),
            *InventoryDragDropOperationLocal::DescribePointerEvent(PointerEvent));
    }

    Super::Dragged_Implementation(PointerEvent);
}
