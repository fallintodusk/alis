// Copyright ALIS. All Rights Reserved.

#include "InteractionTargetResolver.h"
#include "InteractionCapabilitySelector.h"
#include "InteractionComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"

DEFINE_LOG_CATEGORY(LogInteraction);

namespace
{
	static TAutoConsoleVariable<int32> CVarInteractionDebug(
		TEXT("alis.Interaction.Debug"),
		0,
		TEXT("Enable interaction targeting debug logs (overlap, filter, aim, LOS, winner counts)."),
		ECVF_Default);

	struct FInteractionTargetActorInfo
	{
		bool bInitialized = false;
		bool bIsInteractable = false;
		int32 Priority = 0;
		TArray<UActorComponent*> InteractableComponents;
	};

	struct FInteractionTargetCandidateState
	{
		AActor* Actor = nullptr;
		UPrimitiveComponent* Component = nullptr;
		FVector TargetPoint = FVector::ZeroVector;
		float Distance = TNumericLimits<float>::Max();
		float AimDot = -1.0f;
		int32 Priority = 0;
		bool bMatchesTargetMesh = false;
		EInteractionViewRayHitKind ViewRayHitKind = EInteractionViewRayHitKind::None;
		float ViewRayHitDistance = TNumericLimits<float>::Max();
	};

	struct FResolvedTargetPoint
	{
		FVector Point = FVector::ZeroVector;
		EInteractionViewRayHitKind HitKind = EInteractionViewRayHitKind::None;
		float ViewRayHitDistance = TNumericLimits<float>::Max();
	};

	// Bucket rank for the comparator and per-actor primitive selection. Higher rank
	// always beats lower rank regardless of distance: a closer bounds-only hit must
	// not steal focus from a real collision pierce. This is the backpack regression
	// (loose AABB clipped the ray above a smaller collision-pierced item).
	int32 HitKindRank(EInteractionViewRayHitKind Kind)
	{
		switch (Kind)
		{
			case EInteractionViewRayHitKind::Collision: return 2;
			case EInteractionViewRayHitKind::Bounds:    return 1;
			default:                                    return 0;
		}
	}

	// Resolve the per-component representative point for "what the player is visually
	// pointing at" plus a 3-bucket "is the view ray under this candidate" signal.
	// Order:
	//   1. Component line trace (LineTraceComponent) - exact ImpactPoint when collision
	//      matches the visible mesh. HitKind = Collision (highest trust).
	//   2. Bounds AABB intersection with the view ray - catches meshes whose visible
	//      geometry lacks collision (e.g. window glass on a frame-only collision body).
	//      The bounds box is what the player sees on screen, so a bounds-on-ray entry
	//      point gives a centeredness signal even when the trace misses collision.
	//      HitKind = Bounds (looser than Collision; ranks below it so a closer bounds
	//      hit cannot steal focus from a further collision pierce).
	//   3. Closest point on collision to view origin - fallback when the ray misses
	//      both collision and bounds. HitKind = None; the candidate competes in the
	//      cone-fallback bucket on AimDot.
	FResolvedTargetPoint ResolveTargetPoint(
		const AActor* Actor,
		UPrimitiveComponent* Component,
		const FVector& ViewOrigin,
		const FVector& ViewForward,
		float InteractionRadius)
	{
		FResolvedTargetPoint Result;

		if (Component)
		{
			const FVector SafeViewForward = ViewForward.GetSafeNormal();
			const FVector RayEnd =
				SafeViewForward.IsNearlyZero()
					? ViewOrigin
					: ViewOrigin + (SafeViewForward * FMath::Max(InteractionRadius, 1.0f));

			if (!SafeViewForward.IsNearlyZero())
			{
				FHitResult ComponentHit;
				FCollisionQueryParams ComponentTraceParams(SCENE_QUERY_STAT(InteractionTargetPointTrace), false);
				if (Component->LineTraceComponent(ComponentHit, ViewOrigin, RayEnd, ComponentTraceParams))
				{
					Result.Point = ComponentHit.ImpactPoint;
					Result.HitKind = EInteractionViewRayHitKind::Collision;
					Result.ViewRayHitDistance = ComponentHit.Distance;
					return Result;
				}

				const FBox ComponentBounds = Component->Bounds.GetBox();
				if (ComponentBounds.IsValid)
				{
					FVector BoundsHit;
					FVector BoundsNormal;
					float BoundsTime = 0.0f;
					if (FMath::LineExtentBoxIntersection(
							ComponentBounds, ViewOrigin, RayEnd, FVector::ZeroVector,
							BoundsHit, BoundsNormal, BoundsTime))
					{
						Result.Point = BoundsHit;
						Result.HitKind = EInteractionViewRayHitKind::Bounds;
						Result.ViewRayHitDistance = FVector::Dist(ViewOrigin, BoundsHit);
						return Result;
					}
				}
			}

			FVector ClosestPoint = Component->Bounds.Origin;
			const float Distance = Component->GetClosestPointOnCollision(ViewOrigin, ClosestPoint);
			Result.Point = (Distance >= 0.0f) ? ClosestPoint : Component->Bounds.Origin;
			return Result;
		}

		Result.Point = Actor ? Actor->GetActorLocation() : ViewOrigin;
		return Result;
	}

