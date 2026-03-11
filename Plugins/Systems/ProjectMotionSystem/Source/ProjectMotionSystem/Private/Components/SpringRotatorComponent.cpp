// Copyright ALIS. All Rights Reserved.

#include "Components/SpringRotatorComponent.h"
#include "CapabilityValidationRegistry.h"
#include "Components/StaticMeshComponent.h"
#include "MotionDrivers/Chaos/RotatorChaosDriver.h"
#include "MotionDrivers/Kinematic/RotatorKinematicDriver.h"
#include "MotionDrivers/RotatorMotionDriver.h"
#include "ProjectMotionSystemModule.h"

// Self-registration: component owns its validation registration
#if WITH_EDITOR
static struct FHingedValidationAutoReg
{
	FHingedValidationAutoReg()
	{
		FCapabilityValidationRegistry::RegisterValidation(
			FName(TEXT("Hinged")),
			&USpringRotatorComponent::ValidateCapabilityProperties);
	}
} GHingedValidationAutoReg;
#endif

struct USpringRotatorComponent::FMotionDriverState
{
	TUniquePtr<IRotatorMotionDriver> Driver;
};

USpringRotatorComponent::USpringRotatorComponent()
{
	// Base class handles tick setup
}

USpringRotatorComponent::~USpringRotatorComponent() = default;

void USpringRotatorComponent::PostLoad()
{
	Super::PostLoad();

	const USpringRotatorComponent* CDO = GetDefault<USpringRotatorComponent>();
	if (CDO)
	{
		if (KinematicConfig.Stiffness == CDO->KinematicConfig.Stiffness &&
			Stiffness_DEPRECATED != CDO->Stiffness_DEPRECATED)
		{
			KinematicConfig.Stiffness = Stiffness_DEPRECATED;
		}
		if (KinematicConfig.Damping == CDO->KinematicConfig.Damping &&
			Damping_DEPRECATED != CDO->Damping_DEPRECATED)
		{
			KinematicConfig.Damping = Damping_DEPRECATED;
		}

		if (ChaosConfig.HingePivotLocal == CDO->ChaosConfig.HingePivotLocal &&
			HingePivotLocal_DEPRECATED != CDO->HingePivotLocal_DEPRECATED)
		{
			ChaosConfig.HingePivotLocal = HingePivotLocal_DEPRECATED;
		}
		if (ChaosConfig.HingeFrameRotationPYR == CDO->ChaosConfig.HingeFrameRotationPYR &&
			HingeFrameRotationPYR_DEPRECATED != CDO->HingeFrameRotationPYR_DEPRECATED)
		{
			ChaosConfig.HingeFrameRotationPYR = HingeFrameRotationPYR_DEPRECATED;
		}
		if (ChaosConfig.HingeLimitDeg == CDO->ChaosConfig.HingeLimitDeg &&
			HingeLimitDeg_DEPRECATED != CDO->HingeLimitDeg_DEPRECATED)
		{
			ChaosConfig.HingeLimitDeg = HingeLimitDeg_DEPRECATED;
		}
		if (ChaosConfig.HingeMotorStrength == CDO->ChaosConfig.HingeMotorStrength &&
			HingeMotorStrength_DEPRECATED != CDO->HingeMotorStrength_DEPRECATED)
		{
			ChaosConfig.HingeMotorStrength = HingeMotorStrength_DEPRECATED;
		}
		if (ChaosConfig.HingeMotorDamping == CDO->ChaosConfig.HingeMotorDamping &&
			HingeMotorDamping_DEPRECATED != CDO->HingeMotorDamping_DEPRECATED)
		{
			ChaosConfig.HingeMotorDamping = HingeMotorDamping_DEPRECATED;
		}
		if (ChaosConfig.HingeMotorMaxForce == CDO->ChaosConfig.HingeMotorMaxForce &&
			HingeMotorMaxForce_DEPRECATED != CDO->HingeMotorMaxForce_DEPRECATED)
		{
			ChaosConfig.HingeMotorMaxForce = HingeMotorMaxForce_DEPRECATED;
		}
		if (ChaosConfig.HingeMotorSleepThreshold == CDO->ChaosConfig.HingeMotorSleepThreshold &&
			HingeMotorSleepThreshold_DEPRECATED != CDO->HingeMotorSleepThreshold_DEPRECATED)
		{
			ChaosConfig.HingeMotorSleepThreshold = HingeMotorSleepThreshold_DEPRECATED;
		}
	}
}

