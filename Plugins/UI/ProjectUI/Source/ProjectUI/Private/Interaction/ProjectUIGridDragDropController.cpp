// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Interaction/ProjectUIGridDragDropController.h"
#include "Components/UniformGridPanel.h"
#include "HAL/IConsoleManager.h"
#include "Layout/Geometry.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectUIGridDrag, Log, All);

namespace
{
    static TAutoConsoleVariable<int32> CVarInventoryDragDiag(
        TEXT("inv.drag.diag"),
        0,
        TEXT("When 1, logs detailed inventory drag/drop hit-testing, surface geometry, and validation diagnostics."),
        ECVF_Default);

    bool IsInventoryDragDiagEnabled()
    {
#if !UE_BUILD_SHIPPING
        return CVarInventoryDragDiag.GetValueOnAnyThread() != 0;
#else
        return false;
#endif
    }

    const TCHAR* VisibilityToString(const ESlateVisibility Visibility)
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

    FString FormatRect(const FGeometry& Geometry)
    {
        const FVector2D LocalSize = Geometry.GetLocalSize();
        const FVector2D TopLeft = Geometry.LocalToAbsolute(FVector2D::ZeroVector);
        const FVector2D BottomRight = Geometry.LocalToAbsolute(LocalSize);
        return FString::Printf(
            TEXT("TL=(%.1f,%.1f) BR=(%.1f,%.1f) Size=(%.1f,%.1f)"),
            TopLeft.X,
            TopLeft.Y,
            BottomRight.X,
            BottomRight.Y,
            LocalSize.X,
            LocalSize.Y);
    }

    FString DescribePayload(const FProjectUIGridDragPayload& DragPayload)
    {
        return FString::Printf(
            TEXT("Instance=%d Qty=%d Size=%dx%d Source=%s"),
            DragPayload.InstanceId,
            DragPayload.Quantity,
            DragPayload.ItemSize.X,
            DragPayload.ItemSize.Y,
            *DragPayload.SourceSurfaceTag.ToString());
    }

    FString FormatSurfaceOrder(const TArray<FProjectUIGridSurface>& RegisteredSurfaces)
    {
        TArray<FString> Parts;
        Parts.Reserve(RegisteredSurfaces.Num());
        for (const FProjectUIGridSurface& Surface : RegisteredSurfaces)
        {
            Parts.Add(FString::Printf(
                TEXT("%s[p=%d]"),
                *Surface.SurfaceTag.ToString(),
                Surface.Priority));
        }
        return FString::Join(Parts, TEXT(" -> "));
    }

    void LogSurfaceState(
        const FProjectUIGridSurface& Surface,
        const TCHAR* Context,
        const FVector2D* ScreenPos,
        const FString* Extra)
    {
        if (!IsInventoryDragDiagEnabled())
        {
            return;
        }

        UUniformGridPanel* Grid = Surface.Grid.Get();
        if (!Grid)
        {
            UE_LOG(
                LogProjectUIGridDrag,
                Log,
                TEXT("%s Surface=%s Priority=%d Dims=%dx%d Grid=null %s"),
                Context,
                *Surface.SurfaceTag.ToString(),
                Surface.Priority,
                Surface.Dims.X,
                Surface.Dims.Y,
                Extra ? **Extra : TEXT(""));
            return;
        }

        const FGeometry Geometry = Grid->GetCachedGeometry();
        const bool bUnderCursor = ScreenPos ? Geometry.IsUnderLocation(*ScreenPos) : false;
        UE_LOG(
            LogProjectUIGridDrag,
            Log,
            TEXT("%s Surface=%s Priority=%d Dims=%dx%d Grid=%s Vis=%s Enabled=%d IsVisible=%d Rect=%s UnderCursor=%d %s"),
            Context,
            *Surface.SurfaceTag.ToString(),
            Surface.Priority,
            Surface.Dims.X,
            Surface.Dims.Y,
            *Grid->GetPathName(),
            VisibilityToString(Grid->GetVisibility()),
            Grid->GetIsEnabled() ? 1 : 0,
            Grid->IsVisible() ? 1 : 0,
            *FormatRect(Geometry),
            bUnderCursor ? 1 : 0,
            Extra ? **Extra : TEXT(""));
    }
}

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
    return ValidateFootprintWithRule(
        DragPayload,
        Col,
        Row,
        GridWidth,
        GridHeight,
        bSecondary,
        EnabledChecker,
        OccupantChecker,
        [&DragPayload](bool /*bSecondary*/, int32 /*CellIndex*/, int32 OccupantId)
        {
            return OccupantId == INDEX_NONE || OccupantId == DragPayload.InstanceId;
        });
}

