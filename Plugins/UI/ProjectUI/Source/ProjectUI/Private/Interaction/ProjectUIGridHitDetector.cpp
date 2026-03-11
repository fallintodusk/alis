// Copyright ALIS. All Rights Reserved.

#include "Interaction/ProjectUIGridHitDetector.h"
#include "Components/UniformGridPanel.h"

bool FProjectUIGridHitDetector::ResolveGridHit(
    UUniformGridPanel* Panel,
    int32 GridWidth,
    int32 GridHeight,
    const FVector2D& ScreenPos,
    int32& OutCellIndex) const
{
    OutCellIndex = INDEX_NONE;

    if (!Panel || GridWidth <= 0 || GridHeight <= 0)
    {
        return false;
    }

    const FGeometry GridGeometry = Panel->GetCachedGeometry();
    if (!GridGeometry.IsUnderLocation(ScreenPos))
    {
        return false;
    }

    const FVector2D LocalPos = GridGeometry.AbsoluteToLocal(ScreenPos);
    const FVector2D GridSize = GridGeometry.GetLocalSize();

    int32 Col = INDEX_NONE;
    int32 Row = INDEX_NONE;
    if (!LocalPosToGridCoords(LocalPos, GridSize, GridWidth, GridHeight, Col, Row))
    {
        return false;
    }

    OutCellIndex = GridCoordsToIndex(Col, Row, GridWidth);
    return true;
}

bool FProjectUIGridHitDetector::ResolveDualGridHit(
    UUniformGridPanel* PrimaryPanel,
    int32 PrimaryWidth,
    int32 PrimaryHeight,
    UUniformGridPanel* SecondaryPanel,
    int32 SecondaryWidth,
    int32 SecondaryHeight,
    const FVector2D& ScreenPos,
    FProjectUIGridHitResult& OutResult) const
{
    OutResult.Reset();

    int32 PrimaryIndex = INDEX_NONE;
    if (ResolveGridHit(PrimaryPanel, PrimaryWidth, PrimaryHeight, ScreenPos, PrimaryIndex))
    {
        OutResult.CellIndex = PrimaryIndex;
        OutResult.bIsSecondary = false;
        return true;
    }

    int32 SecondaryIndex = INDEX_NONE;
    if (ResolveGridHit(SecondaryPanel, SecondaryWidth, SecondaryHeight, ScreenPos, SecondaryIndex))
    {
        OutResult.CellIndex = SecondaryIndex;
        OutResult.bIsSecondary = true;
        return true;
    }

    return false;
}

bool FProjectUIGridHitDetector::LocalPosToGridCoords(
    const FVector2D& LocalPos,
    const FVector2D& GridSize,
    int32 GridWidth,
    int32 GridHeight,
    int32& OutCol,
    int32& OutRow) const
{
    if (GridWidth <= 0 || GridHeight <= 0 || GridSize.X <= 0.f || GridSize.Y <= 0.f)
    {
        return false;
    }

    // SOT: stride from actual rendered geometry (accounts for SlotPadding,
    // SizeBox overrides, and any layout stretching automatically)
    const float StrideX = GridSize.X / static_cast<float>(GridWidth);
    const float StrideY = GridSize.Y / static_cast<float>(GridHeight);
    OutCol = FMath::FloorToInt(LocalPos.X / StrideX);
    OutRow = FMath::FloorToInt(LocalPos.Y / StrideY);

    return OutCol >= 0 && OutRow >= 0 && OutCol < GridWidth && OutRow < GridHeight;
}

bool FProjectUIGridHitDetector::ResolveDualGridCoords(
    UUniformGridPanel* PrimaryPanel,
    int32 PrimaryWidth,
    int32 PrimaryHeight,
    UUniformGridPanel* SecondaryPanel,
    int32 SecondaryWidth,
    int32 SecondaryHeight,
    const FVector2D& ScreenPos,
    int32& OutCol,
    int32& OutRow,
    int32& OutGridWidth,
    int32& OutGridHeight,
    bool& bOutIsSecondary) const
{
    OutCol = INDEX_NONE;
    OutRow = INDEX_NONE;
    OutGridWidth = 0;
    OutGridHeight = 0;
    bOutIsSecondary = false;

    auto TryResolve = [&](UUniformGridPanel* Panel, int32 Width, int32 Height) -> bool
    {
        if (!Panel || Width <= 0 || Height <= 0)
        {
            return false;
        }

        const FGeometry GridGeometry = Panel->GetCachedGeometry();
        if (!GridGeometry.IsUnderLocation(ScreenPos))
        {
            return false;
        }

        const FVector2D LocalPos = GridGeometry.AbsoluteToLocal(ScreenPos);
        const FVector2D GridSize = GridGeometry.GetLocalSize();
        if (!LocalPosToGridCoords(LocalPos, GridSize, Width, Height, OutCol, OutRow))
        {
            return false;
        }

        OutGridWidth = Width;
        OutGridHeight = Height;
        return true;
    };

    if (TryResolve(PrimaryPanel, PrimaryWidth, PrimaryHeight))
    {
        bOutIsSecondary = false;
        return true;
    }

    if (TryResolve(SecondaryPanel, SecondaryWidth, SecondaryHeight))
    {
        bOutIsSecondary = true;
        return true;
    }

    return false;
}
