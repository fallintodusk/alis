// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

struct FProjectWorldCanonicalBundle;
struct FProjectWorldCanonicalCell;
struct FProjectWorldRealizationLayer;
struct FProjectWorldRealizationProfile;
struct FProjectWorldRealizationResult;
class AActor;
class UMaterialInterface;
class UWorld;

namespace ProjectWorldRoadRealization
{
	bool HashCellInput(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldCanonicalCell& Cell,
		const FProjectWorldRealizationLayer& Layer,
		FString& OutHash,
		FString& OutError);

	bool ExpectsCellOutput(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldCanonicalCell& Cell,
		const FProjectWorldRealizationLayer& Layer,
		bool& bOutExpected,
		FString& OutError);

	bool Apply(
		UWorld* World,
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldRealizationProfile& Profile,
		UMaterialInterface* RoadMaterial,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError);

	bool ReadActorIdentity(
		const AActor* Actor,
		FString& OutCellId,
		FString& OutSemanticHash);
}
