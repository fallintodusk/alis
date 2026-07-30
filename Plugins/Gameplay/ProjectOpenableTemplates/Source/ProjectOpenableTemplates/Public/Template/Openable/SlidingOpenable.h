// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Template/Openable/OpenableActor.h"
#include "SlidingOpenable.generated.h"

class USpringSliderComponent;

/**
 * Sliding openable actor (sliding doors, drawers, panels, garage doors).
 *
 * Legacy actor template for editor placement. The data-driven path uses
 * AInteractableActor + Sliding capability from JSON definitions.
 */
UCLASS(Blueprintable)
class PROJECTOPENABLETEMPLATES_API ASlidingOpenable : public AOpenableActor
{
	GENERATED_BODY()

public:
	ASlidingOpenable();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Openable")
	TObjectPtr<USpringSliderComponent> SliderComponent;
};
