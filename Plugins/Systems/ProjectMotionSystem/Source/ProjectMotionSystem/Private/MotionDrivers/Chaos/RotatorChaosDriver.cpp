// Copyright ALIS. All Rights Reserved.

#include "MotionDrivers/Chaos/RotatorChaosDriver.h"
#include "Components/SpringRotatorComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "ProjectMotionSystemModule.h"
#include "Solvers/MotionConstraintHelpers.h"

FRotatorChaosDriver::FRotatorChaosDriver(USpringRotatorComponent& InComponent)
	: Component(&InComponent)
{
}

void FRotatorChaosDriver::Init()
{
	if (!Component)
	{
		return;
	}

	bShuttingDown = false;
	CancelRetry();
	RetryAttempts = 0;

	FMotionConstraintHelpers::EnablePhysicsOnMesh(Component->ResolvedMesh);
	SetupConstraint();
}

void FRotatorChaosDriver::Shutdown()
{
	bShuttingDown = true;
	CancelRetry();
	DestroyConstraint();
}

void FRotatorChaosDriver::SetTarget(float NewTarget, bool /*bImmediate*/)
{
	if (!Component)
	{
		return;
	}

	CurrentTargetAngle = NewTarget;
	Component->bMotorActive = true;
	SetMotorEnabled(true);

	if (Constraint.IsValid())
	{
		Constraint->SetAngularOrientationTarget(FRotator(CurrentTargetAngle, 0.f, 0.f));
	}
}

float FRotatorChaosDriver::GetCurrent() const
{
	return Constraint.IsValid() ? Constraint->ConstraintInstance.GetCurrentSwing1() : 0.0f;
}

bool FRotatorChaosDriver::Tick(const FMotionTickParams& Params)
{
	if (!Component || !Params.bActive)
	{
		return false;
	}

	if (!Constraint.IsValid())
	{
		return false;
	}

	const float Current = GetCurrent();
	const float Error = FMath::Abs(Current - Params.Target);

	if (Error < Component->ChaosConfig.HingeMotorSleepThreshold)
	{
		SetMotorEnabled(false);
		Component->bMotorActive = false;
		Component->bIsOpen = !FMath::IsNearlyZero(Params.Target);
		Component->OnMotionComplete(Component->bIsOpen);
		return false;
	}

	return true;
}

