#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Core/ProjectMotionTypes.h"
#include "ProjectAnimInstanceBase.generated.h"

/**
 * Base AnimInstance with motion pose variables.
 * Skeleton-specific subclasses extend this.
 *
 * Usage:
 * - Character sets motion pose via SetMotionPose()
 * - AnimBlueprint reads exposed variables for blending
 */
UCLASS()
class PROJECTANIMATION_API UProjectAnimInstanceBase : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** Set the current motion pose (called by character each tick) */
	UFUNCTION(BlueprintCallable, Category = "Animation|Motion")
	void SetMotionPose(const FProjectMotionPose& InPose);

	/** Get the current motion pose */
	UFUNCTION(BlueprintPure, Category = "Animation|Motion")
	FProjectMotionPose GetMotionPose() const { return MotionPose; }

protected:
	//~ Motion pose variables exposed to AnimBlueprint

	/** Current motion pose */
	UPROPERTY(BlueprintReadOnly, Category = "Animation|Motion")
	FProjectMotionPose MotionPose;

	//~ Convenience accessors for common AnimBP use

	/** Movement speed (units/sec) */
	UPROPERTY(BlueprintReadOnly, Category = "Animation|Motion")
	float Speed = 0.0f;

	/** Is in air */
	UPROPERTY(BlueprintReadOnly, Category = "Animation|Motion")
	bool bIsInAir = false;

	/** Current gait (as int for AnimBP state machine) */
	UPROPERTY(BlueprintReadOnly, Category = "Animation|Motion")
	int32 GaitIndex = 0;

	/** Current stance (as int for AnimBP state machine) */
	UPROPERTY(BlueprintReadOnly, Category = "Animation|Motion")
	int32 StanceIndex = 0;
};
