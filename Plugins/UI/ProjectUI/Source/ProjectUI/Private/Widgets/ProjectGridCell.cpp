// Copyright ALIS. All Rights Reserved.

#include "Widgets/ProjectGridCell.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "InputCoreTypes.h"

UProjectGridCell::UProjectGridCell(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

void UProjectGridCell::PostInitProperties()
{
    Super::PostInitProperties();

    if (!HasAnyFlags(RF_ClassDefaultObject))
    {
        OnMouseButtonDownEvent.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(UProjectGridCell, HandleMouseDown));
    }
}

FEventReply UProjectGridCell::HandleMouseDown(FGeometry MyGeometry, const FPointerEvent& MouseEvent)
{
    const FKey Button = MouseEvent.GetEffectingButton();
    if (Button != EKeys::LeftMouseButton && Button != EKeys::RightMouseButton)
    {
        return UWidgetBlueprintLibrary::Unhandled();
    }

    if (bIsGridCell && GridMouseDownHandler.IsBound())
    {
        return GridMouseDownHandler.Execute(CellIndex, bSecondary, MouseEvent);
    }

    if (Button == EKeys::LeftMouseButton)
    {
        OnCellClicked.Broadcast(CellIndex);
    }
    else
    {
        OnCellRightClicked.Broadcast(CellIndex);
    }

    return UWidgetBlueprintLibrary::Handled();
}