	bool MatchesCapabilityTargetMesh(
		const UPrimitiveComponent* OverlapComponent,
		const TArray<UActorComponent*>& InteractableComponents)
	{
		if (!OverlapComponent)
		{
			return false;
		}

		for (UActorComponent* InteractableComponent : InteractableComponents)
		{
			if (FInteractionCapabilitySelector::GetTargetMesh(InteractableComponent) == OverlapComponent)
			{
				return true;
			}
		}

		return false;
	}

	// Per-actor component selection - mirrors the global comparator buckets so the
	// actor's representative primitive carries the same signal the comparator will
	// use later.
	//   1. Target-mesh match (capability-declared mesh) wins first.
	//   2. Higher hit-kind rank (Collision > Bounds > None) wins.
	//   3. Among same-bucket Collision/Bounds: smaller ViewRayHitDistance wins.
	//   4. Among same-bucket None: higher AimDot wins.
	//   5. Smaller Distance.
	//   6. Component UniqueID tiebreak.
	bool ShouldReplaceComponent(
		const FInteractionTargetCandidateState& CurrentState,
		bool bNewMatchesTargetMesh,
		EInteractionViewRayHitKind NewHitKind,
		float NewViewRayHitDistance,
		float NewAimDot,
		float NewDistance,
		UPrimitiveComponent* NewComponent)
	{
		if (!CurrentState.Component)
		{
			return true;
		}

		if (CurrentState.bMatchesTargetMesh != bNewMatchesTargetMesh)
		{
			return bNewMatchesTargetMesh;
		}

		const int32 CurrentRank = HitKindRank(CurrentState.ViewRayHitKind);
		const int32 NewRank = HitKindRank(NewHitKind);
		if (CurrentRank != NewRank)
		{
			return NewRank > CurrentRank;
		}

		if (NewHitKind != EInteractionViewRayHitKind::None)
		{
			if (!FMath::IsNearlyEqual(NewViewRayHitDistance, CurrentState.ViewRayHitDistance))
			{
				return NewViewRayHitDistance < CurrentState.ViewRayHitDistance;
			}
		}
		else
		{
			if (!FMath::IsNearlyEqual(NewAimDot, CurrentState.AimDot))
			{
				return NewAimDot > CurrentState.AimDot;
			}
		}

		if (NewDistance + KINDA_SMALL_NUMBER < CurrentState.Distance)
		{
			return true;
		}

		if (FMath::IsNearlyEqual(NewDistance, CurrentState.Distance))
		{
			const int32 CurrentId = CurrentState.Component ? CurrentState.Component->GetUniqueID() : MAX_int32;
			const int32 NewId = NewComponent ? NewComponent->GetUniqueID() : MAX_int32;
			return NewId < CurrentId;
		}

		return false;
	}