void FRotatorChaosDriver::SetupConstraint()
{
	if (!Component || bShuttingDown)
	{
		return;
	}

	if (Constraint.IsValid() && Constraint->IsRegistered())
	{
		UE_LOG(LogProjectMotionSystem, Warning,
			TEXT("SpringRotatorComponent::SetupPhysicsConstraint: Already setup, skipping"));
		return;
	}

	if (!Component->ResolvedMesh)
	{
		UE_LOG(LogProjectMotionSystem, Warning,
			TEXT("SpringRotatorComponent::SetupPhysicsConstraint: No resolved mesh"));
		return;
	}

	AActor* Owner = Component->GetOwner();
	if (!Owner)
	{
		return;
	}

	UBoxComponent* CreatedAnchor = nullptr;
	CachedAnchorComp = FMotionConstraintHelpers::FindOrCreateAnchor(Component->ResolvedMesh, CreatedAnchor);
	if (!CachedAnchorComp.IsValid())
	{
		UE_LOG(LogProjectMotionSystem, Error,
			TEXT("SpringRotatorComponent::SetupPhysicsConstraint: Failed to find/create anchor"));
		ScheduleRetry(TEXT("anchor not available"));
		return;
	}

	AnchorBox = CreatedAnchor;

	if (!Component->ResolvedMesh->IsRegistered() || !Component->ResolvedMesh->IsPhysicsStateCreated())
	{
		UE_LOG(LogProjectMotionSystem, Error,
			TEXT("[%s] SetupPhysicsConstraint FAILED: Mesh physics not ready. Registered=%d, PhysicsState=%d, Collision=%d"),
			*Owner->GetActorNameOrLabel(),
			Component->ResolvedMesh->IsRegistered(),
			Component->ResolvedMesh->IsPhysicsStateCreated(),
			(int)Component->ResolvedMesh->GetCollisionEnabled());
		ScheduleRetry(TEXT("mesh physics not ready"));
		return;
	}
	if (!CachedAnchorComp->IsRegistered() || !CachedAnchorComp->IsPhysicsStateCreated())
	{
		UE_LOG(LogProjectMotionSystem, Error,
			TEXT("[%s] SetupPhysicsConstraint FAILED: Anchor physics not ready. Registered=%d, PhysicsState=%d, Collision=%d, IsCreatedAnchor=%d"),
			*Owner->GetActorNameOrLabel(),
			CachedAnchorComp->IsRegistered(),
			CachedAnchorComp->IsPhysicsStateCreated(),
			(int)CachedAnchorComp->GetCollisionEnabled(),
			AnchorBox.IsValid() ? 1 : 0);
		ScheduleRetry(TEXT("anchor physics not ready"));
		return;
	}

	UE_LOG(LogProjectMotionSystem, Log,
		TEXT("[%s] SetupPhysicsConstraint: Physics state OK. Mesh Collision=%d, Anchor Collision=%d"),
		*Owner->GetActorNameOrLabel(),
		(int)Component->ResolvedMesh->GetCollisionEnabled(),
		(int)CachedAnchorComp->GetCollisionEnabled());

	Constraint = NewObject<UPhysicsConstraintComponent>(Owner, NAME_None, RF_Transient);
	Owner->AddInstanceComponent(Constraint.Get());
	Constraint->OnComponentCreated();

	Constraint->SetupAttachment(CachedAnchorComp.Get());
	Constraint->RegisterComponent();

	Constraint->SetConstrainedComponents(CachedAnchorComp.Get(), NAME_None, Component->ResolvedMesh, NAME_None);

	const FVector PivotWorld = Component->ResolvedMesh->GetComponentTransform().TransformPosition(Component->ChaosConfig.HingePivotLocal);

	const FVector PivotInAnchor = CachedAnchorComp->GetComponentTransform().InverseTransformPosition(PivotWorld);
	const FVector PivotInDoor = Component->ResolvedMesh->GetComponentTransform().InverseTransformPosition(PivotWorld);

	const FQuat FrameQ = FQuat(FRotator(
		Component->ChaosConfig.HingeFrameRotationPYR.X,
		Component->ChaosConfig.HingeFrameRotationPYR.Y,
		Component->ChaosConfig.HingeFrameRotationPYR.Z
	));

	FTransform Frame1(FrameQ, PivotInAnchor);
	FTransform Frame2(FrameQ, PivotInDoor);

	Constraint->SetConstraintReferenceFrame(EConstraintFrame::Frame1, Frame1);
	Constraint->SetConstraintReferenceFrame(EConstraintFrame::Frame2, Frame2);

	Constraint->SetDisableCollision(true);

	Constraint->SetLinearXLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);
	Constraint->SetLinearYLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);
	Constraint->SetLinearZLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);

	const float Limit = Component->ChaosConfig.HingeLimitDeg > 0.0f ? Component->ChaosConfig.HingeLimitDeg : FMath::Abs(Component->OpenAngle);
	Constraint->SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Limited, Limit);
	Constraint->SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Locked, 0.0f);
	Constraint->SetAngularTwistLimit(EAngularConstraintMotion::ACM_Locked, 0.0f);

	Constraint->ConstraintInstance.SetAngularDriveMode(EAngularDriveMode::TwistAndSwing);
	Constraint->SetOrientationDriveTwistAndSwing(false, true);
	Constraint->SetAngularDriveParams(Component->ChaosConfig.HingeMotorStrength, Component->ChaosConfig.HingeMotorDamping, Component->ChaosConfig.HingeMotorMaxForce);

	CancelRetry();

	UE_LOG(LogProjectMotionSystem, Log,
		TEXT("[%s] SetupPhysicsConstraint OK: Constraint=%s Anchor=%s Mesh=%s ConstraintPtr=%p AnchorPtr=%p MeshPtr=%p"),
		*Owner->GetActorNameOrLabel(),
		Constraint.IsValid() ? *Constraint->GetName() : TEXT("null"),
		CachedAnchorComp.IsValid() ? *CachedAnchorComp->GetName() : TEXT("null"),
		Component->ResolvedMesh ? *Component->ResolvedMesh->GetName() : TEXT("null"),
		Constraint.Get(),
		CachedAnchorComp.Get(),
		Component->ResolvedMesh.Get());

	UE_LOG(LogProjectMotionSystem, Verbose,
		TEXT("SpringRotatorComponent: Created constraint with limit %.1f deg, motor strength %.0f"),
		Limit, Component->ChaosConfig.HingeMotorStrength);

	if (Component->bMotorActive)
	{
		SetMotorEnabled(true);
		Constraint->SetAngularOrientationTarget(FRotator(CurrentTargetAngle, 0.f, 0.f));
	}
}

