// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class UUniformGridPanel;

/**
 * Generic drag payload required for grid preview/drop checks.
 *
 * Surface callbacks (EnabledChecker / OccupantChecker / OccupantAllowedChecker)
 * receive this struct verbatim. They MUST NOT reach into global Slate
 * state (e.g. UWidgetBlueprintLibrary::GetDragDroppingContent) - that
 * has been observed to return null in early/late frames of the drag
 * lifecycle, which makes occupancy validation reject every cell. The
 * payload carries everything the validation rule needs.
 */
struct FProjectUIGridDragPayload
{
    int32 InstanceId = INDEX_NONE;
    FIntPoint ItemSize = FIntPoint(1, 1);
    int32 Quantity = 0;
    FGameplayTag SourceSurfaceTag;
};

/**
 * Registration describing a single grid surface that can receive a drop.
 * The controller indexes these by SurfaceTag and picks the surface whose
 * cached geometry is under the cursor - no boolean primary/secondary
 * assumption. Callers (inventory UI) pick the tag (e.g. Item.Container.*)
 * and feed domain-specific callbacks.
 *
 * Priority: when two surfaces overlap on screen (e.g. narrow viewport
 * or deliberate overlap), the surface with the HIGHER Priority value
 * wins the hit test. Ties break in registration order. Explicit
 * priorities keep routing deterministic and independent of registration
 * order; rely on the implicit default only if you are sure surfaces
 * cannot overlap.
 */
struct FProjectUIGridSurface
{
    FGameplayTag SurfaceTag;
    TWeakObjectPtr<UUniformGridPanel> Grid;
    FIntPoint Dims = FIntPoint::ZeroValue;
    /** Higher value = resolved first when surfaces overlap. */
    int32 Priority = 0;
    TFunction<bool(int32 CellIndex)> EnabledChecker;
    TFunction<int32(int32 CellIndex)> OccupantChecker;
    // Optional; when null, controller falls back to the "self or empty" rule.
    TFunction<bool(int32 CellIndex, int32 OccupantId, const FProjectUIGridDragPayload& Payload)> OccupantAllowedChecker;
};

/**
 * Result of drag preview calculation. Populated by
 * UpdatePreviewOverSurfaces; consumed by widgets via GetPreviewResult().
 *
 * All surfaces are tag-indexed. No boolean primary/secondary distinction:
 * overlap routing is deterministic through Priority (higher wins) with
 * ties preserving registration order.
 */
struct FProjectUIGridDragPreviewResult
{
    bool bActive = false;
    bool bValid = false;

    /** Surface the cursor is currently over. */
    FGameplayTag TargetSurfaceTag;

    /** Cells covered by the drag footprint, keyed by surface tag. */
    TMap<FGameplayTag, TSet<int32>> PreviewCellsBySurface;

    void Reset()
    {
        bActive = false;
        bValid = false;
        TargetSurfaceTag = FGameplayTag();
        PreviewCellsBySurface.Reset();
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

    /**
     * Validate drop footprint with a domain-supplied occupancy rule.
     * OccupantAllowedChecker receives the occupant returned by OccupantChecker
     * and decides whether that occupant may accept the drop.
     */
    static bool ValidateFootprintWithRule(
        const FProjectUIGridDragPayload& DragPayload,
        int32 Col,
        int32 Row,
        int32 GridWidth,
        int32 GridHeight,
        bool bSecondary,
        TFunctionRef<bool(bool bSecondary, int32 CellIndex)> EnabledChecker,
        TFunctionRef<int32(bool bSecondary, int32 CellIndex)> OccupantChecker,
        TFunctionRef<bool(bool bSecondary, int32 CellIndex, int32 OccupantId)> OccupantAllowedChecker);

    void ClearPreview();

    const FProjectUIGridDragPreviewResult& GetPreviewResult() const { return PreviewResult; }
    bool IsDropValid() const { return PreviewResult.bActive && PreviewResult.bValid; }

    // -------------------------------------------------------------------
    // Tag-indexed surface API. Callers register each drop surface with a
    // stable FGameplayTag; the controller resolves hits by iterating
    // registered surfaces (priority descending; ties preserve registration
    // order) and matching the cursor against each grid's cached geometry.
    // -------------------------------------------------------------------

    /** Register or replace a surface keyed by SurfaceTag. */
    void RegisterSurface(FProjectUIGridSurface Surface);

    /** Remove a surface registration. No-op if tag not present. */
    void UnregisterSurface(const FGameplayTag& SurfaceTag);

    /** Drop all registered surfaces (called on teardown or scope change). */
    void ClearSurfaces();

    /** True iff a surface with this tag is currently registered. */
    bool HasSurface(const FGameplayTag& SurfaceTag) const;

    /** Returns the number of registered surfaces. */
    int32 GetSurfaceCount() const;

    /**
     * Surface tags in the exact order hit-testing visits them (priority
     * descending, ties preserve registration order). Exposed so tests
     * and diagnostics can assert overlap-resolution ordering without
     * reaching into the private array.
     */
    TArray<FGameplayTag> GetSurfaceTagsInPriorityOrder() const;

    /** Update preview across all registered surfaces. */
    void UpdatePreviewOverSurfaces(
        const FVector2D& ScreenPos,
        const FProjectUIGridDragPayload& DragPayload);

    /**
     * Update preview for an explicit surface/cell chosen by a smaller
     * semantic target (for example a per-cell wrapper). This bypasses the
     * controller's cross-surface hit test but keeps the same footprint
     * validation and preview-cell bookkeeping.
     */
    void UpdatePreviewOnSurface(
        const FGameplayTag& SurfaceTag,
        int32 CellIndex,
        const FProjectUIGridDragPayload& DragPayload);

    /**
     * Resolve the drop target across registered surfaces. Returns true iff
     * the cursor is over a surface AND the footprint validates against its
     * rules. OutSurfaceTag carries the SOT; OutCol/OutRow are grid-local.
     */
    bool ResolveDropTargetOverSurfaces(
        const FVector2D& ScreenPos,
        const FProjectUIGridDragPayload& DragPayload,
        FGameplayTag& OutSurfaceTag,
        int32& OutCol,
        int32& OutRow) const;

    /**
     * Validate an explicit surface/cell chosen by a smaller semantic target.
     * No geometry or overlap resolution is performed; callers use this after
     * Slate already routed the event to the intended drop widget.
     */
    bool ResolveDropTargetOnSurface(
        const FGameplayTag& SurfaceTag,
        int32 CellIndex,
        const FProjectUIGridDragPayload& DragPayload,
        int32& OutCol,
        int32& OutRow) const;

    /**
     * Pure source-side hit test: which registered surface+cell is under
     * the cursor, without any footprint or occupancy validation.
     *
     * Use this at drag-start time when the question is "which cell did
     * the user click?" - it must NOT be conflated with drop validation
     * (ResolveDropTargetOverSurfaces) because a drag source is by
     * definition an occupied cell, and the drop rule rejects occupied
     * cells for an unrelated payload.
     */
    bool ResolveSurfaceCellAtScreenPos(
        const FVector2D& ScreenPos,
        FGameplayTag& OutSurfaceTag,
        int32& OutCol,
        int32& OutRow) const;

private:
    void SortSurfacesByPriority();

    FProjectUIGridDragPreviewResult PreviewResult;
    TArray<FProjectUIGridSurface> RegisteredSurfaces;
};