	// Comparator chain - player rule: "real ray collision wins; bounds-only ray hit
	// is a visual fallback; cone fallback is last. Within a bucket the front-most
	// (or most-centered) candidate wins. Priority is only a final tiebreak."
	//   1. Higher hit-kind rank wins (Collision > Bounds > None). A closer bounds
	//      hit must NOT outrank a further collision hit (the backpack regression).
	//   2. Among same-bucket Collision/Bounds: smaller ViewRayHitDistance wins
	//      (front-most along the view ray).
	//   3. Among same-bucket None: higher AimDot wins (most centered in cone).
	//   4. Smaller Distance tiebreak.
	//   5. Higher Priority tiebreak (demoted to final tiebreak; never overrides aim).
	//   6. Final: deterministic Actor UniqueID.
	bool IsBetterCandidate(
		const FInteractionTargetCandidate& Candidate,
		const FInteractionTargetCandidate& CurrentWinner)
	{
		const int32 CandidateRank = HitKindRank(Candidate.ViewRayHitKind);
		const int32 WinnerRank = HitKindRank(CurrentWinner.ViewRayHitKind);
		if (CandidateRank != WinnerRank)
		{
			return CandidateRank > WinnerRank;
		}

		if (Candidate.ViewRayHitKind != EInteractionViewRayHitKind::None)
		{
			if (!FMath::IsNearlyEqual(Candidate.ViewRayHitDistance, CurrentWinner.ViewRayHitDistance))
			{
				return Candidate.ViewRayHitDistance < CurrentWinner.ViewRayHitDistance;
			}
		}
		else
		{
			if (!FMath::IsNearlyEqual(Candidate.AimDot, CurrentWinner.AimDot))
			{
				return Candidate.AimDot > CurrentWinner.AimDot;
			}
		}

		if (!FMath::IsNearlyEqual(Candidate.Distance, CurrentWinner.Distance))
		{
			return Candidate.Distance < CurrentWinner.Distance;
		}

		if (Candidate.Priority != CurrentWinner.Priority)
		{
			return Candidate.Priority > CurrentWinner.Priority;
		}

		const int32 CandidateId = Candidate.Actor ? Candidate.Actor->GetUniqueID() : MAX_int32;
		const int32 WinnerId = CurrentWinner.Actor ? CurrentWinner.Actor->GetUniqueID() : MAX_int32;
		return CandidateId < WinnerId;
	}
}

FInteractionTargetResolver::FInteractionTargetResolver(
	UWorld* InWorld,
	const FVector& InViewOrigin,
	const FVector& InViewForward,
	ECollisionChannel InLineOfSightChannel,
	AActor* InIgnoreActor,
	const FInteractionTargetingWeights& InWeights)
	: World(InWorld)
	, ViewOrigin(InViewOrigin)
	, ViewForward(InViewForward.GetSafeNormal())
	, LineOfSightChannel(InLineOfSightChannel)
	, IgnoreActor(InIgnoreActor)
	, Weights(InWeights)
{
}

