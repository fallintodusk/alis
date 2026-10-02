// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Presentation/ProjectWorldProductRouteCollision.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace ProjectWorldProductRouteCollision
{
	int32 CountBlockingPrimitives(const AActor& Actor)
	{
		int32 Count = 0;
		TInlineComponentArray<UPrimitiveComponent*> Components;
		Actor.GetComponents(Components);
		for (const UPrimitiveComponent* Primitive : Components)
		{
			if (Primitive != nullptr && Primitive->IsRegistered() && Primitive->IsCollisionEnabled() &&
				Primitive->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block)
			{
				++Count;
			}
		}
		return Count;
	}

	bool SelectFirstBlockingGroundLocation(
		const TArray<FHitResult>& OrderedHits,
		float CapsuleHalfHeight,
		FVector& OutGroundLocation)
	{
		const FHitResult* FirstBlocker = OrderedHits.FindByPredicate(
			[](const FHitResult& Hit)
			{
				return Hit.bBlockingHit;
			});
		if (FirstBlocker == nullptr)
		{
			return false;
		}
		OutGroundLocation = FirstBlocker->ImpactPoint +
			FVector(0.0f, 0.0f, CapsuleHalfHeight + 10.0f);
		return true;
	}

	bool FindFirstBlockingGroundLocation(
		UWorld& World,
		const AActor& IgnoredActor,
		const FVector& Location,
		float CapsuleHalfHeight,
		FVector& OutGroundLocation)
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ProjectWorldProductRouteGround), false);
		Params.AddIgnoredActor(&IgnoredActor);
		const FVector Start(Location.X, Location.Y, Location.Z + 100000.0f);
		const FVector End(Location.X, Location.Y, Location.Z - 100000.0f);
		TArray<FHitResult> OrderedHits;
		World.LineTraceMultiByChannel(OrderedHits, Start, End, ECC_Pawn, Params);
		return SelectFirstBlockingGroundLocation(
			OrderedHits, CapsuleHalfHeight, OutGroundLocation);
	}
}
