// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interaction/ProjectUIGridHitDetector.h"

class UUniformGridPanel;

/**
 * Generic drag payload required for grid preview/drop checks.
 */
struct FProjectUIGridDragPayload
{
    int32 InstanceId = INDEX_NONE;
    FIntPoint ItemSize = FIntPoint(1, 1);
};

/**
 * Result of drag preview calculation.
 */
struct FProjectUIGridDragPreviewResult
{
    bool bActive = false;
    bool bSecondary = false;
    bool bValid = false;
    TSet<int32> PrimaryCells;
    TSet<int32> SecondaryCells;

    void Reset()
    {
        bActive = false;
        bSecondary = false;
        bValid = false;
        PrimaryCells.Reset();
        SecondaryCells.Reset();
    }
};

/**
 * Generic grid drag/drop controller.
 * Domain rules are provided via callbacks.
 */
class PROJECTUI_API FProjectUIGridDragDropController
{
public:
    FProjectUIGridDragDropController() = default;

    /**
     * Validate drop footprint against bounds, enabled cells, and occupancy rules.
     * OccupantChecker contract: return INDEX_NONE for empty cells.
     */
    static bool ValidateFootprint(
        const FProjectUIGridDragPayload& DragPayload,
        int32 Col,
        int32 Row,
        int32 GridWidth,
        int32 GridHeight,
        bool bSecondary,
        TFunctionRef<bool(bool bSecondary, int32 CellIndex)> EnabledChecker,
        TFunctionRef<int32(bool bSecondary, int32 CellIndex)> OccupantChecker);

    void Initialize(FProjectUIGridHitDetector* InHitDetector);

    void UpdatePreview(
        const FVector2D& ScreenPos,
        const FProjectUIGridDragPayload& DragPayload,
        UUniformGridPanel* PrimaryGrid,
        int32 PrimaryWidth,
        int32 PrimaryHeight,
        UUniformGridPanel* SecondaryGrid,
        int32 SecondaryWidth,
        int32 SecondaryHeight,
        TFunctionRef<bool(bool bSecondary, int32 CellIndex)> EnabledChecker,
        TFunctionRef<int32(bool bSecondary, int32 CellIndex)> OccupantChecker);

    void ClearPreview();

    const FProjectUIGridDragPreviewResult& GetPreviewResult() const { return PreviewResult; }
    bool IsDropValid() const { return PreviewResult.bActive && PreviewResult.bValid; }

    bool ResolveDropGridTarget(
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
        bool& bOutIsSecondary) const;

private:
    FProjectUIGridHitDetector* HitDetector = nullptr;
    FProjectUIGridDragPreviewResult PreviewResult;
};
