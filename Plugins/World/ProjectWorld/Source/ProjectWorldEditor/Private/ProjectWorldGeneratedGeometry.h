// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "ProjectWorldGeneratedActors.h"

struct FProjectWorldCanonicalBundle;
struct FProjectWorldCanonicalCell;
struct FProjectWorldRealizationResult;
class AActor;
class UWorld;

namespace ProjectWorldGeneratedGeometry
{
	bool IsCoreGeneratedActor(const AActor& Actor);

	bool RemoveOwnedActors(
		UWorld* World,
		FProjectWorldRealizationResult& OutResult);

	bool RemoveStaleOwnedActorsForApply(
		UWorld* World,
		const FProjectWorldCanonicalBundle& Bundle,
		const FString& RuntimeProfileId,
		FProjectWorldRealizationResult& OutResult,
		FString* OutError = nullptr);

	double MeasureCoordinateRoundTrip(
		UWorld* World,
		const FProjectWorldCanonicalBundle& Bundle,
		bool bPersistActor,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError);
}
