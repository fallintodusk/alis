// Debug draw and logging helpers for LocalBody spine tracking.
// Guarded by ENABLE_DRAW_DEBUG and CVars. No gameplay logic here.

#pragma once

#include "CoreMinimal.h"
#include "ILocalBodyCorrection.h"

class USkeletalMeshComponent;
class UCameraComponent;
class APawn;

namespace LocalBodyDebug
{
	// Draw debug arrows/lines for spine tracking visualization.
	// Called each frame when alis.LocalBody.DebugDraw != 0.
	void DrawSpineTrackingDebug(
		UWorld* World,
		const APawn* PawnOwner,
		const USkeletalMeshComponent* SourceMesh,
		const USkeletalMeshComponent* LocalBodyComp,
		const UCameraComponent* Camera,
		const FVector& CameraWorldPos,
		const FVector& NeckTargetWorld,
		const FVector& UpperSpineFollowCS,
		float UpperSpinePitchDeg,
		float UpperSpineGuardAlpha,
		const FLocalBodyFilterState& FilterState);

	// One-time idle neck offset diagnostic log.
	void LogIdleNeckOffset(
		const USkeletalMeshComponent* SourceMesh,
		const UCameraComponent* Camera,
		const FRotator& ActorRotation,
		const FVector& NeckOffsetFromCamera);

	// Returns true if debug draw CVar is enabled.
	bool IsDebugDrawEnabled();

	// Returns true if debug log CVar is enabled.
	bool IsDebugLogEnabled();
}
