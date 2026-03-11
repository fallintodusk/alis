// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/UniformGridPanel.h"
#include "Components/Widget.h"

/**
 * Generic grid hit detection result.
 */
struct FProjectUIGridHitResult
{
    int32 CellIndex = INDEX_NONE;
    bool bIsSecondary = false;

    bool IsValid() const { return CellIndex != INDEX_NONE; }
    void Reset()
    {
        CellIndex = INDEX_NONE;
        bIsSecondary = false;
    }
};

/**
 * Generic hit detector for dual-grid UIs.
 * Computes cell coordinates from actual rendered grid geometry (SOT).
 */
class PROJECTUI_API FProjectUIGridHitDetector
{
public:
    FProjectUIGridHitDetector() = default;

    void SetCellSize(float InCellSize) { CellSize = InCellSize; }
    float GetCellSize() const { return CellSize; }

    bool ResolveGridHit(
        UUniformGridPanel* Panel,
        int32 GridWidth,
        int32 GridHeight,
        const FVector2D& ScreenPos,
        int32& OutCellIndex) const;

    bool ResolveDualGridHit(
        UUniformGridPanel* PrimaryPanel,
        int32 PrimaryWidth,
        int32 PrimaryHeight,
        UUniformGridPanel* SecondaryPanel,
        int32 SecondaryWidth,
        int32 SecondaryHeight,
        const FVector2D& ScreenPos,
        FProjectUIGridHitResult& OutResult) const;

    template<typename TWidgetType>
    bool TryGetWidgetIndexAtScreenPos(
        const TArray<TObjectPtr<TWidgetType>>& Widgets,
        const FVector2D& ScreenPos,
        int32& OutIndex) const
    {
        OutIndex = INDEX_NONE;

        for (int32 Index = 0; Index < Widgets.Num(); ++Index)
        {
            const UWidget* Widget = Widgets[Index];
            if (!Widget)
            {
                continue;
            }

            if (Widget->GetCachedGeometry().IsUnderLocation(ScreenPos))
            {
                OutIndex = Index;
                return true;
            }
        }

        return false;
    }

    // Geometry-based: stride = GridSize / GridDimensions
    bool LocalPosToGridCoords(
        const FVector2D& LocalPos,
        const FVector2D& GridSize,
        int32 GridWidth,
        int32 GridHeight,
        int32& OutCol,
        int32& OutRow) const;

    static int32 GridCoordsToIndex(int32 Col, int32 Row, int32 GridWidth)
    {
        return Row * GridWidth + Col;
    }

    static void IndexToGridCoords(int32 Index, int32 GridWidth, int32& OutCol, int32& OutRow)
    {
        OutCol = Index % GridWidth;
        OutRow = Index / GridWidth;
    }

    bool ResolveDualGridCoords(
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
        bool& bOutIsSecondary) const;

private:
    float CellSize = 64.f;
};
