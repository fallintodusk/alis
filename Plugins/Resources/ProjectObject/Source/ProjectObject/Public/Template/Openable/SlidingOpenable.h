// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Template/Openable/OpenableActor.h"
#include "SlidingOpenable.generated.h"

class USpringSliderComponent;

/**
 * Sliding openable actor (sliding doors, drawers, panels, garage doors).
 *
 * Behavior: Slides linearly with spring-damper motion.
 *
 * Hierarchy: AProjectWorldActor -> AInteractableActor -> AOpenableActor -> ASlidingOpenable
 */
UCLASS(Blueprintable)
class PROJECTOBJECT_API ASlidingOpenable : public AOpenableActor
{
	GENERATED_BODY()

public:
	ASlidingOpenable();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Openable")
	TObjectPtr<USpringSliderComponent> SliderComponent;
};
