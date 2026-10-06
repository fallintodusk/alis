// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

struct FProjectWorldCanonicalBundle;
struct FProjectWorldCanonicalCell;
struct FProjectWorldAuthoredOverlaySet;
struct FProjectWorldRealizationLayer;
struct FProjectWorldRealizationProfile;
struct FProjectWorldRealizationResult;
class AActor;
class UWorld;

struct FProjectWorldVegetationInstance
{
	FString StableId;
	int32 MeshIndex = 0;
	FTransform Transform = FTransform::Identity;
};

struct FProjectWorldVegetationPlacementStats
{
	int32 CandidateCount = 0;
	int32 RoadExcludedCount = 0;
	int32 WaterExcludedCount = 0;
	int32 AuthoredMaskExcludedCount = 0;
};

namespace ProjectWorldVegetationRealization
{
	bool HashCellInput(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldCanonicalCell& Cell,
		const FProjectWorldRealizationLayer& Layer,
		const FProjectWorldRealizationProfile& Profile,
		const FProjectWorldAuthoredOverlaySet& AuthoredOverlaySet,
		FString& OutHash,
		FString& OutError);

	bool BuildCellInstances(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldCanonicalCell& Cell,
		const FProjectWorldRealizationLayer& Layer,
		const FProjectWorldRealizationProfile& Profile,
		const FProjectWorldAuthoredOverlaySet& AuthoredOverlaySet,
		TArray<FProjectWorldVegetationInstance>& OutInstances,
		FProjectWorldVegetationPlacementStats* OutStats,
		FString& OutSemanticHash,
		FString& OutError);

	bool ExpectsCellOutput(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldCanonicalCell& Cell,
		const FProjectWorldRealizationLayer& Layer,
		const FProjectWorldRealizationProfile& Profile,
		const FProjectWorldAuthoredOverlaySet& AuthoredOverlaySet,
		bool& bOutExpected,
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
		FProjectWorldRealizationResult& OutResult,
		FString& OutError);
}
