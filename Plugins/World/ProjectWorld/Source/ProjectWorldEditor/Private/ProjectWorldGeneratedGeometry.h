// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

struct FProjectWorldCanonicalBundle;
struct FProjectWorldRealizationResult;
class UWorld;

namespace ProjectWorldGeneratedGeometry
{
	bool RemoveOwnedActors(
		UWorld* World,
		bool bPreserveLandscape,
		FProjectWorldRealizationResult& OutResult);

	bool CreateOwnedActors(
		UWorld* World,
		const FProjectWorldCanonicalBundle& Bundle,
		bool bIncludeTerrain,
		int32 MaxRoadFeatures,
		int32 MaxBuildingFeatures,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError);

	double MeasureCoordinateRoundTrip(
		UWorld* World,
		const FProjectWorldCanonicalBundle& Bundle,
		bool bPersistActor,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError);
}
