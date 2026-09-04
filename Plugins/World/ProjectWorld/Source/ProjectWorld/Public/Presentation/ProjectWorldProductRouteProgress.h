// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

// Fail-closed acceptance state shared by the packaged product-route gate and
// its focused automation test. A loaded map alone is never product evidence.
struct FProjectWorldProductRouteProgress
{
	bool bMapIdentity = false;
	bool bGameModeIdentity = false;
	bool bPossessedPlayer = false;
	bool bGroundedPlayer = false;
	bool bNormalMovement = false;
	bool bTerrainCollision = false;
	bool bRoadCollision = false;
	bool bBuildingCollision = false;
	bool bGameplayInteraction = false;
	bool bCenterUnloadedAtEdge = false;
	bool bEdgeLoaded = false;
	bool bCenterReloaded = false;

	bool IsAccepted(bool bGameplayInteractionRequired = true) const
	{
		return FirstMissingGate(bGameplayInteractionRequired).IsEmpty();
	}

	FString FirstMissingGate(bool bGameplayInteractionRequired = true) const
	{
		if (!bMapIdentity) return TEXT("map_identity");
		if (!bGameModeIdentity) return TEXT("game_mode_identity");
		if (!bPossessedPlayer) return TEXT("possessed_player");
		if (!bGroundedPlayer) return TEXT("grounded_player");
		if (!bNormalMovement) return TEXT("normal_movement");
		if (!bTerrainCollision) return TEXT("terrain_collision");
		if (!bRoadCollision) return TEXT("road_collision");
		if (!bBuildingCollision) return TEXT("building_collision");
		if (bGameplayInteractionRequired && !bGameplayInteraction) return TEXT("gameplay_interaction");
		if (!bCenterUnloadedAtEdge) return TEXT("center_unloaded_at_edge");
		if (!bEdgeLoaded) return TEXT("edge_loaded");
		if (!bCenterReloaded) return TEXT("center_reloaded");
		return FString();
	}
};
