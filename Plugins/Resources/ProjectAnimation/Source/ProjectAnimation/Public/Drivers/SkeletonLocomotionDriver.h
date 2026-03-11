#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Core/ProjectMotionTypes.h"
#include "SkeletonLocomotionDriver.generated.h"

class USkeletalMeshComponent;

UINTERFACE(MinimalAPI, Blueprintable)
class USkeletonLocomotionDriver : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for skeleton-specific locomotion drivers.
 * Implementations know bone names, curve names, and how to apply motion pose.
 *
 * Each skeleton family (MaleHero, FemaleHero, NPCs) has its own driver.
 */
class PROJECTANIMATION_API ISkeletonLocomotionDriver
{
	GENERATED_BODY()

public:
	/**
	 * Apply motion pose to skeletal mesh.
	 * Implementation handles skeleton-specific bone/curve mapping.
	 */
	virtual void ApplyMotionPose(USkeletalMeshComponent* Mesh, const FProjectMotionPose& Pose) = 0;

	/**
	 * Get the skeleton family name this driver supports.
	 */
	virtual FName GetSkeletonFamilyName() const = 0;
};
