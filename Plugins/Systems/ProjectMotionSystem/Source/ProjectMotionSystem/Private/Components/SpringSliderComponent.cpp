// Copyright ALIS. All Rights Reserved.

#include "Components/SpringSliderComponent.h"
#include "CapabilityValidationRegistry.h"
#include "Components/StaticMeshComponent.h"
#include "MotionDrivers/Chaos/SliderChaosDriver.h"
#include "MotionDrivers/Kinematic/SliderKinematicDriver.h"
#include "MotionDrivers/SliderMotionDriver.h"
#include "ProjectMotionSystemModule.h"

// Self-registration: component owns its validation registration
#if WITH_EDITOR
static struct FSlidingValidationAutoReg
{
	FSlidingValidationAutoReg()
	{
		FCapabilityValidationRegistry::RegisterValidation(
			FName(TEXT("Sliding")),
			&USpringSliderComponent::ValidateCapabilityProperties);
	}
} GSlidingValidationAutoReg;
#endif

struct USpringSliderComponent::FMotionDriverState
{
	TUniquePtr<ISliderMotionDriver> Driver;
};

USpringSliderComponent::USpringSliderComponent()
{
	// Base class handles tick setup
}

USpringSliderComponent::~USpringSliderComponent() = default;

void USpringSliderComponent::PostLoad()
{
	Super::PostLoad();

	const USpringSliderComponent* CDO = GetDefault<USpringSliderComponent>();
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

		if (ChaosConfig.SliderPivotLocal == CDO->ChaosConfig.SliderPivotLocal &&
			SliderPivotLocal_DEPRECATED != CDO->SliderPivotLocal_DEPRECATED)
		{
			ChaosConfig.SliderPivotLocal = SliderPivotLocal_DEPRECATED;
		}
		if (ChaosConfig.SliderAxisLocal == CDO->ChaosConfig.SliderAxisLocal &&
			SliderAxisLocal_DEPRECATED != CDO->SliderAxisLocal_DEPRECATED)
		{
			ChaosConfig.SliderAxisLocal = SliderAxisLocal_DEPRECATED;
		}
		if (ChaosConfig.SliderLimitCm == CDO->ChaosConfig.SliderLimitCm &&
			SliderLimitCm_DEPRECATED != CDO->SliderLimitCm_DEPRECATED)
		{
			ChaosConfig.SliderLimitCm = SliderLimitCm_DEPRECATED;
		}
		if (ChaosConfig.SliderMotorStrength == CDO->ChaosConfig.SliderMotorStrength &&
			SliderMotorStrength_DEPRECATED != CDO->SliderMotorStrength_DEPRECATED)
		{
			ChaosConfig.SliderMotorStrength = SliderMotorStrength_DEPRECATED;
		}
		if (ChaosConfig.SliderMotorDamping == CDO->ChaosConfig.SliderMotorDamping &&
			SliderMotorDamping_DEPRECATED != CDO->SliderMotorDamping_DEPRECATED)
		{
			ChaosConfig.SliderMotorDamping = SliderMotorDamping_DEPRECATED;
		}
		if (ChaosConfig.SliderMotorMaxForce == CDO->ChaosConfig.SliderMotorMaxForce &&
			SliderMotorMaxForce_DEPRECATED != CDO->SliderMotorMaxForce_DEPRECATED)
		{
			ChaosConfig.SliderMotorMaxForce = SliderMotorMaxForce_DEPRECATED;
		}
		if (ChaosConfig.SliderMotorSleepThreshold == CDO->ChaosConfig.SliderMotorSleepThreshold &&
			SliderMotorSleepThreshold_DEPRECATED != CDO->SliderMotorSleepThreshold_DEPRECATED)
		{
			ChaosConfig.SliderMotorSleepThreshold = SliderMotorSleepThreshold_DEPRECATED;
		}
	}
}

FPrimaryAssetId USpringSliderComponent::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(FPrimaryAssetType(TEXT("CapabilityComponent")), FName(TEXT("Sliding")));
}

void USpringSliderComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!bCapturedInitialLocation && ResolvedMesh)
	{
		InitialLocation = ResolvedMesh->GetRelativeLocation();
		bCapturedInitialLocation = true;
	}

	SelectMotionDriver();
	if (DriverState && DriverState->Driver)
	{
		DriverState->Driver->Init();
	}

	UE_LOG(LogProjectMotionSystem, Log,
		TEXT("SpringSliderComponent on %s: InitialLocation = %s, ClosedPosition = %s, OpenPosition = %s"),
		*GetOwner()->GetName(),
		*InitialLocation.ToString(),
		*ClosedPosition.ToString(),
		*OpenPosition.ToString());
}

