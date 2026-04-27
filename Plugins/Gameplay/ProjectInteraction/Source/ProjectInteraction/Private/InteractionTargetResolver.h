// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InteractionComponent.h"

class APawn;
class AActor;
class UPrimitiveComponent;
class UWorld;

DECLARE_LOG_CATEGORY_EXTERN(LogInteraction, Log, All);

// Default values mirror UInteractionComponent's static constexpr Defaults so the
// component remains the single source of truth. BuildWeights() always overwrites
// these with the live component values; defaults exist only for safety when the
// struct is constructed without a component (tests, fixtures).
struct FInteractionTargetingWeights
{
	float MinAimDot = UInteractionComponent::DefaultMinAimDot;
	float InteractionRadius = UInteractionComponent::DefaultInteractionRadius;
	float ShortCircuitRadius = UInteractionComponent::DefaultShortCircuitRadius;
};

// Three-bucket signal for "is the view ray under this candidate" - drives the
// comparator's primary bucket order.
//   Collision: LineTraceComponent hit. Real ray pierce against the candidate's
//              physics shape. Most trustworthy "the player is looking at this".
//   Bounds:    AABB ray intersection succeeded but collision did not. Catches
//              meshes whose visible geometry has no/sparse collision (window
//              glass on a frame-only body, decorative meshes). The bounds box
//              is loose for thick or rotated meshes, so a bounds-only hit is
//              weaker than a collision hit and must NOT outrank a Collision
//              candidate even when its hit distance is closer (the backpack
//              case where a large AABB clips above a smaller real-collision
//              target).
//   None:      Neither collision nor bounds intersected the ray. Falls back to
//              closest-point on collision; ranks by AimDot.
enum class EInteractionViewRayHitKind : uint8
{
	None,
	Bounds,
	Collision
};

struct FInteractionTargetCandidate
{
	AActor* Actor = nullptr;
	UPrimitiveComponent* Component = nullptr;
	FVector TargetPoint = FVector::ZeroVector;
	float Distance = 0.0f;
	float AimDot = -1.0f;
	int32 Priority = 0;

	// Bucket signal: how the view ray relates to this candidate. See
	// EInteractionViewRayHitKind for semantics. The comparator orders buckets
	// Collision > Bounds > None; ViewRayHitDistance is the within-bucket
	// discriminator for Collision and Bounds (front-most wins).
	EInteractionViewRayHitKind ViewRayHitKind = EInteractionViewRayHitKind::None;
	float ViewRayHitDistance = TNumericLimits<float>::Max();
};

enum class EInteractionTargetDebugRayType : uint8
{
	AimRejected,
	LosRejected
};

struct FInteractionTargetDebugRay
{
	FVector TargetPoint = FVector::ZeroVector;
	EInteractionTargetDebugRayType Type = EInteractionTargetDebugRayType::AimRejected;
};

struct FInteractionTargetDebugSnapshot
{
	FVector ViewOrigin = FVector::ZeroVector;
	FVector ViewForward = FVector::ForwardVector;
	FVector MissEndPoint = FVector::ZeroVector;
	float InteractionRadius = 0.0f;
	TArray<FInteractionTargetDebugRay> Rays;

	void AddRay(const FVector& InTargetPoint, EInteractionTargetDebugRayType InType)
	{
		FInteractionTargetDebugRay& Ray = Rays.AddDefaulted_GetRef();
		Ray.TargetPoint = InTargetPoint;
		Ray.Type = InType;
	}
};

class FInteractionTargetResolver
{
public:
	FInteractionTargetResolver(
		UWorld* InWorld,
		const FVector& InViewOrigin,
		const FVector& InViewForward,
		ECollisionChannel InLineOfSightChannel,
		AActor* InIgnoreActor,
		const FInteractionTargetingWeights& InWeights);

	bool Resolve(
		FInteractionTargetCandidate& OutWinner,
		FInteractionTargetDebugSnapshot* OutDebug = nullptr,
		const AActor* TrackedActor = nullptr,
		FInteractionTargetCandidate* OutTrackedCandidate = nullptr) const;

	static FInteractionTargetingWeights BuildWeights(const UInteractionComponent& Component);
	static bool ShouldKeepCurrentCandidate(
		const FInteractionTargetCandidate& CurrentFocus,
		const FInteractionTargetCandidate& NewWinner,
		float FocusSwitchHysteresis,
		float MinAimDot);
	static float GetDebugLifetime(const UInteractionComponent& Component);
	static bool ResolvePlayerCameraView(APawn* Pawn, FVector& OutViewOrigin, FVector& OutViewForward);
	static bool ResolveActorEyesView(APawn* Pawn, FVector& OutViewOrigin, FVector& OutViewForward);
	static void DrawDebug(
		UWorld* World,
		const FInteractionTargetDebugSnapshot& Debug,
		const FInteractionTargetCandidate* Winner,
		float LifeTime,
		bool bDrawRejectedRays);

private:
	UWorld* World = nullptr;
	FVector ViewOrigin = FVector::ZeroVector;
	FVector ViewForward = FVector::ForwardVector;
	ECollisionChannel LineOfSightChannel = ECC_Visibility;
	TWeakObjectPtr<AActor> IgnoreActor;
	FInteractionTargetingWeights Weights;
};
