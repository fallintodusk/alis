#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "IMotionDataProvider.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UMotionDataProvider : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for actors that provide pure motion data.
 * Characters implement this to feed data to the motion system.
 *
 * IMPORTANT: This interface is skeleton-agnostic and gameplay-agnostic.
 * - NO stamina, stance, or gameplay-specific concepts
 * - Only pure motion inputs (velocity, in-air state)
 * - Gameplay logic (stamina -> gait downgrade) stays in ProjectCharacter
 */
class PROJECTMOTIONSYSTEM_API IMotionDataProvider
{
	GENERATED_BODY()

public:
	/** Get velocity in world space (units/sec) */
	virtual FVector GetVelocityWS() const = 0;

	/** Is actor in air (optional - return false if not applicable) */
	virtual bool IsInAir() const { return false; }

	/** Get desired gait as normalized scalar (0-1, optional hint) */
	virtual float GetDesiredGait01() const { return 1.0f; }
};