void FRotatorChaosDriver::ScheduleRetry(const TCHAR* Reason)
{
	if (!Component || bShuttingDown)
	{
		return;
	}

	if (bPendingRetry)
	{
		return;
	}

	UWorld* World = Component->GetWorld();
	if (!World)
	{
		return;
	}

	if (RetryAttempts >= MaxRetryAttempts)
	{
		UE_LOG(LogProjectMotionSystem, Warning,
			TEXT("[%s] SetupPhysicsConstraint: retry limit reached (%d). LastReason=%s"),
			Component->GetOwner() ? *Component->GetOwner()->GetActorNameOrLabel() : TEXT("NULL"),
			MaxRetryAttempts,
			Reason ? Reason : TEXT("unknown"));
		return;
	}

	++RetryAttempts;
	bPendingRetry = true;

	UE_LOG(LogProjectMotionSystem, Warning,
		TEXT("[%s] SetupPhysicsConstraint: retry %d/%d in %.2fs (Reason=%s)"),
		Component->GetOwner() ? *Component->GetOwner()->GetActorNameOrLabel() : TEXT("NULL"),
		RetryAttempts,
		MaxRetryAttempts,
		RetryDelaySeconds,
		Reason ? Reason : TEXT("unknown"));

	World->GetTimerManager().SetTimer(
		RetryHandle,
		FTimerDelegate::CreateRaw(this, &FRotatorChaosDriver::RetrySetup),
		RetryDelaySeconds,
		false);
}

void FRotatorChaosDriver::CancelRetry()
{
	if (Component)
	{
		if (UWorld* World = Component->GetWorld())
		{
			World->GetTimerManager().ClearTimer(RetryHandle);
		}
	}
	bPendingRetry = false;
}

void FRotatorChaosDriver::RetrySetup()
{
	bPendingRetry = false;
	if (bShuttingDown)
	{
		return;
	}

	SetupConstraint();
}

void FRotatorChaosDriver::DestroyConstraint()
{
	if (Constraint.IsValid())
	{
		Constraint->BreakConstraint();
		Constraint->DestroyComponent();
		Constraint = nullptr;
	}

	if (AnchorBox.IsValid())
	{
		AnchorBox->DestroyComponent();
		AnchorBox = nullptr;
	}

	CachedAnchorComp = nullptr;
}

void FRotatorChaosDriver::SetMotorEnabled(bool bEnable)
{
	if (!Constraint.IsValid())
	{
		return;
	}

	Constraint->SetOrientationDriveTwistAndSwing(false, bEnable);

	if (!bEnable)
	{
		Constraint->SetAngularDriveParams(0.f, Component->ChaosConfig.HingeMotorDamping, 0.f);
	}
	else
	{
		Constraint->SetAngularDriveParams(Component->ChaosConfig.HingeMotorStrength, Component->ChaosConfig.HingeMotorDamping, Component->ChaosConfig.HingeMotorMaxForce);
	}
}
