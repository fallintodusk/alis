// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DragBusTestHostWidget.generated.h"

/**
 * Concrete UUserWidget subclass used by the drag-event-bus tests to
 * host a UniformGridPanel in the viewport. UUserWidget itself is
 * abstract in UMG 5.7 so CreateWidget<UUserWidget> is rejected. This
 * minimal non-abstract subclass exists only to bypass that check.
 */
UCLASS()
class UDragBusTestHostWidget : public UUserWidget
{
    GENERATED_BODY()
};
