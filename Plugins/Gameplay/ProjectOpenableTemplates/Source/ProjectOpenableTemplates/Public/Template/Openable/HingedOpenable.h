// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Template/Openable/OpenableActor.h"
#include "HingedOpenable.generated.h"

class USpringRotatorComponent;

/**
 * Hinged openable actor (doors, windows, hatches, cabinet doors).
 *
 * Legacy actor template for editor placement. The data-driven path uses
 * AInteractableActor + Hinged capability from JSON definitions.
 */
UCLASS(Blueprintable)
class PROJECTOPENABLETEMPLATES_API AHingedOpenable : public AOpenableActor
{
	GENERATED_BODY()

public:
	AHingedOpenable();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Openable")
	TObjectPtr<USpringRotatorComponent> RotatorComponent;
};