void USpringSliderComponent::SelectMotionDriver()
{
	DriverState = MakeUnique<FMotionDriverState>();
	if (ShouldUseChaosMode())
	{
		DriverState->Driver = MakeUnique<FSliderChaosDriver>(*this);
	}
	else
	{
		DriverState->Driver = MakeUnique<FSliderKinematicDriver>(*this);
	}
}

// -------------------------------------------------------------------------
// Base Class Overrides
// -------------------------------------------------------------------------

bool USpringSliderComponent::TickMotion(float DeltaTime)
{
	if (!DriverState || !DriverState->Driver)
	{
		return false;
	}

	FMotionTickParams Params;
	Params.DeltaTime = DeltaTime;
	Params.Target = TargetProgress;
	Params.bActive = bIsAnimating || bMotorActive;

	return DriverState->Driver->Tick(Params);
}

void USpringSliderComponent::ShutdownMotionDriver()
{
	if (DriverState && DriverState->Driver)
	{
		DriverState->Driver->Shutdown();
	}
	DriverState.Reset();
}

void USpringSliderComponent::OnMotionComplete(bool bOpened)
{
	OnSlideComplete.Broadcast(bOpened);
}

void USpringSliderComponent::DoOpen()
{
	TargetProgress = MaxSlideRatio;
	if (DriverState && DriverState->Driver)
	{
		DriverState->Driver->SetTarget(TargetProgress, false);
	}
}

void USpringSliderComponent::DoClose()
{
	TargetProgress = MinSlideRatio;
	if (DriverState && DriverState->Driver)
	{
		DriverState->Driver->SetTarget(TargetProgress, false);
	}
}

// -------------------------------------------------------------------------
// Slider-Specific Methods
// -------------------------------------------------------------------------

void USpringSliderComponent::SetPositionImmediate(float Progress)
{
	const float Clamped = FMath::Clamp(Progress, MinSlideRatio, MaxSlideRatio);
	TargetProgress = Clamped;
	bDesiredOpen = Clamped > MinSlideRatio + KINDA_SMALL_NUMBER;

	if (DriverState && DriverState->Driver)
	{
		DriverState->Driver->SetTarget(Clamped, true);
		SetComponentTickEnabled(true);
	}
}

FVector USpringSliderComponent::CalculatePositionFromProgress(float Progress) const
{
	if (IsPositionMode())
	{
		// Position mode: lerp between ClosedPosition and OpenPosition
		return FMath::Lerp(ClosedPosition, OpenPosition, Progress);
	}
	else
	{
		// Direction mode: relative to InitialLocation
		return InitialLocation + SlideDirection.GetSafeNormal() * OpenDistance * Progress;
	}
}


// -------------------------------------------------------------------------
// Capability Validation
// -------------------------------------------------------------------------