bool FProjectUIGridDragDropController::ValidateFootprintWithRule(
    const FProjectUIGridDragPayload& DragPayload,
    int32 Col,
    int32 Row,
    int32 GridWidth,
    int32 GridHeight,
    bool bSecondary,
    TFunctionRef<bool(bool bSecondary, int32 CellIndex)> EnabledChecker,
    TFunctionRef<int32(bool bSecondary, int32 CellIndex)> OccupantChecker,
    TFunctionRef<bool(bool bSecondary, int32 CellIndex, int32 OccupantId)> OccupantAllowedChecker)
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
            if (!OccupantAllowedChecker(bSecondary, CellIndex, OccupantId))
            {
                return false;
            }
        }
    }

    return true;
}

void FProjectUIGridDragDropController::ClearPreview()
{
    PreviewResult.Reset();
}

// -------------------------------------------------------------------
// Tag-indexed surface API.
//
// Callers register drop surfaces with a stable FGameplayTag. Hit-testing
// iterates registered surfaces and picks the one whose cached geometry
// contains the cursor. No boolean primary/secondary assumption - any
// number of surfaces with distinct tags are supported.
// -------------------------------------------------------------------

namespace
{
    bool ResolveHitOnSurface(
        const FProjectUIGridSurface& Surface,
        const FVector2D& ScreenPos,
        int32& OutCol,
        int32& OutRow,
        FString* OutReason = nullptr)
    {
        OutCol = INDEX_NONE;
        OutRow = INDEX_NONE;

        UUniformGridPanel* Grid = Surface.Grid.Get();
        if (!Grid)
        {
            // Widget destroyed but surface still registered. Most likely
            // a lifecycle bug (missing UnregisterSurface on widget teardown)
            // or the subsystem holds a stale registration across scope
            // changes. Verbose so it does not spam but is catchable in dev.
            UE_LOG(LogProjectUIGridDrag, Verbose,
                TEXT("ResolveHitOnSurface: surface '%s' has a null/destroyed grid; skipped."),
                *Surface.SurfaceTag.ToString());
            if (OutReason)
            {
                *OutReason = TEXT("NullGrid");
            }
            return false;
        }
        if (Surface.Dims.X <= 0 || Surface.Dims.Y <= 0)
        {
            if (OutReason)
            {
                *OutReason = FString::Printf(TEXT("InvalidDims=%dx%d"), Surface.Dims.X, Surface.Dims.Y);
            }
            return false;
        }

        const FGeometry Geometry = Grid->GetCachedGeometry();
        if (!Geometry.IsUnderLocation(ScreenPos))
        {
            if (OutReason)
            {
                *OutReason = FString::Printf(
                    TEXT("ScreenPosMiss Pos=(%.1f,%.1f) Rect=%s"),
                    ScreenPos.X,
                    ScreenPos.Y,
                    *FormatRect(Geometry));
            }
            return false;
        }

        const FVector2D LocalPos = Geometry.AbsoluteToLocal(ScreenPos);
        const FVector2D GridSize = Geometry.GetLocalSize();
        if (GridSize.X <= 0.f || GridSize.Y <= 0.f)
        {
            if (OutReason)
            {
                *OutReason = FString::Printf(TEXT("ZeroLocalSize=(%.1f,%.1f)"), GridSize.X, GridSize.Y);
            }
            return false;
        }

        const float StrideX = GridSize.X / static_cast<float>(Surface.Dims.X);
        const float StrideY = GridSize.Y / static_cast<float>(Surface.Dims.Y);
        if (StrideX <= 0.f || StrideY <= 0.f)
        {
            if (OutReason)
            {
                *OutReason = FString::Printf(TEXT("ZeroStride=(%.3f,%.3f)"), StrideX, StrideY);
            }
            return false;
        }

        OutCol = FMath::FloorToInt(LocalPos.X / StrideX);
        OutRow = FMath::FloorToInt(LocalPos.Y / StrideY);

        const bool bInBounds = OutCol >= 0 && OutRow >= 0
            && OutCol < Surface.Dims.X && OutRow < Surface.Dims.Y;
        if (OutReason)
        {
            *OutReason = FString::Printf(
                TEXT("Local=(%.1f,%.1f) Cell=(%d,%d) InBounds=%d"),
                LocalPos.X,
                LocalPos.Y,
                OutCol,
                OutRow,
                bInBounds ? 1 : 0);
        }
        return bInBounds;
    }

