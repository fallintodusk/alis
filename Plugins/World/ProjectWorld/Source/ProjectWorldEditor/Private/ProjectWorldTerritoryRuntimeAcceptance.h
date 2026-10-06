// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

struct FProjectWorldRealizationResult;
struct FProjectWorldRuntimeProfile;
class UWorld;

namespace ProjectWorldTerritoryRuntimeAcceptance
{
	bool IsInstancingPolicySatisfied(
		bool bVegetationLayerSelected,
		bool bFoundVegetation,
		bool bInstancingAccepted);
	bool HasValidVegetationInstancing(int32 MeshComponentCount, bool bAllMeshesHaveInstances);

	bool CaptureAndCheck(
		UWorld* World,
		const FProjectWorldRuntimeProfile& Profile,
		bool bVegetationLayerSelected,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError);
}
