// Copyright ALIS. All Rights Reserved.

#include "Interaction/ProjectUIGridDragDropController.h"
#include "Components/UniformGridPanel.h"

bool FProjectUIGridDragDropController::ValidateFootprint(
    const FProjectUIGridDragPayload& DragPayload,
    int32 Col,
    int32 Row,
    int32 GridWidth,
    int32 GridHeight,
    bool bSecondary,
    TFunctionRef<bool(bool bSecondary, int32 CellIndex)> EnabledChecker,
    TFunctionRef<int32(bool bSecondary, int32 CellIndex)> OccupantChecker)
{
    if (DragPayload.ItemSize.X <= 0 || DragPayload.ItemSize.Y <= 0)
    {
        return false;
    }

    const int32 MaxCol = Col + DragPayload.ItemSize.X;
    const int32 MaxRow = Row + DragPayload.ItemSize.Y;
    if (Col < 0 || Row < 0 || MaxCol > GridWidth || MaxRow > GridHeight)
    {
        return false;
    }

    for (int32 OffsetY = 0; OffsetY < DragPayload.ItemSize.Y; ++OffsetY)
    {
        for (int32 OffsetX = 0; OffsetX < DragPayload.ItemSize.X; ++OffsetX)
        {
            const int32 X = Col + OffsetX;
            const int32 Y = Row + OffsetY;
            if (X < 0 || Y < 0 || X >= GridWidth || Y >= GridHeight)
            {
                return false;
            }

            const int32 CellIndex = Y * GridWidth + X;
            if (!EnabledChecker(bSecondary, CellIndex))
            {
                return false;
            }

            const int32 OccupantId = OccupantChecker(bSecondary, CellIndex);
            if (OccupantId != INDEX_NONE && OccupantId != DragPayload.InstanceId)
            {
                return false;
            }
        }
    }

    return true;
}

void FProjectUIGridDragDropController::Initialize(FProjectUIGridHitDetector* InHitDetector)
{
    HitDetector = InHitDetector;
}

void FProjectUIGridDragDropController::UpdatePreview(
    const FVector2D& ScreenPos,
    const FProjectUIGridDragPayload& DragPayload,
    UUniformGridPanel* PrimaryGrid,
    int32 PrimaryWidth,
    int32 PrimaryHeight,
    UUniformGridPanel* SecondaryGrid,
    int32 SecondaryWidth,
    int32 SecondaryHeight,
    TFunctionRef<bool(bool bSecondary, int32 CellIndex)> EnabledChecker,
    TFunctionRef<int32(bool bSecondary, int32 CellIndex)> OccupantChecker)
{
    ClearPreview();

    if (!HitDetector)
    {
        return;
    }

    int32 Col = INDEX_NONE;
    int32 Row = INDEX_NONE;
    int32 GridWidth = 0;
    int32 GridHeight = 0;
    bool bSecondary = false;

    if (!HitDetector->ResolveDualGridCoords(
            PrimaryGrid, PrimaryWidth, PrimaryHeight,
            SecondaryGrid, SecondaryWidth, SecondaryHeight,
            ScreenPos,
            Col, Row, GridWidth, GridHeight, bSecondary))
    {
        return;
    }

    const FIntPoint ItemSize = DragPayload.ItemSize;
    if (ItemSize.X <= 0 || ItemSize.Y <= 0)
    {
        return;
    }

    PreviewResult.bActive = true;
    PreviewResult.bSecondary = bSecondary;
    PreviewResult.bValid = true;

    const int32 MaxCol = Col + ItemSize.X;
    const int32 MaxRow = Row + ItemSize.Y;
    if (Col < 0 || Row < 0 || MaxCol > GridWidth || MaxRow > GridHeight)
    {
        PreviewResult.bValid = false;
    }

    for (int32 OffsetY = 0; OffsetY < ItemSize.Y; ++OffsetY)
    {
        for (int32 OffsetX = 0; OffsetX < ItemSize.X; ++OffsetX)
        {
            const int32 X = Col + OffsetX;
            const int32 Y = Row + OffsetY;
            if (X < 0 || Y < 0 || X >= GridWidth || Y >= GridHeight)
            {
                PreviewResult.bValid = false;
                continue;
            }

            const int32 CellIndex = Y * GridWidth + X;
            if (!EnabledChecker(bSecondary, CellIndex))
            {
                PreviewResult.bValid = false;
            }

            const int32 OccupantId = OccupantChecker(bSecondary, CellIndex);
            if (OccupantId != INDEX_NONE && OccupantId != DragPayload.InstanceId)
            {
                PreviewResult.bValid = false;
            }

            if (bSecondary)
            {
                PreviewResult.SecondaryCells.Add(CellIndex);
            }
            else
            {
                PreviewResult.PrimaryCells.Add(CellIndex);
            }
        }
    }
}

void FProjectUIGridDragDropController::ClearPreview()
{
    PreviewResult.Reset();
}

bool FProjectUIGridDragDropController::ResolveDropGridTarget(
    const FVector2D& ScreenPos,
    const FProjectUIGridDragPayload& DragPayload,
    UUniformGridPanel* PrimaryGrid,
    int32 PrimaryWidth,
    int32 PrimaryHeight,
    UUniformGridPanel* SecondaryGrid,
    int32 SecondaryWidth,
    int32 SecondaryHeight,
    TFunctionRef<bool(bool bSecondary, int32 CellIndex)> EnabledChecker,
    TFunctionRef<int32(bool bSecondary, int32 CellIndex)> OccupantChecker,
    int32& OutCol,
    int32& OutRow,
    bool& bOutIsSecondary) const
{
    OutCol = INDEX_NONE;
    OutRow = INDEX_NONE;
    bOutIsSecondary = false;

    if (!HitDetector)
    {
        return false;
    }

    int32 GridWidth = 0;
    int32 GridHeight = 0;
    if (!HitDetector->ResolveDualGridCoords(
            PrimaryGrid, PrimaryWidth, PrimaryHeight,
            SecondaryGrid, SecondaryWidth, SecondaryHeight,
            ScreenPos,
            OutCol, OutRow, GridWidth, GridHeight, bOutIsSecondary))
    {
        return false;
    }

    return ValidateFootprint(
        DragPayload,
        OutCol,
        OutRow,
        GridWidth,
        GridHeight,
        bOutIsSecondary,
        EnabledChecker,
        OccupantChecker);
}