    bool ValidateSurfaceFootprint(
        const FProjectUIGridSurface& Surface,
        const FProjectUIGridDragPayload& DragPayload,
        int32 Col,
        int32 Row,
        FString* OutRejectReason = nullptr)
    {
        if (DragPayload.ItemSize.X <= 0 || DragPayload.ItemSize.Y <= 0)
        {
            if (OutRejectReason)
            {
                *OutRejectReason = FString::Printf(
                    TEXT("InvalidPayloadSize=%dx%d"),
                    DragPayload.ItemSize.X,
                    DragPayload.ItemSize.Y);
            }
            return false;
        }

        const int32 MaxCol = Col + DragPayload.ItemSize.X;
        const int32 MaxRow = Row + DragPayload.ItemSize.Y;
        if (Col < 0 || Row < 0 || MaxCol > Surface.Dims.X || MaxRow > Surface.Dims.Y)
        {
            if (OutRejectReason)
            {
                *OutRejectReason = FString::Printf(
                    TEXT("FootprintOutOfBounds Cell=(%d,%d) Max=(%d,%d) SurfaceDims=%dx%d"),
                    Col,
                    Row,
                    MaxCol,
                    MaxRow,
                    Surface.Dims.X,
                    Surface.Dims.Y);
            }
            return false;
        }

        for (int32 OffsetY = 0; OffsetY < DragPayload.ItemSize.Y; ++OffsetY)
        {
            for (int32 OffsetX = 0; OffsetX < DragPayload.ItemSize.X; ++OffsetX)
            {
                const int32 X = Col + OffsetX;
                const int32 Y = Row + OffsetY;
                const int32 CellIndex = Y * Surface.Dims.X + X;

                if (Surface.EnabledChecker && !Surface.EnabledChecker(CellIndex))
                {
                    if (OutRejectReason)
                    {
                        *OutRejectReason = FString::Printf(
                            TEXT("DisabledCell Index=%d Cell=(%d,%d)"),
                            CellIndex,
                            X,
                            Y);
                    }
                    return false;
                }

                const int32 OccupantId = Surface.OccupantChecker
                    ? Surface.OccupantChecker(CellIndex)
                    : INDEX_NONE;

                const bool bAllowed = Surface.OccupantAllowedChecker
                    ? Surface.OccupantAllowedChecker(CellIndex, OccupantId, DragPayload)
                    : (OccupantId == INDEX_NONE || OccupantId == DragPayload.InstanceId);
                if (!bAllowed)
                {
                    if (OutRejectReason)
                    {
                        *OutRejectReason = FString::Printf(
                            TEXT("OccupantRejected Index=%d Cell=(%d,%d) Occupant=%d DragInstance=%d"),
                            CellIndex,
                            X,
                            Y,
                            OccupantId,
                            DragPayload.InstanceId);
                    }
                    return false;
                }
            }
        }

        return true;
    }