bool FInteractionTargetResolver::Resolve(
	FInteractionTargetCandidate& OutWinner,
	FInteractionTargetDebugSnapshot* OutDebug,
	const AActor* TrackedActor,
	FInteractionTargetCandidate* OutTrackedCandidate) const
{
	OutWinner = FInteractionTargetCandidate();
	if (OutTrackedCandidate)
	{
		*OutTrackedCandidate = FInteractionTargetCandidate();
	}

	if (!World || ViewForward.IsNearlyZero())
	{
		return false;
	}

	if (OutDebug)
	{
		OutDebug->ViewOrigin = ViewOrigin;
		OutDebug->ViewForward = ViewForward;
		OutDebug->InteractionRadius = Weights.InteractionRadius;
		OutDebug->MissEndPoint = ViewOrigin + (ViewForward * Weights.InteractionRadius);
		OutDebug->Rays.Reset();
	}

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams OverlapQueryParams(SCENE_QUERY_STAT(InteractionTargetingOverlap), false);
	if (IgnoreActor.IsValid())
	{
		OverlapQueryParams.AddIgnoredActor(IgnoreActor.Get());
	}

	TArray<FOverlapResult> OverlapResults;
	World->OverlapMultiByObjectType(
		OverlapResults,
		ViewOrigin,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(Weights.InteractionRadius),
		OverlapQueryParams);

	int32 NumAimRejected = 0;
	int32 NumLosRejected = 0;
	TMap<AActor*, FInteractionTargetActorInfo> ActorInfoCache;
	TMap<AActor*, FInteractionTargetCandidateState> CandidateStates;

	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AActor* CandidateActor = OverlapResult.GetActor();
		UPrimitiveComponent* CandidateComponent = OverlapResult.GetComponent();
		if (!CandidateActor || CandidateActor == IgnoreActor.Get())
		{
			continue;
		}

		FInteractionTargetActorInfo& ActorInfo = ActorInfoCache.FindOrAdd(CandidateActor);
		if (!ActorInfo.bInitialized)
		{
			ActorInfo.bInitialized = true;
			FInteractionCapabilitySelector::GatherComponents(CandidateActor, ActorInfo.InteractableComponents);
			ActorInfo.bIsInteractable =
				CandidateActor->Implements<UInteractableTargetInterface>() ||
				ActorInfo.InteractableComponents.Num() > 0;
			ActorInfo.Priority =
				ActorInfo.InteractableComponents.Num() > 0
					? IInteractableComponentTargetInterface::Execute_GetInteractPriority(ActorInfo.InteractableComponents[0])
					: 0;
		}

		if (!ActorInfo.bIsInteractable)
		{
			continue;
		}

		const FResolvedTargetPoint Resolved = ResolveTargetPoint(
			CandidateActor,
			CandidateComponent,
			ViewOrigin,
			ViewForward,
			Weights.InteractionRadius);
		const float Distance = FVector::Dist(ViewOrigin, Resolved.Point);
		const FVector ToTarget = Resolved.Point - ViewOrigin;
		const float AimDot =
			(Distance <= KINDA_SMALL_NUMBER)
				? 1.0f
				: FVector::DotProduct(ToTarget / Distance, ViewForward);
		const bool bMatchesTargetMesh =
			MatchesCapabilityTargetMesh(CandidateComponent, ActorInfo.InteractableComponents);

		FInteractionTargetCandidateState& CandidateState = CandidateStates.FindOrAdd(CandidateActor);
		if (!CandidateState.Actor)
		{
			CandidateState.Actor = CandidateActor;
			CandidateState.Priority = ActorInfo.Priority;
		}

		if (ShouldReplaceComponent(
				CandidateState,
				bMatchesTargetMesh,
				Resolved.HitKind,
				Resolved.ViewRayHitDistance,
				AimDot,
				Distance,
				CandidateComponent))
		{
			CandidateState.Component = CandidateComponent;
			CandidateState.TargetPoint = Resolved.Point;
			CandidateState.Distance = Distance;
			CandidateState.AimDot = AimDot;
			CandidateState.bMatchesTargetMesh = bMatchesTargetMesh;
			CandidateState.ViewRayHitKind = Resolved.HitKind;
			CandidateState.ViewRayHitDistance = Resolved.ViewRayHitDistance;
		}
	}

	const int32 NumCandidates = CandidateStates.Num();
	bool bFoundWinner = false;

	for (const TPair<AActor*, FInteractionTargetCandidateState>& Pair : CandidateStates)
	{
		const FInteractionTargetCandidateState& CandidateState = Pair.Value;
		if (!CandidateState.Actor)
		{
			continue;
		}

		FInteractionTargetCandidate Candidate;
		Candidate.Actor = CandidateState.Actor;
		Candidate.Component = CandidateState.Component;
		Candidate.TargetPoint = CandidateState.TargetPoint;
		Candidate.Priority = CandidateState.Priority;
		Candidate.Distance = CandidateState.Distance;
		Candidate.AimDot = CandidateState.AimDot;
		Candidate.ViewRayHitKind = CandidateState.ViewRayHitKind;
		Candidate.ViewRayHitDistance = CandidateState.ViewRayHitDistance;

		if (Candidate.AimDot < Weights.MinAimDot)
		{
			++NumAimRejected;
			if (OutDebug)
			{
				OutDebug->AddRay(Candidate.TargetPoint, EInteractionTargetDebugRayType::AimRejected);
			}
			continue;
		}

		if (Candidate.Distance > Weights.ShortCircuitRadius)
		{
			FCollisionQueryParams LosQueryParams(SCENE_QUERY_STAT(InteractionTargetingLos), false);
			if (IgnoreActor.IsValid())
			{
				LosQueryParams.AddIgnoredActor(IgnoreActor.Get());
			}

			FHitResult HitResult;
			const bool bHit = World->LineTraceSingleByChannel(
				HitResult,
				ViewOrigin,
				Candidate.TargetPoint,
				LineOfSightChannel,
				LosQueryParams);

			if (bHit && HitResult.GetActor() != Candidate.Actor)
			{
				++NumLosRejected;
				if (OutDebug)
				{
					OutDebug->AddRay(Candidate.TargetPoint, EInteractionTargetDebugRayType::LosRejected);
				}
				continue;
			}
		}

		if (OutTrackedCandidate && TrackedActor && Candidate.Actor == TrackedActor)
		{
			*OutTrackedCandidate = Candidate;
		}

		if (!bFoundWinner || IsBetterCandidate(Candidate, OutWinner))
		{
			OutWinner = Candidate;
			bFoundWinner = true;
		}
	}

	if (CVarInteractionDebug.GetValueOnGameThread() != 0)
	{
		const TCHAR* HitKindLabel = TEXT("None");
		switch (OutWinner.ViewRayHitKind)
		{
			case EInteractionViewRayHitKind::Collision: HitKindLabel = TEXT("Collision"); break;
			case EInteractionViewRayHitKind::Bounds:    HitKindLabel = TEXT("Bounds");    break;
			default: break;
		}
		UE_LOG(
			LogInteraction,
			Verbose,
			TEXT("[InteractionTargeting] overlaps=%d candidates=%d aimRejected=%d losRejected=%d winner=%s hitKind=%s rayHitDist=%.1f aim=%.3f distance=%.1f priority=%d"),
			OverlapResults.Num(),
			NumCandidates,
			NumAimRejected,
			NumLosRejected,
			*GetNameSafe(OutWinner.Actor),
			HitKindLabel,
			OutWinner.ViewRayHitDistance,
			OutWinner.AimDot,
			OutWinner.Distance,
			OutWinner.Priority);
	}

	return bFoundWinner;
}