FPrimaryAssetId USpringRotatorComponent::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(FPrimaryAssetType(TEXT("CapabilityComponent")), FName(TEXT("Hinged")));
}

void USpringRotatorComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!bCapturedInitialRotation && ResolvedMesh)
	{
		InitialRotation = ResolvedMesh->GetRelativeRotation();
		bCapturedInitialRotation = true;
	}

	SelectMotionDriver();
	if (DriverState && DriverState->Driver)
	{
		DriverState->Driver->Init();
	}

	UE_LOG(LogProjectMotionSystem, Log,
		TEXT("SpringRotatorComponent on %s: InitialRotation = %s, OpenAngle = %.1f"),
		*GetOwner()->GetName(),
		*InitialRotation.ToString(),
		OpenAngle);
}

void USpringRotatorComponent::SelectMotionDriver()
{
	DriverState = MakeUnique<FMotionDriverState>();
	if (ShouldUseChaosMode())
	{
		DriverState->Driver = MakeUnique<FRotatorChaosDriver>(*this);
	}
	else
	{
		DriverState->Driver = MakeUnique<FRotatorKinematicDriver>(*this);
	}
}

// -------------------------------------------------------------------------
// Base Class Overrides
// -------------------------------------------------------------------------

bool USpringRotatorComponent::TickMotion(float DeltaTime)
{
	if (!DriverState || !DriverState->Driver)
	{
		return false;
	}

	FMotionTickParams Params;
	Params.DeltaTime = DeltaTime;
	Params.Target = TargetAngle;
	Params.bActive = bIsAnimating || bMotorActive;

	return DriverState->Driver->Tick(Params);
}

void USpringRotatorComponent::ShutdownMotionDriver()
{
	if (DriverState && DriverState->Driver)
	{
		DriverState->Driver->Shutdown();
	}
	DriverState.Reset();
}

void USpringRotatorComponent::OnMotionComplete(bool bOpened)
{
	OnRotationComplete.Broadcast(bOpened);
}

void USpringRotatorComponent::DoOpen()
{
	TargetAngle = OpenAngle;
	if (DriverState && DriverState->Driver)
	{
		DriverState->Driver->SetTarget(TargetAngle, false);
	}
}

void USpringRotatorComponent::DoClose()
{
	TargetAngle = 0.0f;
	if (DriverState && DriverState->Driver)
	{
		DriverState->Driver->SetTarget(TargetAngle, false);
	}
}

// -------------------------------------------------------------------------
// Rotator-Specific Methods
// -------------------------------------------------------------------------

void USpringRotatorComponent::SetAngleImmediate(float Angle)
{
	TargetAngle = Angle;
	bDesiredOpen = !FMath::IsNearlyZero(Angle);
	if (DriverState && DriverState->Driver)
	{
		DriverState->Driver->SetTarget(Angle, true);
		SetComponentTickEnabled(true);
	}
}

// -------------------------------------------------------------------------
// Capability Validation
// -------------------------------------------------------------------------