    bool TryResolveExplicitSurfaceCell(
        const TArray<FProjectUIGridSurface>& RegisteredSurfaces,
        const FGameplayTag& SurfaceTag,
        int32 CellIndex,
        const FProjectUIGridDragPayload& DragPayload,
        int32& OutCol,
        int32& OutRow,
        FString* OutRejectReason = nullptr)
    {
        OutCol = INDEX_NONE;
        OutRow = INDEX_NONE;

        const FProjectUIGridSurface* Surface = RegisteredSurfaces.FindByPredicate(
            [&SurfaceTag](const FProjectUIGridSurface& Existing)
            {
                return Existing.SurfaceTag == SurfaceTag;
            });
        if (!Surface)
        {
            if (OutRejectReason)
            {
                *OutRejectReason = FString::Printf(TEXT("UnknownSurface=%s"), *SurfaceTag.ToString());
            }
            return false;
        }

        if (Surface->Dims.X <= 0 || Surface->Dims.Y <= 0)
        {
            if (OutRejectReason)
            {
                *OutRejectReason = FString::Printf(TEXT("InvalidDims=%dx%d"), Surface->Dims.X, Surface->Dims.Y);
            }
            return false;
        }

        if (CellIndex < 0 || CellIndex >= Surface->Dims.X * Surface->Dims.Y)
        {
            if (OutRejectReason)
            {
                *OutRejectReason = FString::Printf(
                    TEXT("CellIndexOutOfBounds=%d SurfaceDims=%dx%d"),
                    CellIndex,
                    Surface->Dims.X,
                    Surface->Dims.Y);
            }
            return false;
        }

        OutCol = CellIndex % Surface->Dims.X;
        OutRow = CellIndex / Surface->Dims.X;
        return ValidateSurfaceFootprint(*Surface, DragPayload, OutCol, OutRow, OutRejectReason);
    }

    void PopulatePreviewForSurfaceCell(
        FProjectUIGridDragPreviewResult& PreviewResult,
        const FProjectUIGridSurface& Surface,
        int32 Col,
        int32 Row,
        const FProjectUIGridDragPayload& DragPayload)
    {
        PreviewResult.Reset();
        if (DragPayload.ItemSize.X <= 0 || DragPayload.ItemSize.Y <= 0)
        {
            return;
        }

        PreviewResult.bActive = true;
        PreviewResult.TargetSurfaceTag = Surface.SurfaceTag;
        PreviewResult.bValid = true;

        TSet<int32>& Cells = PreviewResult.PreviewCellsBySurface.FindOrAdd(Surface.SurfaceTag);

        for (int32 OffsetY = 0; OffsetY < DragPayload.ItemSize.Y; ++OffsetY)
        {
            for (int32 OffsetX = 0; OffsetX < DragPayload.ItemSize.X; ++OffsetX)
            {
                const int32 X = Col + OffsetX;
                const int32 Y = Row + OffsetY;
                if (X < 0 || Y < 0 || X >= Surface.Dims.X || Y >= Surface.Dims.Y)
                {
                    PreviewResult.bValid = false;
                    continue;
                }

                const int32 ExplicitCellIndex = Y * Surface.Dims.X + X;
                if (Surface.EnabledChecker && !Surface.EnabledChecker(ExplicitCellIndex))
                {
                    PreviewResult.bValid = false;
                }

                const int32 OccupantId = Surface.OccupantChecker
                    ? Surface.OccupantChecker(ExplicitCellIndex)
                    : INDEX_NONE;
                const bool bAllowed = Surface.OccupantAllowedChecker
                    ? Surface.OccupantAllowedChecker(ExplicitCellIndex, OccupantId, DragPayload)
                    : (OccupantId == INDEX_NONE || OccupantId == DragPayload.InstanceId);
                if (!bAllowed)
                {
                    PreviewResult.bValid = false;
                }

                Cells.Add(ExplicitCellIndex);
            }
        }
    }
}