TArray<FCapabilityValidationResult> USpringSliderComponent::ValidateCapabilityProperties(
	const TMap<FName, FString>& Properties)
{
	TArray<FCapabilityValidationResult> Results;
	const USpringSliderComponent* CDO = GetDefault<USpringSliderComponent>();

	// Parse property values (CDO defaults as fallback)
	FVector OpenPosition = CDO->OpenPosition;
	FVector SlideDirection = CDO->SlideDirection;
	float OpenDistance = CDO->OpenDistance;
	float MinSlideRatio = CDO->MinSlideRatio;
	float MaxSlideRatio = CDO->MaxSlideRatio;

	// Track which keys are explicitly set (for mode mixing detection)
	const bool bHasOpenPositionKey = Properties.Contains(FName(TEXT("OpenPosition")));
	const bool bHasDirectionKeys = Properties.Contains(FName(TEXT("SlideDirection"))) ||
		Properties.Contains(FName(TEXT("OpenDistance")));

	if (const FString* Value = Properties.Find(FName(TEXT("OpenPosition"))))
	{
		USpringMotionComponentBase::ParseVector(*Value, OpenPosition);
	}
	if (const FString* Value = Properties.Find(FName(TEXT("SlideDirection"))))
	{
		USpringMotionComponentBase::ParseVector(*Value, SlideDirection);
	}
	if (const FString* Value = Properties.Find(FName(TEXT("OpenDistance"))))
	{
		OpenDistance = FCString::Atof(**Value);
	}
	if (const FString* Value = Properties.Find(FName(TEXT("MinSlideRatio"))))
	{
		MinSlideRatio = FCString::Atof(**Value);
	}
	if (const FString* Value = Properties.Find(FName(TEXT("MaxSlideRatio"))))
	{
		MaxSlideRatio = FCString::Atof(**Value);
	}

	const bool bHasPositionMode = !OpenPosition.IsZero();

	// Warn if both modes explicitly set (position mode takes priority)
	if (bHasOpenPositionKey && bHasPositionMode && bHasDirectionKeys)
	{
		FCapabilityValidationResult R;
		R.PropertyPath = TEXT("OpenPosition/SlideDirection");
		R.Message = TEXT("Mixes Position mode (OpenPosition) and Direction mode (SlideDirection/OpenDistance). Position mode takes priority.");
		R.bIsWarning = true;
		Results.Add(R);
	}

	// Error if no motion possible
	if (!bHasPositionMode && SlideDirection.IsZero())
	{
		FCapabilityValidationResult R;
		R.PropertyPath = TEXT("SlideDirection");
		R.Message = TEXT("Invalid: OpenPosition is zero AND SlideDirection is zero. Mesh will not move.");
		R.bIsWarning = false;
		Results.Add(R);
	}

	// Error if ratio constraints invalid
	if (MinSlideRatio > MaxSlideRatio)
	{
		FCapabilityValidationResult R;
		R.PropertyPath = TEXT("MinSlideRatio/MaxSlideRatio");
		R.Message = FString::Printf(TEXT("MinSlideRatio (%.2f) > MaxSlideRatio (%.2f)"), MinSlideRatio, MaxSlideRatio);
		R.bIsWarning = false;
		Results.Add(R);
	}

	// Parse all Chaos properties (matching ShouldUseChaosMode logic)
	FVector SliderPivotLocal = CDO->ChaosConfig.SliderPivotLocal;
	FVector SliderAxisLocal = CDO->ChaosConfig.SliderAxisLocal;
	float SliderLimitCm = CDO->ChaosConfig.SliderLimitCm;
	float SliderMotorStrength = CDO->ChaosConfig.SliderMotorStrength;
	float SliderMotorDamping = CDO->ChaosConfig.SliderMotorDamping;
	float SliderMotorMaxForce = CDO->ChaosConfig.SliderMotorMaxForce;
	float SliderMotorSleepThreshold = CDO->ChaosConfig.SliderMotorSleepThreshold;

	if (const FString* Value = Properties.Find(FName(TEXT("SliderPivotLocal"))))
	{
		if (!USpringMotionComponentBase::ParseVector(*Value, SliderPivotLocal))
		{
			FCapabilityValidationResult R;
			R.PropertyPath = TEXT("SliderPivotLocal");
			R.Message = FString::Printf(TEXT("Invalid FVector format: '%s'. Expected '(X,Y,Z)' or 'X=1 Y=2 Z=3'."), **Value);
			R.bIsWarning = true;
			Results.Add(R);
		}
	}
	if (const FString* Value = Properties.Find(FName(TEXT("SliderAxisLocal"))))
	{
		if (!USpringMotionComponentBase::ParseVector(*Value, SliderAxisLocal))
		{
			FCapabilityValidationResult R;
			R.PropertyPath = TEXT("SliderAxisLocal");
			R.Message = FString::Printf(TEXT("Invalid FVector format: '%s'. Expected '(X,Y,Z)'."), **Value);
			R.bIsWarning = true;
			Results.Add(R);
		}
	}
	if (const FString* Value = Properties.Find(FName(TEXT("SliderLimitCm"))))
	{
		SliderLimitCm = FCString::Atof(**Value);
	}
	if (const FString* Value = Properties.Find(FName(TEXT("SliderMotorStrength"))))
	{
		SliderMotorStrength = FCString::Atof(**Value);
	}
	if (const FString* Value = Properties.Find(FName(TEXT("SliderMotorDamping"))))
	{
		SliderMotorDamping = FCString::Atof(**Value);
	}
	if (const FString* Value = Properties.Find(FName(TEXT("SliderMotorMaxForce"))))
	{
		SliderMotorMaxForce = FCString::Atof(**Value);
	}
	if (const FString* Value = Properties.Find(FName(TEXT("SliderMotorSleepThreshold"))))
	{
		SliderMotorSleepThreshold = FCString::Atof(**Value);
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
		if (SliderMotorStrength < 100.0f)
		{
			FCapabilityValidationResult R;
			R.PropertyPath = TEXT("SliderMotorStrength");
			R.Message = FString::Printf(TEXT("SliderMotorStrength (%.0f) is very low. Slider may not overcome friction."), SliderMotorStrength);
			R.bIsWarning = true;
			Results.Add(R);
		}

		if (SliderMotorMaxForce < 1000.0f)
		{
			FCapabilityValidationResult R;
			R.PropertyPath = TEXT("SliderMotorMaxForce");
			R.Message = FString::Printf(TEXT("SliderMotorMaxForce (%.0f) is very low. Slider may not overcome resistance."), SliderMotorMaxForce);
			R.bIsWarning = true;
			Results.Add(R);
		}
	}

	return Results;
}
