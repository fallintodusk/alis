// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CapabilityValidationRegistry.h"
#include "Components/SpringMotionComponentBase.h"
#include "SpringSliderComponent.generated.h"

class FSliderKinematicDriver;
class FSliderChaosDriver;

USTRUCT(BlueprintType)
struct FSliderKinematicConfig
{
	GENERATED_BODY()

	/** Spring stiffness for kinematic mode (higher = faster motion). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion|Kinematic", meta = (ClampMin = "1", ClampMax = "500"))
	float Stiffness = 100.0f;

	/** Damping ratio for kinematic mode (higher = less oscillation). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion|Kinematic", meta = (ClampMin = "1", ClampMax = "100"))
	float Damping = 20.0f;
};

USTRUCT(BlueprintType)
struct FSliderChaosConfig
{
	GENERATED_BODY()

	/** Slider pivot offset in mesh local space (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion|Physics")
	FVector SliderPivotLocal = FVector::ZeroVector;

	/** Constraint axis in local space (normalized). Default: X-axis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion|Physics")
	FVector SliderAxisLocal = FVector::ForwardVector;

	/** Linear limit in cm (symmetric). If zero, uses OpenDistance as limit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion|Physics")
	float SliderLimitCm = 0.0f;

	/** Motor drive strength (spring constant). Higher = faster acceleration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion|Physics")
	float SliderMotorStrength = 3000.0f;

	/** Motor drive damping. Higher = less overshoot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion|Physics")
	float SliderMotorDamping = 80.0f;

	/** Motor max force. Lower = blockable by player. Tuned for ~3kg drawer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion|Physics")
	float SliderMotorMaxForce = 10000.0f;

	/** Position error threshold (cm) below which motor disables for physics sleep. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion|Physics")
	float SliderMotorSleepThreshold = 0.5f;
};

/**
 * Spring-damper linear motion for world objects (sliding doors, drawers, panels).
 *
 * Two slide modes (mutually exclusive):
 * 1. Position-based: ClosedPosition/OpenPosition as absolute targets
 * 2. Direction-based: SlideDirection + OpenDistance for relative motion
 *
 * Supports two motion modes:
 * - Kinematic: Tick-based spring simulation (default, backward compatible)
 * - Chaos: PhysicsConstraintComponent with linear motor (mass-based blocking)
 *
 * Chaos mode enabled when MotionMode = Chaos (explicit opt-in).
 */
UCLASS(ClassGroup = (ProjectMotion), meta = (BlueprintSpawnableComponent))
class PROJECTMOTIONSYSTEM_API USpringSliderComponent : public USpringMotionComponentBase
{
	GENERATED_BODY()

public:
	USpringSliderComponent();
	virtual ~USpringSliderComponent();

	// --- Stable ID for Capability Registry ---
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	// --- Designer Config: Position Mode (absolute targets) ---

	/** Target position when fully closed (absolute, relative to actor root). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion|Position Mode")
	FVector ClosedPosition = FVector::ZeroVector;

	/** Target position when fully open (absolute, relative to actor root). If non-zero, uses position mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion|Position Mode")
	FVector OpenPosition = FVector::ZeroVector;

	// --- Designer Config: Direction Mode (direction + distance) ---

	/** Slide direction in local space (normalized). Used if OpenPosition is zero. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion|Direction Mode")
	FVector SlideDirection = FVector::RightVector;

	/** Distance to slide when open (cm). Used if OpenPosition is zero. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion|Direction Mode")
	float OpenDistance = 100.0f;

	// --- Designer Config: Constraints ---

	/** Minimum slide ratio (0.0-1.0). Prevents closing past this point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion|Constraints", meta = (ClampMin = "0", ClampMax = "1"))
	float MinSlideRatio = 0.0f;

	/** Maximum slide ratio (0.0-1.0). Prevents opening past this point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion|Constraints", meta = (ClampMin = "0", ClampMax = "1"))
	float MaxSlideRatio = 1.0f;

	// --- Mode Config ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion|Kinematic")
	FSliderKinematicConfig KinematicConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion|Physics")
	FSliderChaosConfig ChaosConfig;

	/** Validate capability properties from JSON. Uses CDO defaults as SOT. */
	static TArray<FCapabilityValidationResult> ValidateCapabilityProperties(
		const TMap<FName, FString>& Properties);

	// --- Public API ---

	/** Set to specific position immediately (no animation). */
	UFUNCTION(BlueprintCallable, Category = "Motion")
	void SetPositionImmediate(float Progress);

	// --- Delegates ---

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSlideComplete, bool, bOpened);

	UPROPERTY(BlueprintAssignable, Category = "Motion")
	FOnSlideComplete OnSlideComplete;

protected:
	virtual void PostLoad() override;
	virtual void BeginPlay() override;

	// --- Base Class Overrides ---
	virtual bool TickMotion(float DeltaTime) override;
	virtual void ShutdownMotionDriver() override;
	virtual void OnMotionComplete(bool bOpened) override;
	virtual void DoOpen() override;
	virtual void DoClose() override;

private:
	void SelectMotionDriver();

	struct FMotionDriverState;
	TUniquePtr<FMotionDriverState> DriverState;

	friend class FSliderKinematicDriver;
	friend class FSliderChaosDriver;

	// Kinematic mode state (progress-based: 0.0 = closed, 1.0 = open)
	float CurrentProgress = 0.0f;
	float TargetProgress = 0.0f;
	float ProgressVelocity = 0.0f;
	FVector InitialLocation = FVector::ZeroVector;
	bool bCapturedInitialLocation = false;

	UPROPERTY()
	float Stiffness_DEPRECATED = 100.0f;

	UPROPERTY()
	float Damping_DEPRECATED = 20.0f;

	UPROPERTY()
	FVector SliderPivotLocal_DEPRECATED = FVector::ZeroVector;

	UPROPERTY()
	FVector SliderAxisLocal_DEPRECATED = FVector::ForwardVector;

	UPROPERTY()
	float SliderLimitCm_DEPRECATED = 0.0f;

	UPROPERTY()
	float SliderMotorStrength_DEPRECATED = 3000.0f;

	UPROPERTY()
	float SliderMotorDamping_DEPRECATED = 80.0f;

	UPROPERTY()
	float SliderMotorMaxForce_DEPRECATED = 10000.0f;

	UPROPERTY()
	float SliderMotorSleepThreshold_DEPRECATED = 0.5f;

	// --- Methods ---

	bool IsPositionMode() const { return !OpenPosition.IsZero(); }
	FVector CalculatePositionFromProgress(float Progress) const;
};