void FProjectUIGridDragDropController::RegisterSurface(FProjectUIGridSurface Surface)
{
    if (!Surface.SurfaceTag.IsValid())
    {
        return;
    }

    const FGameplayTag IncomingTag = Surface.SurfaceTag;

    for (FProjectUIGridSurface& Existing : RegisteredSurfaces)
    {
        if (Existing.SurfaceTag == Surface.SurfaceTag)
        {
            Existing = MoveTemp(Surface);
            SortSurfacesByPriority();
            if (IsInventoryDragDiagEnabled())
            {
                UE_LOG(
                    LogProjectUIGridDrag,
                    Log,
                    TEXT("RegisterSurface: replaced %s Order=%s"),
                    *Existing.SurfaceTag.ToString(),
                    *FormatSurfaceOrder(RegisteredSurfaces));
                LogSurfaceState(Existing, TEXT("RegisterSurface"), nullptr, nullptr);
            }
            return;
        }
    }

    RegisteredSurfaces.Add(MoveTemp(Surface));
    SortSurfacesByPriority();
    if (IsInventoryDragDiagEnabled())
    {
        const FProjectUIGridSurface* Added = RegisteredSurfaces.FindByPredicate(
            [IncomingTag](const FProjectUIGridSurface& Existing)
            {
                return Existing.SurfaceTag == IncomingTag;
            });
        UE_LOG(
            LogProjectUIGridDrag,
            Log,
            TEXT("RegisterSurface: added Order=%s"),
            *FormatSurfaceOrder(RegisteredSurfaces));
        if (Added)
        {
            LogSurfaceState(*Added, TEXT("RegisterSurface"), nullptr, nullptr);
        }
    }
}

void FProjectUIGridDragDropController::SortSurfacesByPriority()
{
    // Higher priority first; stable so ties preserve registration order.
    RegisteredSurfaces.StableSort(
        [](const FProjectUIGridSurface& A, const FProjectUIGridSurface& B)
        {
            return A.Priority > B.Priority;
        });
}

void FProjectUIGridDragDropController::UnregisterSurface(const FGameplayTag& SurfaceTag)
{
    if (!SurfaceTag.IsValid())
    {
        return;
    }

    const int32 BeforeCount = RegisteredSurfaces.Num();
    RegisteredSurfaces.RemoveAll([&SurfaceTag](const FProjectUIGridSurface& Surface)
    {
        return Surface.SurfaceTag == SurfaceTag;
    });

    if (IsInventoryDragDiagEnabled())
    {
        UE_LOG(
            LogProjectUIGridDrag,
            Log,
            TEXT("UnregisterSurface: %s Removed=%d Order=%s"),
            *SurfaceTag.ToString(),
            BeforeCount != RegisteredSurfaces.Num() ? 1 : 0,
            *FormatSurfaceOrder(RegisteredSurfaces));
    }
}

void FProjectUIGridDragDropController::ClearSurfaces()
{
    RegisteredSurfaces.Reset();
}

bool FProjectUIGridDragDropController::HasSurface(const FGameplayTag& SurfaceTag) const
{
    if (!SurfaceTag.IsValid())
    {
        return false;
    }
    for (const FProjectUIGridSurface& Surface : RegisteredSurfaces)
    {
        if (Surface.SurfaceTag == SurfaceTag)
        {
            return true;
        }
    }
    return false;
}

int32 FProjectUIGridDragDropController::GetSurfaceCount() const
{
    return RegisteredSurfaces.Num();
}

TArray<FGameplayTag> FProjectUIGridDragDropController::GetSurfaceTagsInPriorityOrder() const
{
    TArray<FGameplayTag> Tags;
    Tags.Reserve(RegisteredSurfaces.Num());
    for (const FProjectUIGridSurface& Surface : RegisteredSurfaces)
    {
        Tags.Add(Surface.SurfaceTag);
    }
    return Tags;
}

