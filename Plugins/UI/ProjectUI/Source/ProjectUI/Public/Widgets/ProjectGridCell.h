// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/Border.h"
#include "ProjectGridCell.generated.h"

struct FPointerEvent;

/**
 * Generic clickable grid cell widget.
 * Emits left/right click events and can optionally forward grid-cell mouse down
 * to an owning controller for drag-detect behavior.
 */
UCLASS()
class PROJECTUI_API UProjectGridCell : public UBorder
{
    GENERATED_BODY()

public:
    UProjectGridCell(const FObjectInitializer& ObjectInitializer);

    DECLARE_MULTICAST_DELEGATE_OneParam(FOnCellClicked, int32);
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnCellRightClicked, int32);
    DECLARE_DELEGATE_RetVal_ThreeParams(FEventReply, FOnGridCellMouseDown, int32, bool, const FPointerEvent&);

    void SetCellIndex(int32 InIndex) { CellIndex = InIndex; }
    int32 GetCellIndex() const { return CellIndex; }

    void SetSecondaryGrid(bool bInSecondary) { bSecondary = bInSecondary; }
    bool IsSecondaryGrid() const { return bSecondary; }

    void SetIsGridCell(bool bInGridCell) { bIsGridCell = bInGridCell; }
    bool IsGridCell() const { return bIsGridCell; }

    void SetGridMouseDownHandler(FOnGridCellMouseDown InHandler) { GridMouseDownHandler = MoveTemp(InHandler); }

    FOnCellClicked OnCellClicked;
    FOnCellRightClicked OnCellRightClicked;

protected:
    virtual void PostInitProperties() override;

    UFUNCTION()
    FEventReply HandleMouseDown(FGeometry MyGeometry, const FPointerEvent& MouseEvent);

private:
    UPROPERTY()
    int32 CellIndex = INDEX_NONE;

    UPROPERTY()
    bool bSecondary = false;

    // Only grid cells forward mouse-down to panel/controller for selection/drag.
    // Tabs/equip slots still use click delegates only.
    bool bIsGridCell = false;

    FOnGridCellMouseDown GridMouseDownHandler;
};
