#pragma once

#include "CoreMinimal.h"
#include "ProjectMotionTypes.generated.h"

/**
 * Gait states for locomotion.
 */
UENUM(BlueprintType)
enum class EProjectGait : uint8
{
	Idle		UMETA(DisplayName = "Idle"),
	Walk		UMETA(DisplayName = "Walk"),
	Run			UMETA(DisplayName = "Run"),
	Sprint		UMETA(DisplayName = "Sprint")
};

/**
 * Stance states for locomotion.
 */
UENUM(BlueprintType)
enum class EProjectStance : uint8
{
	Upright		UMETA(DisplayName = "Upright"),
	Crouch		UMETA(DisplayName = "Crouch"),
	Prone		UMETA(DisplayName = "Prone")
};

/**
 * Motion pose - skeleton-agnostic locomotion state.
 * Computed by MotionSourceComponent, consumed by AnimInstance.
 *
 * IMPORTANT: This struct is pure motion data.
 * - Gait/Stance are set by the CHARACTER (gameplay layer), not computed here
 * - MotionSystem computes speed/direction/inAir from physics
 * - Character reads stamina from GAS and decides gait, then sets it here
 */
USTRUCT(BlueprintType)
struct PROJECTMOTIONSYSTEM_API FProjectMotionPose
{
	GENERATED_BODY()

	//~ Pure motion data (computed by MotionSourceComponent)

	/** Current movement speed (units/sec) */
	UPROPERTY(BlueprintReadWrite, Category = "Motion|Computed")
	float Speed = 0.0f;

	/** Movement direction (normalized, world space) */
	UPROPERTY(BlueprintReadWrite, Category = "Motion|Computed")
	FVector Direction = FVector::ZeroVector;

	/** Is actor in air (jumping/falling) */
	UPROPERTY(BlueprintReadWrite, Category = "Motion|Computed")
	bool bInAir = false;

	/** Lean amount (-1 to 1, for turns) */
	UPROPERTY(BlueprintReadWrite, Category = "Motion|Computed")
	float Lean = 0.0f;

	//~ Gameplay-driven values (set by character, not computed here)

	/** Current gait state (set by character based on input + stamina) */
	UPROPERTY(BlueprintReadWrite, Category = "Motion|Gameplay")
	EProjectGait Gait = EProjectGait::Idle;

	/** Current stance state (set by character based on input) */
	UPROPERTY(BlueprintReadWrite, Category = "Motion|Gameplay")
	EProjectStance Stance = EProjectStance::Upright;

	/** Stride scale (for speed matching, optional) */
	UPROPERTY(BlueprintReadWrite, Category = "Motion|Gameplay")
	float StrideScale = 1.0f;
};
