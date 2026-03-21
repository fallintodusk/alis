// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IInventoryReadOnly.h"

class UProjectUIThemeData;
class UWidget;

struct PROJECTINVENTORYUI_API FInventoryDragVisualBuilder
{
    static UWidget* Build(
        UObject* Outer,
        const FInventoryEntryView& Entry,
        int32 DragQuantity,
        UProjectUIThemeData* Theme);
};
