// Copyright ALIS. All Rights Reserved.

#include "MotionDrivers/Kinematic/SliderKinematicDriver.h"
#include "Components/SpringSliderComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/EngineTypes.h"
#include "Solvers/MotionSolverLibrary.h"

FSliderKinematicDriver::FSliderKinematicDriver(USpringSliderComponent& InComponent)
	: Component(&InComponent)
{
}

void FSliderKinematicDriver::Init()
{
	if (!Component)
	{
		return;
	}

	// Calculate initial progress from actual mesh position (avoids jump on first tick)
	Component->CurrentProgress = CalculateProgressFromMeshPosition();
	Component->bIsOpen = Component->CurrentProgress > Component->MinSlideRatio + KINDA_SMALL_NUMBER;
	Component->bDesiredOpen = Component->bIsOpen;
	Component->TargetProgress = Component->CurrentProgress;
	Component->ProgressVelocity = 0.0f;
}

float FSliderKinematicDriver::CalculateProgressFromMeshPosition() const
{
	if (!Component || !Component->ResolvedMesh)
	{
		return Component ? (Component->bIsOpen ? Component->MaxSlideRatio : Component->MinSlideRatio) : 0.0f;
	}

	const FVector CurrentLocation = Component->ResolvedMesh->GetRelativeLocation();

	if (Component->IsPositionMode())
	{
		const FVector Range = Component->OpenPosition - Component->ClosedPosition;
		const float RangeSq = Range.SizeSquared();
		if (RangeSq < KINDA_SMALL_NUMBER)
		{
			return Component->MinSlideRatio;
		}
		const FVector Delta = CurrentLocation - Component->ClosedPosition;
		const float Progress = FVector::DotProduct(Delta, Range) / RangeSq;
		return FMath::Clamp(Progress, Component->MinSlideRatio, Component->MaxSlideRatio);
	}

	// Direction mode: offset from InitialLocation
	const FVector Dir = Component->SlideDirection.GetSafeNormal();
	if (Component->OpenDistance < KINDA_SMALL_NUMBER || Dir.IsNearlyZero())
	{
		return Component->MinSlideRatio;
	}
	const FVector Delta = CurrentLocation - Component->InitialLocation;
	const float Progress = FVector::DotProduct(Delta, Dir) / Component->OpenDistance;
	return FMath::Clamp(Progress, Component->MinSlideRatio, Component->MaxSlideRatio);
}

void FSliderKinematicDriver::Shutdown()
{
}

void FSliderKinematicDriver::SetTarget(float NewTarget, bool bImmediate)
{
	if (!Component)
	{
		return;
	}

	const float Clamped = FMath::Clamp(NewTarget, Component->MinSlideRatio, Component->MaxSlideRatio);

	if (bImmediate)
	{
		Component->CurrentProgress = Clamped;
		Component->TargetProgress = Clamped;
		Component->ProgressVelocity = 0.0f;
		Component->bIsAnimating = false;
		Component->bIsOpen = Clamped > Component->MinSlideRatio + KINDA_SMALL_NUMBER;
		Component->SetComponentTickEnabled(false);

		if (Component->ResolvedMesh)
		{
			const FVector NewLocation = Component->CalculatePositionFromProgress(Component->CurrentProgress);
			Component->ResolvedMesh->SetRelativeLocation(NewLocation);
		}
		return;
	}

	Component->TargetProgress = Clamped;
	Component->bIsAnimating = true;
}

float FSliderKinematicDriver::GetCurrent() const
{
	return Component ? Component->CurrentProgress : 0.0f;
}

bool FSliderKinematicDriver::Tick(const FMotionTickParams& Params)
{
	if (!Component || !Params.bActive)
	{
		return false;
	}

	if (!Component->ResolvedMesh)
	{
		Component->bIsAnimating = false;
		return false;
	}

	const float PrevProgress = Component->CurrentProgress;
	const FVector PrevLocation = Component->ResolvedMesh->GetRelativeLocation();

	Component->CurrentProgress = UMotionSolverLibrary::SpringDamperFloat(
		Component->CurrentProgress,
		Params.Target,
		Component->ProgressVelocity,
		Params.DeltaTime,
		Component->KinematicConfig.Stiffness,
		Component->KinematicConfig.Damping
	);

	Component->CurrentProgress = FMath::Clamp(Component->CurrentProgress, Component->MinSlideRatio, Component->MaxSlideRatio);

	const FVector NewLocation = Component->CalculatePositionFromProgress(Component->CurrentProgress);
	FHitResult Hit;
	Component->ResolvedMesh->SetRelativeLocation(NewLocation, true, &Hit);

	if (Hit.IsValidBlockingHit())
	{
		Component->CurrentProgress = PrevProgress;
		Component->ProgressVelocity = 0.0f;
		Component->ResolvedMesh->SetRelativeLocation(PrevLocation, false);
		return true;
	}

	const float ProgressTolerance = 0.005f;
	const float VelocityTolerance = 0.001f;

	if (FMath::IsNearlyEqual(Component->CurrentProgress, Params.Target, ProgressTolerance) &&
		FMath::IsNearlyZero(Component->ProgressVelocity, VelocityTolerance))
	{
		Component->CurrentProgress = Params.Target;
		Component->ProgressVelocity = 0.0f;

		const FVector FinalLocation = Component->CalculatePositionFromProgress(Component->CurrentProgress);
		Component->ResolvedMesh->SetRelativeLocation(FinalLocation);

		Component->bIsOpen = Component->CurrentProgress > Component->MinSlideRatio + KINDA_SMALL_NUMBER;
		Component->bIsAnimating = false;

		Component->OnMotionComplete(Component->bIsOpen);
		return false;
	}

	return true;
}
