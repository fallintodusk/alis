// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Central table of drag-host surface priorities for inventory-family
 * widgets. Registered with FProjectUIGridDragDropController::RegisterSurface
 * via UInventoryUIDragHostSubsystem.
 *
 * Higher value wins the hit test when two surfaces overlap on screen.
 * Ties preserve registration order (stable sort).
 *
 * Rules:
 *   - Widgets MUST pick a constant from this header. Do not pass ad-hoc
 *     integer literals to RegisterSurface.
 *   - When adding a new surface, add a constant here first and document
 *     how it relates to the existing values.
 *   - Keep values at coarse steps (10s) so new surfaces can slot in
 *     between without renumbering.
 */
namespace InventoryUISurfacePriority
{
    /**
     * Default for player-side grids living inside W_InventoryPanel:
     * Backpack, hands, pockets, and tabbed secondary player storage.
     * These never overlap each other in normal layout, so the exact
     * value does not matter beyond "below any overlay surface".
     */
    constexpr int32 PlayerStorage = 0;

    /**
     * Nearby world-container grid (W_NearbyContainerPanel).
     * Strictly greater than PlayerStorage so if a future layout briefly
     * overlaps the nearby panel with a player-side surface, nearby wins
     * the hit test (its content is the most relevant to the active
     * session the moment it is visible).
     */
    constexpr int32 NearbyWorldStorage = 10;

    /**
     * Reserved for future modal / context overlays that must absorb
     * drags regardless of what sits underneath (e.g. a transient split
     * prompt or a confirm dialog that overlays the grids). Not used by
     * any current widget; documented so the value is not silently
     * claimed for something else.
     */
    constexpr int32 ModalOverlay = 100;
}
