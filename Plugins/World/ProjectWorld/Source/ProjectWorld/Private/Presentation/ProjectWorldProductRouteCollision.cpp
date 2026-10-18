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

	bool ProbeActorCollision(const AActor& Actor, const AActor* IgnoredActor)
	{
		FVector Origin;
		FVector Extent;
		Actor.GetActorBounds(false, Origin, Extent, true);
		if (Origin.ContainsNaN() || Extent.ContainsNaN() || Extent.IsNearlyZero())
		{
			return false;
		}
		TInlineComponentArray<UPrimitiveComponent*> Components;
		Actor.GetComponents(Components);
		for (UPrimitiveComponent* Primitive : Components)
		{
			if (Primitive == nullptr || !Primitive->IsRegistered() || !Primitive->IsCollisionEnabled() ||
				Primitive->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block)
			{
				continue;
			}
			const FBoxSphereBounds Bounds = Primitive->Bounds;
			const float Radius = FMath::Max(100.0, Bounds.SphereRadius + 100.0);
			const FCollisionShape ProbeSphere = FCollisionShape::MakeSphere(Radius);
			if (Primitive->OverlapComponent(Bounds.Origin, FQuat::Identity, ProbeSphere))
			{
				return true;
			}
			const FVector Start = Bounds.Origin + FVector(0.0f, 0.0f, Bounds.BoxExtent.Z + Radius + 100.0f);
			const FVector End = Bounds.Origin - FVector(0.0f, 0.0f, Bounds.BoxExtent.Z + Radius + 100.0f);
			FHitResult Hit;
			if (Primitive->SweepComponent(
				Hit, Start, End, FQuat::Identity, ProbeSphere, false) && Hit.bBlockingHit)
			{
				return true;
			}
		}
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ProjectWorldProductRouteCollision), true);
		Params.AddIgnoredActor(IgnoredActor);
		const FVector QueryOffsets[] =
		{
			FVector(0.0f, 0.0f, Extent.Z + 5000.0f),
			FVector(Extent.X + 5000.0f, 0.0f, 0.0f),
			FVector(-(Extent.X + 5000.0f), 0.0f, 0.0f),
			FVector(0.0f, Extent.Y + 5000.0f, 0.0f),
			FVector(0.0f, -(Extent.Y + 5000.0f), 0.0f)
		};
		for (const FVector& Offset : QueryOffsets)
		{
			FVector ClosestPoint;
			UPrimitiveComponent* Primitive = nullptr;
			const float Distance = Actor.ActorGetDistanceToCollision(
				Origin + Offset, ECC_Pawn, ClosestPoint, &Primitive);
			if (Distance >= 0.0f && Primitive != nullptr)
			{
				return true;
			}
		}
		constexpr int32 SamplesPerAxis = 25;
		for (int32 XIndex = 0; XIndex < SamplesPerAxis; ++XIndex)
		{
			for (int32 YIndex = 0; YIndex < SamplesPerAxis; ++YIndex)
			{
				const float XAlpha = SamplesPerAxis == 1 ? 0.0f : -1.0f + 2.0f * XIndex / (SamplesPerAxis - 1);
				const float YAlpha = SamplesPerAxis == 1 ? 0.0f : -1.0f + 2.0f * YIndex / (SamplesPerAxis - 1);
				const FVector Start(Origin.X + Extent.X * XAlpha, Origin.Y + Extent.Y * YAlpha, Origin.Z + Extent.Z + 5000.0f);
				const FVector End(Start.X, Start.Y, Origin.Z - Extent.Z - 5000.0f);
				FHitResult Hit;
				if (Actor.ActorLineTraceSingle(Hit, Start, End, ECC_Pawn, Params) && Hit.bBlockingHit)
				{
					return true;
				}
			}
		}
		return false;
	}
}