FInteractionTargetingWeights FInteractionTargetResolver::BuildWeights(const UInteractionComponent& Component)
{
	FInteractionTargetingWeights Weights;
	Weights.InteractionRadius = FMath::Max(Component.InteractionRadius, 1.0f);
	Weights.ShortCircuitRadius = FMath::Clamp(Component.ShortCircuitRadius, 0.0f, Weights.InteractionRadius);
	Weights.MinAimDot = FMath::Clamp(Component.MinAimDot, -1.0f, 0.999f);
	return Weights;
}

bool FInteractionTargetResolver::ShouldKeepCurrentCandidate(
	const FInteractionTargetCandidate& CurrentFocus,
	const FInteractionTargetCandidate& NewWinner,
	float FocusSwitchHysteresis,
	float MinAimDot)
{
	if (!CurrentFocus.Actor || CurrentFocus.Actor == NewWinner.Actor)
	{
		return false;
	}

	// Cross-bucket switches (different ViewRayHitKind, e.g. Collision -> Bounds or
	// Bounds -> None) are deliberate aim transitions and are never smoothed.
	if (CurrentFocus.ViewRayHitKind != NewWinner.ViewRayHitKind)
	{
		return false;
	}

	const float ClampedHysteresis = FMath::Clamp(FocusSwitchHysteresis, 0.0f, 0.95f);

	if (CurrentFocus.ViewRayHitKind != EInteractionViewRayHitKind::None)
	{
		// Same bucket (both Collision or both Bounds): keep incumbent if its hit
		// distance is within hysteresis of the challenger's. Distance is unbounded
		// so a multiplicative threshold scales correctly.
		const float Threshold = NewWinner.ViewRayHitDistance * (1.0f + ClampedHysteresis);
		return CurrentFocus.ViewRayHitDistance <= Threshold;
	}

	// Same bucket (both fallback / None): smooth on angular error, NOT on AimDot
	// ratio. AimDot lives in [MinAimDot, 1.0] (the cone), which is too narrow for
	// a multiplicative threshold - e.g. with MinAimDot=0.85 and hysteresis=0.10,
	// a NewWinner.AimDot=0.999 ratio threshold = 0.8991 would keep ANY candidate
	// inside the cone, sticking the incumbent forever. Convert to angles and let
	// the leeway scale with the cone half-angle so the player rule "closest to
	// center wins" is honored even within the smoothing window.
	const float SafeMinAimDot = FMath::Clamp(MinAimDot, -1.0f, 0.999f);
	const float ConeHalfAngleRad = FMath::Acos(SafeMinAimDot);

	const float CurrentAngleRad = FMath::Acos(FMath::Clamp(CurrentFocus.AimDot, -1.0f, 1.0f));
	const float NewAngleRad = FMath::Acos(FMath::Clamp(NewWinner.AimDot, -1.0f, 1.0f));

	const float LeewayRad = ConeHalfAngleRad * ClampedHysteresis;
	return CurrentAngleRad <= NewAngleRad + LeewayRad;
}

