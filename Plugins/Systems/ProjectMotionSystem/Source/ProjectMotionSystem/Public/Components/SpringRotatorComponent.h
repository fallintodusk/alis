// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CapabilityValidationRegistry.h"
#include "Components/SpringMotionComponentBase.h"
#include "SpringRotatorComponent.generated.h"

class FRotatorKinematicDriver;
class FRotatorChaosDriver;

USTRUCT(BlueprintType)
struct FRotatorKinematicConfig
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
struct FRotatorChaosConfig
{
	GENERATED_BODY()

	/** Hinge pivot offset in mesh local space (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion|Physics")
	FVector HingePivotLocal = FVector::ZeroVector;

	/** Constraint frame rotation [Pitch, Yaw, Roll] in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion|Physics")
	FVector HingeFrameRotationPYR = FVector::ZeroVector;

	/** Angular limit in degrees (symmetric). If zero, uses OpenAngle as limit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion|Physics")
	float HingeLimitDeg = 0.0f;

	/** Motor drive strength (spring constant). Higher = faster acceleration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion|Physics")
	float HingeMotorStrength = 5000.0f;

	/** Motor drive damping. Higher = less overshoot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion|Physics")
	float HingeMotorDamping = 100.0f;

	/** Motor max torque. Lower = blockable by player. Tuned for ~10kg door. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion|Physics")
	float HingeMotorMaxForce = 15000.0f;

	/** Angle error threshold (deg) below which motor disables for physics sleep. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion|Physics")
	float HingeMotorSleepThreshold = 1.0f;
};

/**
 * Spring-damper rotation for world objects (doors, windows, lids).
 *
 * Supports two modes:
 * - Kinematic: Tick-based spring simulation (default, backward compatible)
 * - Chaos: PhysicsConstraintComponent with angular motor (mass-based blocking)
 *
 * Chaos mode enabled when MotionMode = Chaos (explicit opt-in).
 */
UCLASS(ClassGroup = (ProjectMotion), meta = (BlueprintSpawnableComponent))
class PROJECTMOTIONSYSTEM_API USpringRotatorComponent : public USpringMotionComponentBase
{
	GENERATED_BODY()

public:
	USpringRotatorComponent();
	virtual ~USpringRotatorComponent();

	// --- Stable ID for Capability Registry ---
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	// --- Rotator-Specific Config ---

	/** Target rotation angle when open (degrees). Negative for opposite direction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion", meta = (ClampMin = "-180", ClampMax = "180"))
	float OpenAngle = 90.0f;

	// --- Mode Config ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion|Kinematic")
	FRotatorKinematicConfig KinematicConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion|Physics")
	FRotatorChaosConfig ChaosConfig;

	/** Validate capability properties from JSON. Uses CDO defaults as SOT. */
	static TArray<FCapabilityValidationResult> ValidateCapabilityProperties(
		const TMap<FName, FString>& Properties);

	// --- Public API ---

	/** Set to specific angle immediately (no animation). */
	UFUNCTION(BlueprintCallable, Category = "Motion")
	void SetAngleImmediate(float Angle);

	// --- Delegates ---

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRotationComplete, bool, bOpened);

	UPROPERTY(BlueprintAssignable, Category = "Motion")
	FOnRotationComplete OnRotationComplete;

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

	friend class FRotatorKinematicDriver;
	friend class FRotatorChaosDriver;

	// Kinematic mode state
	float CurrentAngle = 0.0f;
	float TargetAngle = 0.0f;
	float AngularVelocity = 0.0f;
	FRotator InitialRotation = FRotator::ZeroRotator;
	bool bCapturedInitialRotation = false;

	UPROPERTY()
	float Stiffness_DEPRECATED = 100.0f;

	UPROPERTY()
	float Damping_DEPRECATED = 20.0f;

	UPROPERTY()
	FVector HingePivotLocal_DEPRECATED = FVector::ZeroVector;

	UPROPERTY()
	FVector HingeFrameRotationPYR_DEPRECATED = FVector::ZeroVector;

	UPROPERTY()
	float HingeLimitDeg_DEPRECATED = 0.0f;

	UPROPERTY()
	float HingeMotorStrength_DEPRECATED = 5000.0f;

	UPROPERTY()
	float HingeMotorDamping_DEPRECATED = 100.0f;

	UPROPERTY()
	float HingeMotorMaxForce_DEPRECATED = 15000.0f;

	UPROPERTY()
	float HingeMotorSleepThreshold_DEPRECATED = 1.0f;
};
