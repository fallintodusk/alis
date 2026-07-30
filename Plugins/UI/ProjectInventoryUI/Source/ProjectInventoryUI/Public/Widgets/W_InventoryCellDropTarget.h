// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "Interaction/IInventoryDropTarget.h"
#include "W_InventoryCellDropTarget.generated.h"

class UProjectGridCell;
class UInventoryUIDragHostSubsystem;
class UInventoryDragDropOperation;
class UInventoryViewModel;

/**
 * Smallest-semantic drop target for a single inventory grid cell.
 *
 * This is a thin wrapper UUserWidget that hosts a UProjectGridCell as
 * its root content. The visuals still live on UProjectGridCell (kept
 * unchanged in ProjectUI); this wrapper exists purely to give each
 * cell its own UUserWidget with overridable NativeOnDragOver /
 * NativeOnDrop handlers, since those virtuals exist only on UUserWidget.
 *
 * Why a wrapper instead of overriding on UProjectGridCell:
 *   - UProjectGridCell derives from UBorder, not UUserWidget.
 *   - NativeOnDragOver / NativeOnDrop live on UUserWidget in UE 5.7.
 *   - Moving UProjectGridCell to UUserWidget would disturb ProjectUI
 *     (domain-agnostic framework) with inventory-only concerns. Keeping
 *     the wrapper inside ProjectInventoryUI preserves plugin boundaries.
 *
 * Drop contract (Slice 17):
 *   - Construction: owning builder calls SetCellIdentity(SurfaceTag,
 *     CellIndex, GridWidth). Identity is set once at build time.
 *   - NativeOnDragOver: forwards the screen pos + DragOp payload to
 *     UInventoryUIDragHostSubsystem::UpdatePreview and returns
 *     FReply::Handled so Slate keeps routing moves to this cell.
 *   - NativeOnDrop: forwards to UInventoryUIDragHostSubsystem::CompleteDrop.
 *     Returns FReply::Handled on successful VM dispatch, FReply::Unhandled
 *     when the subsystem rejects the drop (so the event bubbles and a
 *     higher-priority handler - e.g. UW_InventoryPanel's equip path - can
 *     still accept it in Slice 17).
 *
 * Implements IInventoryDropTarget so the Slice 20 fitness test marks
 * this class as allowed to override drag handlers.
 *
 * Layout contract: content is the wrapped UProjectGridCell; the wrapper
 * itself sets a pass-through visibility (Visible) and no padding, so
 * grid dimensions remain identical to the un-wrapped layout. See the
 * grid builder's BuildGrid call site for the zero-pixel-shift proof
 * (UniformGridSlot -> SizeBox -> UW_InventoryCellDropTarget -> UProjectGridCell).
 */
UCLASS()
class PROJECTINVENTORYUI_API UW_InventoryCellDropTarget
    : public UUserWidget
    , public IInventoryDropTarget
{
    GENERATED_BODY()

public:
    UW_InventoryCellDropTarget(const FObjectInitializer& ObjectInitializer);

    /**
     * Seed the cell identity. Called once by FInventoryPanelGridBuilder
     * after constructing the wrapper and before adding it to the layout.
     *
     * SurfaceTag - registered surface under which this cell's grid is
     *   published with the drag host subsystem. Drives router dispatch.
     * CellIndex - index inside the grid (row-major). Drives target-cell
     *   resolution for logging and event-bus SourceCell reporting.
     * GridWidth - used to expose (col, row) for diagnostics.
     *
     * The wrapper is intentionally stateless beyond these three fields -
     * all drop validation happens via the subsystem's controller against
     * the registered surface, so the wrapper does not mirror surface
     * state.
     */
    void SetCellIdentity(const FGameplayTag& InSurfaceTag, int32 InCellIndex, int32 InGridWidth);

    /**
     * Set the hosted cell visual. Builder calls this with the already-
     * constructed UProjectGridCell so the wrapper's root content slot
     * hosts it. The wrapper takes no ownership beyond parenting through
     * the widget tree.
     */
    void SetHostedCell(UProjectGridCell* InCell);

    const FGameplayTag& GetSurfaceTag() const { return SurfaceTag; }
    int32 GetCellIndex() const { return CellIndex; }

    // Drag-drop overrides are public to match UUserWidget's public virtual
    // signatures (the engine calls them through a pointer-to-base). The
    // Slice 17 E2E tests also call them directly with synthetic events
    // to pin the forwarding contract.
    virtual bool NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual void NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

private:
    /**
     * Resolve the drag host subsystem via the owning local player.
     * Returns nullptr outside a real player context (e.g. archetype
     * evaluation) so callers must null-check.
     */
    UInventoryUIDragHostSubsystem* ResolveDragHost() const;

    /** Registered surface tag the wrapper's grid was published under. */
    UPROPERTY()
    FGameplayTag SurfaceTag;

    /** Row-major index within the grid. */
    UPROPERTY()
    int32 CellIndex = INDEX_NONE;

    /** Grid width; used for (col, row) derivation in diagnostics. */
    UPROPERTY()
    int32 GridWidth = 0;

    /** Hosted visual cell. Owned via the widget tree, not by us directly. */
    UPROPERTY()
    TObjectPtr<UProjectGridCell> HostedCell;
};
