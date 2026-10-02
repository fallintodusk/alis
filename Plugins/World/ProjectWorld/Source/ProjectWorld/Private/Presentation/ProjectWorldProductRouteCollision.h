// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

class AActor;
class UWorld;

namespace ProjectWorldProductRouteCollision
{
	int32 CountBlockingPrimitives(const AActor& Actor);
	bool SelectFirstBlockingGroundLocation(
		const TArray<FHitResult>& OrderedHits,
		float CapsuleHalfHeight,
		FVector& OutGroundLocation);
	bool FindFirstBlockingGroundLocation(
		UWorld& World,
		const AActor& IgnoredActor,
		const FVector& Location,
		float CapsuleHalfHeight,
		FVector& OutGroundLocation);
}
