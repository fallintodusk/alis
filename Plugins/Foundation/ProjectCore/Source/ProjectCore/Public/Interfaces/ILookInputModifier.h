// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ILookInputModifier.generated.h"

/**
 * Optional modifier for raw look-input axis values.
 *
 * The first-person character's Look() handler queries the active
 * GameMode (or another resolvable owner) for this interface and, if
 * present, runs incoming Yaw/Pitch through `ModifyLook` before adding
 * the value to controller input. Implementations might scale (cinematic
 * recording wants smoother captured camera), apply accessibility
 * curves, dead-zone, or remap.
 *
 * Default implementation returns the input unchanged, so any
 * implementer can opt out of modifying a specific axis by overriding
 * only the cases it cares about.
 *
 * Gameplay code (the character) must NOT branch on the implementer's
 * concrete type. It only knows that "some current authority can
 * optionally modify look input"; ownership of the modification policy
 * lives entirely with the implementer.
 */
UINTERFACE(MinimalAPI, BlueprintType)
class ULookInputModifier : public UInterface
{
	GENERATED_BODY()
};

class PROJECTCORE_API ILookInputModifier
{
	GENERATED_BODY()

public:
	/**
	 * Apply implementation-defined modifications to the raw look-input
	 * axis vector (X=Yaw, Y=Pitch) and return the result. Default
	 * implementation is identity; the input is unchanged.
	 */
	virtual FVector2D ModifyLook(const FVector2D& Input) const { return Input; }
};