void FProjectUIGridDragDropController::UpdatePreviewOverSurfaces(
    const FVector2D& ScreenPos,
    const FProjectUIGridDragPayload& DragPayload)
{
    ClearPreview();

    if (DragPayload.ItemSize.X <= 0 || DragPayload.ItemSize.Y <= 0)
    {
        return;
    }

    for (const FProjectUIGridSurface& Surface : RegisteredSurfaces)
    {
        int32 Col = INDEX_NONE;
        int32 Row = INDEX_NONE;
        if (!ResolveHitOnSurface(Surface, ScreenPos, Col, Row))
        {
            continue;
        }

        PopulatePreviewForSurfaceCell(PreviewResult, Surface, Col, Row, DragPayload);
        return; // first hit wins
    }
}

void FProjectUIGridDragDropController::UpdatePreviewOnSurface(
    const FGameplayTag& SurfaceTag,
    int32 CellIndex,
    const FProjectUIGridDragPayload& DragPayload)
{
    ClearPreview();

    if (DragPayload.ItemSize.X <= 0 || DragPayload.ItemSize.Y <= 0)
    {
        return;
    }

    const FProjectUIGridSurface* Surface = RegisteredSurfaces.FindByPredicate(
        [&SurfaceTag](const FProjectUIGridSurface& Existing)
        {
            return Existing.SurfaceTag == SurfaceTag;
        });
    if (!Surface || Surface->Dims.X <= 0 || Surface->Dims.Y <= 0)
    {
        return;
    }

    const int32 MaxCellCount = Surface->Dims.X * Surface->Dims.Y;
    if (CellIndex < 0 || CellIndex >= MaxCellCount)
    {
        return;
    }

    const int32 Col = CellIndex % Surface->Dims.X;
    const int32 Row = CellIndex / Surface->Dims.X;
    PopulatePreviewForSurfaceCell(PreviewResult, *Surface, Col, Row, DragPayload);
}

bool FProjectUIGridDragDropController::ResolveDropTargetOverSurfaces(
    const FVector2D& ScreenPos,
    const FProjectUIGridDragPayload& DragPayload,
    FGameplayTag& OutSurfaceTag,
    int32& OutCol,
    int32& OutRow) const
{
    OutSurfaceTag = FGameplayTag();
    OutCol = INDEX_NONE;
    OutRow = INDEX_NONE;

    if (IsInventoryDragDiagEnabled())
    {
        UE_LOG(
            LogProjectUIGridDrag,
            Log,
            TEXT("ResolveDropTargetOverSurfaces: Screen=(%.1f,%.1f) Payload={%s} Order=%s"),
            ScreenPos.X,
            ScreenPos.Y,
            *DescribePayload(DragPayload),
            *FormatSurfaceOrder(RegisteredSurfaces));
    }

    for (int32 SurfaceIndex = 0; SurfaceIndex < RegisteredSurfaces.Num(); ++SurfaceIndex)
    {
        const FProjectUIGridSurface& Surface = RegisteredSurfaces[SurfaceIndex];
        int32 Col = INDEX_NONE;
        int32 Row = INDEX_NONE;
        FString HitReason;
        if (!ResolveHitOnSurface(Surface, ScreenPos, Col, Row, &HitReason))
        {
            LogSurfaceState(Surface, TEXT("ResolveDropTargetOverSurfaces: miss"), &ScreenPos, &HitReason);
            continue;
        }

        LogSurfaceState(
            Surface,
            TEXT("ResolveDropTargetOverSurfaces: hit"),
            &ScreenPos,
            &HitReason);

        FString RejectReason;
        if (!ValidateSurfaceFootprint(Surface, DragPayload, Col, Row, &RejectReason))
        {
            if (IsInventoryDragDiagEnabled())
            {
                UE_LOG(
                    LogProjectUIGridDrag,
                    Warning,
                    TEXT("ResolveDropTargetOverSurfaces: hit %s Cell=(%d,%d) but rejected: %s. Lower-priority surfaces will not be considered."),
                    *Surface.SurfaceTag.ToString(),
                    Col,
                    Row,
                    *RejectReason);

                for (int32 NextIndex = SurfaceIndex + 1; NextIndex < RegisteredSurfaces.Num(); ++NextIndex)
                {
                    const FProjectUIGridSurface& OverlapSurface = RegisteredSurfaces[NextIndex];
                    int32 OverlapCol = INDEX_NONE;
                    int32 OverlapRow = INDEX_NONE;
                    FString OverlapReason;
                    if (ResolveHitOnSurface(OverlapSurface, ScreenPos, OverlapCol, OverlapRow, &OverlapReason))
                    {
                        UE_LOG(
                            LogProjectUIGridDrag,
                            Warning,
                            TEXT("ResolveDropTargetOverSurfaces: overlap candidate beneath rejected surface -> %s Cell=(%d,%d) Priority=%d %s"),
                            *OverlapSurface.SurfaceTag.ToString(),
                            OverlapCol,
                            OverlapRow,
                            OverlapSurface.Priority,
                            *OverlapReason);
                        LogSurfaceState(
                            OverlapSurface,
                            TEXT("ResolveDropTargetOverSurfaces: overlap"),
                            &ScreenPos,
                            &OverlapReason);
                    }
                }
            }
            return false;
        }

        OutSurfaceTag = Surface.SurfaceTag;
        OutCol = Col;
        OutRow = Row;
        if (IsInventoryDragDiagEnabled())
        {
            UE_LOG(
                LogProjectUIGridDrag,
                Log,
                TEXT("ResolveDropTargetOverSurfaces: resolved Surface=%s Cell=(%d,%d)"),
                *OutSurfaceTag.ToString(),
                OutCol,
                OutRow);
        }
        return true;
    }

    if (IsInventoryDragDiagEnabled())
    {
        UE_LOG(
            LogProjectUIGridDrag,
            Warning,
            TEXT("ResolveDropTargetOverSurfaces: no registered surface hit at Screen=(%.1f,%.1f)."),
            ScreenPos.X,
            ScreenPos.Y);
    }
    return false;
}

