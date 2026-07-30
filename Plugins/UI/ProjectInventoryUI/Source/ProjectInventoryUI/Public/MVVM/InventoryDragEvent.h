// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Delegates/DelegateCombinations.h"
#include "InventoryDragEvent.generated.h"

/**
 * Structured event stream for the inventory drag session.
 *
 * Slate does not ship a domain-level observable for drag decisions
 * ("dispatcher rejected with reason=SelfOverlap", "router invoked
 * RequestStoreItemInNearbyContainerAt"). This enum + event struct is
 * the thin domain layer the engine is missing and is consumed by:
 *   - FInventoryDragEventRecorder (test-side sequence assertions)
 *   - FInventoryDragLogger (Verbose LogInventoryDragHost output, CVar
 *     gated)
 */
UENUM()
enum class EInventoryDragEventKind : uint8
{
    Started,
    PreviewUpdated,
    PreviewCleared,
    DropResolved,
    DropRejected,
    Routed,
    VMInvoked,
    Completed,
    Cancelled,
};

/**
 * Snapshot of the preview state. Widgets pull this read-only view to
 * paint preview highlights without subscribing to session mutation.
 */
USTRUCT()
struct PROJECTINVENTORYUI_API FInventoryPreviewSnapshot
{
    GENERATED_BODY()

    /** Surface the cursor is currently resolved over. Invalid when no preview is active. */
    UPROPERTY()
    FGameplayTag TargetTag;

    /** Cell (column, row) in the target surface's grid. (-1,-1) when no preview is active. */
    UPROPERTY()
    FIntPoint TargetCell = FIntPoint(-1, -1);

    /**
     * True iff the preview resolved to a surface AND the dispatcher/
     * controller accepted the footprint. Widgets paint "valid" highlights
     * only when bValid is true.
     */
    UPROPERTY()
    bool bValid = false;

    /**
     * When bValid is false and a preview was attempted, RejectReason
     * names why. NAME_None indicates "no preview active" rather than a
     * rejection.
     */
    UPROPERTY()
    FName RejectReason;
};

/**
 * Plain-old-data session state owned by UInventoryUIDragHostSubsystem.
 * No raw pointers, no widget references.
 */
USTRUCT()
struct PROJECTINVENTORYUI_API FInventoryDragSession
{
    GENERATED_BODY()

    UPROPERTY()
    FGameplayTag SourceSurfaceTag;

    UPROPERTY()
    FIntPoint SourceCell = FIntPoint(-1, -1);

    UPROPERTY()
    int32 InstanceId = INDEX_NONE;

    UPROPERTY()
    int32 Quantity = 0;

    UPROPERTY()
    bool bRotated = false;

    UPROPERTY()
    bool bActive = false;

    UPROPERTY()
    FInventoryPreviewSnapshot Preview;
};

/**
 * Single event emitted on every subsystem state transition during a
 * drag session. Fields default to sentinels so tests can match
 * partially - only fields set by an entry in an expected sequence are
 * asserted.
 */
USTRUCT()
struct PROJECTINVENTORYUI_API FInventoryDragEvent
{
    GENERATED_BODY()

    UPROPERTY()
    EInventoryDragEventKind Kind = EInventoryDragEventKind::Started;

    UPROPERTY()
    FGameplayTag SourceTag;

    UPROPERTY()
    FGameplayTag TargetTag;

    UPROPERTY()
    FIntPoint SourceCell = FIntPoint(-1, -1);

    UPROPERTY()
    FIntPoint TargetCell = FIntPoint(-1, -1);

    UPROPERTY()
    int32 InstanceId = INDEX_NONE;

    UPROPERTY()
    int32 Quantity = 0;

    /** Populated for DropRejected events; NAME_None otherwise. */
    UPROPERTY()
    FName RejectReason;

    /** Populated for VMInvoked events; names the ViewModel method. */
    UPROPERTY()
    FName VMMethod;

    UPROPERTY()
    double TimestampSeconds = 0.0;
};

/**
 * Multicast delegate for drag decisions. Subscribers: the test-side
 * recorder and (optionally) the Verbose-level logger. Both are thin
 * consumers; the event struct is the SOT.
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnInventoryDragEvent, const FInventoryDragEvent& /*Event*/);

/**
 * Inputs for BeginCellDrag. The subsystem uses these to seed the
 * session state and Started event payload without widgets passing
 * raw DragOp references through the API.
 */
USTRUCT()
struct PROJECTINVENTORYUI_API FInventoryDragStartParams
{
    GENERATED_BODY()

    UPROPERTY()
    FGameplayTag SourceTag;

    UPROPERTY()
    FIntPoint SourceCell = FIntPoint(-1, -1);

    UPROPERTY()
    int32 InstanceId = INDEX_NONE;

    UPROPERTY()
    int32 Quantity = 0;

    UPROPERTY()
    bool bRotated = false;
};

/**
 * Context for preview/drop candidates: the widget hands over the
 * DragOp-derived payload fields required to validate + route through
 * the subsystem's controller/dispatcher/router. Keeps widgets off
 * the concrete dispatcher types on the API boundary.
 */
USTRUCT()
struct PROJECTINVENTORYUI_API FInventoryCellCandidate
{
    GENERATED_BODY()

    UPROPERTY()
    int32 InstanceId = INDEX_NONE;

    UPROPERTY()
    FGameplayTag SourceSurfaceTag;

    UPROPERTY()
    FIntPoint SourcePos = FIntPoint(-1, -1);

    UPROPERTY()
    int32 Quantity = 0;

    UPROPERTY()
    bool bRotated = false;

    UPROPERTY()
    FIntPoint ItemSize = FIntPoint(1, 1);
};
