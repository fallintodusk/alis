// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

struct FProjectWorldAuthoredOverlaySet;
struct FProjectWorldBuildingMeshBuildResult;
struct FProjectWorldCanonicalBundle;
struct FProjectWorldCanonicalCell;
struct FProjectWorldRealizationLayer;
struct FProjectWorldRealizationProfile;
struct FProjectWorldRealizationResult;
class AActor;
class UMaterialInterface;
class UWorld;

namespace ProjectWorldBuildingRealization
{
	bool HashCellInput(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldCanonicalCell& Cell,
		const FProjectWorldRealizationLayer& Layer,
		const FProjectWorldAuthoredOverlaySet& AuthoredOverlaySet,
		FString& OutHash,
		FString& OutError);

	bool BuildCellOutput(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldCanonicalCell& Cell,
		const FProjectWorldRealizationLayer& Layer,
		const FProjectWorldAuthoredOverlaySet& AuthoredOverlaySet,
		FProjectWorldBuildingMeshBuildResult& OutBuild,
		FString& OutError);

	bool ReadActorIdentity(
		const AActor* Actor,
		FString& OutCellId,
		FString& OutSemanticHash);

	bool Apply(
		UWorld* World,
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldRealizationProfile& Profile,
		const FProjectWorldAuthoredOverlaySet& AuthoredOverlaySet,
		UMaterialInterface* BuildingMaterial,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError);
}