bool FProjectUIGridDragDropController::ResolveSurfaceCellAtScreenPos(
    const FVector2D& ScreenPos,
    FGameplayTag& OutSurfaceTag,
    int32& OutCol,
    int32& OutRow) const
{
    OutSurfaceTag = FGameplayTag();
    OutCol = INDEX_NONE;
    OutRow = INDEX_NONE;

    for (const FProjectUIGridSurface& Surface : RegisteredSurfaces)
    {
        int32 Col = INDEX_NONE;
        int32 Row = INDEX_NONE;
        if (!ResolveHitOnSurface(Surface, ScreenPos, Col, Row))
        {
            continue;
        }

        OutSurfaceTag = Surface.SurfaceTag;
        OutCol = Col;
        OutRow = Row;
        return true;
    }

    return false;
}

bool FProjectUIGridDragDropController::ResolveDropTargetOnSurface(
    const FGameplayTag& SurfaceTag,
    int32 CellIndex,
    const FProjectUIGridDragPayload& DragPayload,
    int32& OutCol,
    int32& OutRow) const
{
    FString RejectReason;
    const bool bResolved = TryResolveExplicitSurfaceCell(
        RegisteredSurfaces,
        SurfaceTag,
        CellIndex,
        DragPayload,
        OutCol,
        OutRow,
        &RejectReason);

    if (IsInventoryDragDiagEnabled())
    {
        if (bResolved)
        {
            UE_LOG(
                LogProjectUIGridDrag,
                Log,
                TEXT("ResolveDropTargetOnSurface: Surface=%s CellIndex=%d Payload={%s} Resolved=%d Cell=(%d,%d) Reason=%s"),
                *SurfaceTag.ToString(),
                CellIndex,
                *DescribePayload(DragPayload),
                1,
                OutCol,
                OutRow,
                *RejectReason);
        }
        else
        {
            UE_LOG(
                LogProjectUIGridDrag,
                Warning,
                TEXT("ResolveDropTargetOnSurface: Surface=%s CellIndex=%d Payload={%s} Resolved=%d Cell=(%d,%d) Reason=%s"),
                *SurfaceTag.ToString(),
                CellIndex,
                *DescribePayload(DragPayload),
                0,
                OutCol,
                OutRow,
                *RejectReason);
        }
    }

    return bResolved;
}
