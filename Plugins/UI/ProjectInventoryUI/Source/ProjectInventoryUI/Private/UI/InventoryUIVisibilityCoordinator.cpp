// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
// Coordinator: spawn pair for inventory UI surfaces. See header for rules.

#include "UI/InventoryUIVisibilityCoordinator.h"

#include "Subsystems/ProjectUILayerHostSubsystem.h"

namespace InventoryUIVisibilityCoordinator
{
    namespace
    {
        constexpr const TCHAR* MainPanelDefinitionId = TEXT("ProjectInventoryUI.InventoryPanel");
        constexpr const TCHAR* NearbyPanelDefinitionId = TEXT("ProjectInventoryUI.NearbyContainerPanel");
    }

    const TCHAR* GetMainPanelDefinitionId()  { return MainPanelDefinitionId; }
    const TCHAR* GetNearbyPanelDefinitionId(){ return NearbyPanelDefinitionId; }

    void SetInventoryUIVisible(UProjectUILayerHostSubsystem* LayerHost, bool bVisible)
    {
        if (!LayerHost)
        {
            return;
        }

        if (bVisible)
        {
            LayerHost->ShowDefinition(MainPanelDefinitionId);
            LayerHost->ShowDefinition(NearbyPanelDefinitionId);
        }
        else
        {
            LayerHost->HideDefinition(MainPanelDefinitionId);
            LayerHost->HideDefinition(NearbyPanelDefinitionId);
        }
    }
}
