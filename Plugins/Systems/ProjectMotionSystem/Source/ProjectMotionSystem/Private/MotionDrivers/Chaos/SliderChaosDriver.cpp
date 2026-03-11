// Copyright ALIS. All Rights Reserved.

#include "MotionDrivers/Chaos/SliderChaosDriver.h"
#include "Components/SpringSliderComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "ProjectMotionSystemModule.h"
#include "Solvers/MotionConstraintHelpers.h"

FSliderChaosDriver::FSliderChaosDriver(USpringSliderComponent& InComponent)
	: Component(&InComponent)
{
}

void FSliderChaosDriver::Init()
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

void FSliderChaosDriver::Shutdown()
{
	bShuttingDown = true;
	CancelRetry();
	DestroyConstraint();
}

void FSliderChaosDriver::SetTarget(float NewTarget, bool bImmediate)
{
	if (!Component)
	{
		return;
	}

	const float Clamped = FMath::Clamp(NewTarget, Component->MinSlideRatio, Component->MaxSlideRatio);
	const float OpenDist = Component->ChaosConfig.SliderLimitCm > 0.0f ? Component->ChaosConfig.SliderLimitCm : Component->OpenDistance;
	CurrentTargetPosition = OpenDist * Clamped;

	if (bImmediate)
	{
		UE_LOG(LogProjectMotionSystem, Warning,
			TEXT("SetTarget immediate in Chaos mode is best-effort (uses motor target)"));
	}

	Component->bMotorActive = true;
	SetMotorEnabled(true);

	if (Constraint.IsValid())
	{
		Constraint->SetLinearPositionTarget(FVector(CurrentTargetPosition, 0.f, 0.f));
	}
}

float FSliderChaosDriver::GetCurrent() const
{
	if (!Component || !Component->ResolvedMesh || !CachedAnchorComp.IsValid())
	{
		return 0.0f;
	}

	const FVector Displacement = Component->ResolvedMesh->GetComponentLocation() - CachedAnchorComp->GetComponentLocation();
	return FVector::DotProduct(Displacement, CachedSlideAxisWorld);
}

bool FSliderChaosDriver::Tick(const FMotionTickParams& Params)
{
	if (!Component || !Params.bActive)
	{
		return false;
	}

	if (!Constraint.IsValid())
	{
		return false;
	}

	const float Error = FMath::Abs(GetCurrent() - CurrentTargetPosition);
	if (Error < Component->ChaosConfig.SliderMotorSleepThreshold)
	{
		SetMotorEnabled(false);
		Component->bMotorActive = false;
		Component->bIsOpen = Component->TargetProgress > Component->MinSlideRatio + KINDA_SMALL_NUMBER;
		Component->OnMotionComplete(Component->bIsOpen);
		return false;
	}

	return true;
}

void FSliderChaosDriver::SetupConstraint()
{
	if (!Component || bShuttingDown)
	{
		return;
	}

	if (Constraint.IsValid() && Constraint->IsRegistered())
	{
		UE_LOG(LogProjectMotionSystem, Warning,
			TEXT("SpringSliderComponent::SetupPhysicsConstraint: Already setup, skipping"));
		return;
	}

	if (!Component->ResolvedMesh)
	{
		UE_LOG(LogProjectMotionSystem, Warning,
			TEXT("SpringSliderComponent::SetupPhysicsConstraint: No resolved mesh"));
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
			TEXT("SpringSliderComponent::SetupPhysicsConstraint: Failed to find/create anchor"));
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

	const FVector PivotWorld = Component->ResolvedMesh->GetComponentTransform().TransformPosition(Component->ChaosConfig.SliderPivotLocal);

	const FVector PivotInAnchor = CachedAnchorComp->GetComponentTransform().InverseTransformPosition(PivotWorld);
	const FVector PivotInSlider = Component->ResolvedMesh->GetComponentTransform().InverseTransformPosition(PivotWorld);

	const FVector SlideAxis = Component->ChaosConfig.SliderAxisLocal.GetSafeNormal();
	const FQuat FrameQ = FQuat::FindBetweenNormals(FVector::ForwardVector, SlideAxis);

	const FTransform Frame1(FrameQ, PivotInAnchor);
	const FTransform Frame2(FrameQ, PivotInSlider);

	Constraint->SetConstraintReferenceFrame(EConstraintFrame::Frame1, Frame1);
	Constraint->SetConstraintReferenceFrame(EConstraintFrame::Frame2, Frame2);

	CachedSlideAxisWorld = CachedAnchorComp->GetComponentTransform().TransformVectorNoScale(SlideAxis);

	Constraint->SetDisableCollision(true);

	const float Limit = Component->ChaosConfig.SliderLimitCm > 0.0f ? Component->ChaosConfig.SliderLimitCm : Component->OpenDistance;
	Constraint->SetLinearXLimit(ELinearConstraintMotion::LCM_Limited, Limit);
	Constraint->SetLinearYLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);
	Constraint->SetLinearZLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);

	Constraint->SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Locked, 0.0f);
	Constraint->SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Locked, 0.0f);
	Constraint->SetAngularTwistLimit(EAngularConstraintMotion::ACM_Locked, 0.0f);

	Constraint->SetLinearPositionDrive(true, false, false);
	Constraint->SetLinearDriveParams(Component->ChaosConfig.SliderMotorStrength, Component->ChaosConfig.SliderMotorDamping, Component->ChaosConfig.SliderMotorMaxForce);

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
		TEXT("SpringSliderComponent: Created constraint with limit %.1f cm, motor strength %.0f"),
		Limit, Component->ChaosConfig.SliderMotorStrength);

	if (Component->bMotorActive)
	{
		SetMotorEnabled(true);
		Constraint->SetLinearPositionTarget(FVector(CurrentTargetPosition, 0.f, 0.f));
	}
}

void FSliderChaosDriver::ScheduleRetry(const TCHAR* Reason)
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
		FTimerDelegate::CreateRaw(this, &FSliderChaosDriver::RetrySetup),
		RetryDelaySeconds,
		false);
}

void FSliderChaosDriver::CancelRetry()
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

void FSliderChaosDriver::RetrySetup()
{
	bPendingRetry = false;
	if (bShuttingDown)
	{
		return;
	}

	SetupConstraint();
}

void FSliderChaosDriver::DestroyConstraint()
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

void FSliderChaosDriver::SetMotorEnabled(bool bEnable)
{
	if (!Constraint.IsValid())
	{
		return;
	}

	Constraint->SetLinearPositionDrive(bEnable, false, false);

	if (!bEnable)
	{
		Constraint->SetLinearDriveParams(0.f, Component->ChaosConfig.SliderMotorDamping, 0.f);
	}
	else
	{
		Constraint->SetLinearDriveParams(Component->ChaosConfig.SliderMotorStrength, Component->ChaosConfig.SliderMotorDamping, Component->ChaosConfig.SliderMotorMaxForce);
	}
}