float FInteractionTargetResolver::GetDebugLifetime(const UInteractionComponent& Component)
{
	return FMath::Max(0.05f, Component.TraceIntervalSeconds);
}

bool FInteractionTargetResolver::ResolvePlayerCameraView(
	APawn* Pawn,
	FVector& OutViewOrigin,
	FVector& OutViewForward)
{
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC || !PC->PlayerCameraManager)
	{
		return false;
	}

	OutViewOrigin = PC->PlayerCameraManager->GetCameraLocation();
	OutViewForward = PC->PlayerCameraManager->GetCameraRotation().Vector().GetSafeNormal();
	return !OutViewForward.IsNearlyZero();
}

bool FInteractionTargetResolver::ResolveActorEyesView(
	APawn* Pawn,
	FVector& OutViewOrigin,
	FVector& OutViewForward)
{
	if (!Pawn)
	{
		return false;
	}

	FRotator ViewRotation = FRotator::ZeroRotator;
	if (AController* Controller = Pawn->GetController())
	{
		Controller->GetActorEyesViewPoint(OutViewOrigin, ViewRotation);
	}
	else
	{
		Pawn->GetActorEyesViewPoint(OutViewOrigin, ViewRotation);
	}

	OutViewForward = ViewRotation.Vector().GetSafeNormal();
	return !OutViewForward.IsNearlyZero();
}

void FInteractionTargetResolver::DrawDebug(
	UWorld* World,
	const FInteractionTargetDebugSnapshot& Debug,
	const FInteractionTargetCandidate* Winner,
	float LifeTime,
	bool bDrawRejectedRays)
{
	if (!World)
	{
		return;
	}

	DrawDebugSphere(
		World,
		Debug.ViewOrigin,
		Debug.InteractionRadius,
		16,
		FColor::Cyan,
		false,
		LifeTime,
		0,
		0.25f);

	if (bDrawRejectedRays)
	{
		for (const FInteractionTargetDebugRay& Ray : Debug.Rays)
		{
			const FColor Color = (Ray.Type == EInteractionTargetDebugRayType::LosRejected) ? FColor::Yellow : FColor::Red;
			DrawDebugLine(World, Debug.ViewOrigin, Ray.TargetPoint, Color, false, LifeTime, 0, 0.0f);
			DrawDebugSphere(World, Ray.TargetPoint, 3.0f, 8, Color, false, LifeTime, 0, 0.25f);
		}
	}

	if (Winner && Winner->Actor)
	{
		DrawDebugLine(World, Debug.ViewOrigin, Winner->TargetPoint, FColor::Green, false, LifeTime, 0, 0.0f);
		DrawDebugSphere(World, Winner->TargetPoint, 4.0f, 8, FColor::Green, false, LifeTime, 0, 0.25f);
		return;
	}

	DrawDebugLine(World, Debug.ViewOrigin, Debug.MissEndPoint, FColor::Red, false, LifeTime, 0, 0.0f);
	DrawDebugSphere(World, Debug.MissEndPoint, 4.0f, 8, FColor::Red, false, LifeTime, 0, 0.25f);
}
