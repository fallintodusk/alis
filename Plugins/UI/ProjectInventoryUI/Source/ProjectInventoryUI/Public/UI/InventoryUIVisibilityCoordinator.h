// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

class UProjectUILayerHostSubsystem;

/**
 * Single source of truth for "the inventory UI is open / closed".
 *
 * Inventory UI is composed of two layer-host widgets that must come up
 * and down together:
 *   - ProjectInventoryUI.InventoryPanel       (main panel)
 *   - ProjectInventoryUI.NearbyContainerPanel (nearby loot sibling)
 *
 * The nearby panel self-collapses when no loot session is active, so
 * showing it eagerly alongside the main panel is safe. Any caller that
 * toggles inventory UI visibility MUST go through this helper - calling
 * `LayerHost->ShowDefinition("...InventoryPanel")` directly leaves the
 * nearby sibling unspawned and breaks loot sessions silently.
 */
namespace InventoryUIVisibilityCoordinator
{
    /**
     * Show or hide the inventory UI surfaces as a single unit. Both
     * widgets are toggled together. No-op if LayerHost is null.
     */
    PROJECTINVENTORYUI_API void SetInventoryUIVisible(
        UProjectUILayerHostSubsystem* LayerHost,
        bool bVisible);

    /**
     * Returns the layer-host definition id for the main inventory panel.
     * Exposed so tests and diagnostics can assert by id without hard-coding
     * the string in multiple places.
     */
    PROJECTINVENTORYUI_API const TCHAR* GetMainPanelDefinitionId();

    /**
     * Returns the layer-host definition id for the nearby container panel.
     */
    PROJECTINVENTORYUI_API const TCHAR* GetNearbyPanelDefinitionId();
}