TArray<FCapabilityValidationResult> USpringRotatorComponent::ValidateCapabilityProperties(
	const TMap<FName, FString>& Properties)
{
	TArray<FCapabilityValidationResult> Results;
	const USpringRotatorComponent* CDO = GetDefault<USpringRotatorComponent>();

	float OpenAngle = CDO->OpenAngle;
	if (const FString* Value = Properties.Find(FName(TEXT("OpenAngle"))))
	{
		OpenAngle = FCString::Atof(**Value);
	}

	// Error if no rotation possible
	if (FMath::IsNearlyZero(OpenAngle))
	{
		FCapabilityValidationResult R;
		R.PropertyPath = TEXT("OpenAngle");
		R.Message = TEXT("OpenAngle is zero. Mesh will not rotate.");
		R.bIsWarning = false;
		Results.Add(R);
	}

	// Parse all Chaos properties (matching ShouldUseChaosMode logic)
	FVector HingePivotLocal = CDO->ChaosConfig.HingePivotLocal;
	FVector HingeFrameRotationPYR = CDO->ChaosConfig.HingeFrameRotationPYR;
	float HingeLimitDeg = CDO->ChaosConfig.HingeLimitDeg;
	float HingeMotorStrength = CDO->ChaosConfig.HingeMotorStrength;
	float HingeMotorDamping = CDO->ChaosConfig.HingeMotorDamping;
	float HingeMotorMaxForce = CDO->ChaosConfig.HingeMotorMaxForce;
	float HingeMotorSleepThreshold = CDO->ChaosConfig.HingeMotorSleepThreshold;

	if (const FString* Value = Properties.Find(FName(TEXT("HingePivotLocal"))))
	{
		if (!USpringMotionComponentBase::ParseVector(*Value, HingePivotLocal))
		{
			FCapabilityValidationResult R;
			R.PropertyPath = TEXT("HingePivotLocal");
			R.Message = FString::Printf(TEXT("Invalid FVector format: '%s'. Expected '(X,Y,Z)' or 'X=1 Y=2 Z=3'."), **Value);
			R.bIsWarning = true;
			Results.Add(R);
		}
	}
	if (const FString* Value = Properties.Find(FName(TEXT("HingeFrameRotationPYR"))))
	{
		if (!USpringMotionComponentBase::ParseVector(*Value, HingeFrameRotationPYR))
		{
			FCapabilityValidationResult R;
			R.PropertyPath = TEXT("HingeFrameRotationPYR");
			R.Message = FString::Printf(TEXT("Invalid FVector format: '%s'. Expected '(Pitch,Yaw,Roll)'."), **Value);
			R.bIsWarning = true;
			Results.Add(R);
		}
	}
	if (const FString* Value = Properties.Find(FName(TEXT("HingeLimitDeg"))))
	{
		HingeLimitDeg = FCString::Atof(**Value);
	}
	if (const FString* Value = Properties.Find(FName(TEXT("HingeMotorStrength"))))
	{
		HingeMotorStrength = FCString::Atof(**Value);
	}
	if (const FString* Value = Properties.Find(FName(TEXT("HingeMotorDamping"))))
	{
		HingeMotorDamping = FCString::Atof(**Value);
	}
	if (const FString* Value = Properties.Find(FName(TEXT("HingeMotorMaxForce"))))
	{
		HingeMotorMaxForce = FCString::Atof(**Value);
	}
	if (const FString* Value = Properties.Find(FName(TEXT("HingeMotorSleepThreshold"))))
	{
		HingeMotorSleepThreshold = FCString::Atof(**Value);
	}

	// Compute bWouldUseChaosMode using explicit MotionMode opt-in.
	bool bWouldUseChaosMode = false;
	if (const FString* MotionModeValue = Properties.Find(FName(TEXT("MotionMode"))))
	{
		if (MotionModeValue->Equals(TEXT("Chaos"), ESearchCase::IgnoreCase))
		{
			bWouldUseChaosMode = true;
		}
	}

	// Only validate motor params if Chaos mode will be used
	if (bWouldUseChaosMode)
	{
		if (HingeMotorStrength < 100.0f)
		{
			FCapabilityValidationResult R;
			R.PropertyPath = TEXT("HingeMotorStrength");
			R.Message = FString::Printf(TEXT("HingeMotorStrength (%.0f) is very low. Door may not overcome friction."), HingeMotorStrength);
			R.bIsWarning = true;
			Results.Add(R);
		}

		if (HingeMotorMaxForce < 1000.0f)
		{
			FCapabilityValidationResult R;
			R.PropertyPath = TEXT("HingeMotorMaxForce");
			R.Message = FString::Printf(TEXT("HingeMotorMaxForce (%.0f) is very low. Door may not overcome resistance."), HingeMotorMaxForce);
			R.bIsWarning = true;
			Results.Add(R);
		}
	}

	return Results;
}
