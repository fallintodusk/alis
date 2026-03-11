// Copyright ALIS. All Rights Reserved.

#include "MotionDrivers/Kinematic/RotatorKinematicDriver.h"
#include "Components/SpringRotatorComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Solvers/MotionSolverLibrary.h"

FRotatorKinematicDriver::FRotatorKinematicDriver(USpringRotatorComponent& InComponent)
	: Component(&InComponent)
{
}

void FRotatorKinematicDriver::Init()
{
	if (!Component)
	{
		return;
	}

	// Calculate initial angle from actual mesh rotation (avoids jump on first tick)
	Component->CurrentAngle = CalculateAngleFromMeshRotation();
	Component->bIsOpen = !FMath::IsNearlyZero(Component->CurrentAngle, 0.5f);
	Component->bDesiredOpen = Component->bIsOpen;
	Component->TargetAngle = Component->CurrentAngle;
	Component->AngularVelocity = 0.0f;
}

float FRotatorKinematicDriver::CalculateAngleFromMeshRotation() const
{
	if (!Component || !Component->ResolvedMesh)
	{
		return Component ? (Component->bIsOpen ? Component->OpenAngle : 0.0f) : 0.0f;
	}

	const FRotator CurrentRotation = Component->ResolvedMesh->GetRelativeRotation();
	const float DeltaYaw = FRotator::NormalizeAxis(CurrentRotation.Yaw - Component->InitialRotation.Yaw);
	return FMath::Clamp(DeltaYaw, FMath::Min(0.0f, Component->OpenAngle), FMath::Max(0.0f, Component->OpenAngle));
}

void FRotatorKinematicDriver::Shutdown()
{
}

void FRotatorKinematicDriver::SetTarget(float NewTarget, bool bImmediate)
{
	if (!Component)
	{
		return;
	}

	if (bImmediate)
	{
		Component->CurrentAngle = NewTarget;
		Component->TargetAngle = NewTarget;
		Component->AngularVelocity = 0.0f;
		Component->bIsAnimating = false;
		Component->bIsOpen = !FMath::IsNearlyZero(NewTarget);
		Component->SetComponentTickEnabled(false);

		if (Component->ResolvedMesh)
		{
			FRotator NewRotation = Component->InitialRotation;
			NewRotation.Yaw += Component->CurrentAngle;
			Component->ResolvedMesh->SetRelativeRotation(NewRotation);
		}
		return;
	}

	Component->TargetAngle = NewTarget;
	Component->bIsAnimating = true;
}

float FRotatorKinematicDriver::GetCurrent() const
{
	return Component ? Component->CurrentAngle : 0.0f;
}

bool FRotatorKinematicDriver::Tick(const FMotionTickParams& Params)
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

	const float PrevAngle = Component->CurrentAngle;
	const FRotator PrevRotation = Component->ResolvedMesh->GetRelativeRotation();

	Component->CurrentAngle = UMotionSolverLibrary::SpringDamperFloat(
		Component->CurrentAngle,
		Params.Target,
		Component->AngularVelocity,
		Params.DeltaTime,
		Component->KinematicConfig.Stiffness,
		Component->KinematicConfig.Damping
	);

	FRotator NewRotation = Component->InitialRotation;
	NewRotation.Yaw += Component->CurrentAngle;
	Component->ResolvedMesh->SetRelativeRotation(NewRotation);

	bool bBlockedByPawn = false;
	if (UWorld* World = Component->GetWorld())
	{
		TArray<FOverlapResult> Overlaps;
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MotionHingeGate), false, Component->GetOwner());
		FCollisionObjectQueryParams ObjectParams;
		ObjectParams.AddObjectTypesToQuery(ECC_Pawn);

		const FBoxSphereBounds Bounds = Component->ResolvedMesh->Bounds;
		bBlockedByPawn = World->OverlapMultiByObjectType(
			Overlaps,
			Bounds.Origin,
			Component->ResolvedMesh->GetComponentQuat(),
			ObjectParams,
			FCollisionShape::MakeBox(Bounds.BoxExtent),
			QueryParams);
	}

	if (bBlockedByPawn)
	{
		Component->CurrentAngle = PrevAngle;
		Component->AngularVelocity = 0.0f;
		Component->ResolvedMesh->SetRelativeRotation(PrevRotation);
		return true;
	}

	const float AngleTolerance = 0.5f;
	const float VelocityTolerance = 0.1f;

	if (FMath::IsNearlyEqual(Component->CurrentAngle, Params.Target, AngleTolerance) &&
		FMath::IsNearlyZero(Component->AngularVelocity, VelocityTolerance))
	{
		Component->CurrentAngle = Params.Target;
		Component->AngularVelocity = 0.0f;

		FRotator FinalRotation = Component->InitialRotation;
		FinalRotation.Yaw += Component->CurrentAngle;
		Component->ResolvedMesh->SetRelativeRotation(FinalRotation);

		Component->bIsOpen = !FMath::IsNearlyZero(Component->CurrentAngle);
		Component->bIsAnimating = false;

		Component->OnMotionComplete(Component->bIsOpen);
		return false;
	}

	return true;
}
