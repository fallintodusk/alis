#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/ProjectMotionTypes.h"
#include "MotionSourceComponent.generated.h"

/**
 * Computes FProjectMotionPose from owner actor.
 * Reads velocity, acceleration, and optionally queries IMotionDataProvider.
 * Skeleton-agnostic - works with any actor type.
 */
UCLASS(ClassGroup = (Motion), meta = (BlueprintSpawnableComponent))
class PROJECTMOTIONSYSTEM_API UMotionSourceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMotionSourceComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Get the current computed motion pose */
	UFUNCTION(BlueprintPure, Category = "Motion")
	FProjectMotionPose GetMotionPose() const { return CurrentPose; }

	/** Manually set the motion pose (for external control) */
	UFUNCTION(BlueprintCallable, Category = "Motion")
	void SetMotionPose(const FProjectMotionPose& InPose) { CurrentPose = InPose; }

protected:
	/** Current computed motion pose */
	UPROPERTY(BlueprintReadOnly, Category = "Motion")
	FProjectMotionPose CurrentPose;

	// TODO: Add gait thresholds, smoothing parameters
};
