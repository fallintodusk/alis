// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "GameplayTagContainer.h"
#include "UObject/WeakObjectPtr.h"

class UInventoryUIDragHostSubsystem;
class UW_InventoryPanel;
struct FInventoryDragEvent;

/**
 * Per-panel owner of drag-host surface registration and the drag-event
 * bus subscription.  Split out of UW_InventoryPanel so that shared-
 * subsystem bookkeeping (primary/secondary/pocket/hand tags, OnDragEvent
 * handle) lives alongside the state it mutates.
 *
 * Ownership: UW_InventoryPanel holds one FInventoryPanelSurfaceRegistry
 * as a member.  Initialize once at NativeConstruct; all Register* /
 * Unregister* calls go through this struct thereafter.
 *
 * Dependency on parent: weak pointer only.  The registry never calls
 * UClass-private methods on the panel; instead, preview-driven repaint
 * is signalled via the OnPreviewChanged delegate which the panel binds
 * to UW_InventoryPanel::UpdateAllVisuals at NativeConstruct time.
 */
struct FInventoryPanelSurfaceRegistry
{
    /** Fires on drag-bus events that invalidate preview highlights. */
    DECLARE_DELEGATE(FOnSurfacePreviewChanged);

    FInventoryPanelSurfaceRegistry() = default;

    /**
     * Safety-net destructor.  Defence-in-depth for the BindEventBus
     * AddRaw(this, ...) contract: if a panel is destroyed without
     * NativeDestruct running (early shutdown, test harness, exception
     * path), this ensures we never leave a dangling subscription in the
     * shared drag-host subsystem.  Safe to call when already unbound -
     * UnbindEventBus / UnregisterAll both no-op cleanly.
     */
    ~FInventoryPanelSurfaceRegistry();

    /** Bind owner pointer.  Must be called before BindEventBus / Register*. */
    void Initialize(UW_InventoryPanel* InOwner);

    /**
     * Subscribe to UInventoryUIDragHostSubsystem::OnDragEvent.  Safe to
     * call repeatedly; an existing subscription is removed first.
     */
    void BindEventBus();

    /** Reverse of BindEventBus.  Safe to call when already unbound. */
    void UnbindEventBus();

    /** Publish the current hand grids to the shared drag host. */
    void RegisterHands();

    /**
     * Publish the current pocket grid set to the shared drag host.
     * Tags that are no longer present are unregistered first.
     */
    void RegisterPockets();

    /**
     * Publish the current primary/secondary player-storage tabs to
     * the shared drag host.  Stale tags from previous tabs are
     * unregistered first.
     */
    void RegisterPlayerGrids();

    /**
     * Unregister every surface this panel currently owns in the shared
     * drag host, resetting local bookkeeping so repeat calls no-op.
     */
    void UnregisterAll();

    /** Callback for UW_InventoryPanel::UpdateAllVisuals. */
    FOnSurfacePreviewChanged OnPreviewChanged;

    /**
     * Cached primary/secondary surface tags - consumed by
     * UW_InventoryPanel::UpdateAllVisuals to index the drag host's
     * tag-keyed preview map.  Read-only from the panel; mutated by
     * Register* / Unregister* here.
     */
    const FGameplayTag& GetCachedPrimarySurfaceTag() const { return CachedPrimarySurfaceTag; }
    const FGameplayTag& GetCachedSecondarySurfaceTag() const { return CachedSecondarySurfaceTag; }

private:
    UInventoryUIDragHostSubsystem* ResolveDragHostSubsystem() const;
    void HandleDragEvent(const FInventoryDragEvent& Event);

    TWeakObjectPtr<UW_InventoryPanel> Owner;
    FDelegateHandle DragEventHandle;
    FGameplayTag CachedPrimarySurfaceTag;
    FGameplayTag CachedSecondarySurfaceTag;
    TArray<FGameplayTag> RegisteredPocketSurfaceTags;
    bool bHandSurfacesRegistered = false;
};
