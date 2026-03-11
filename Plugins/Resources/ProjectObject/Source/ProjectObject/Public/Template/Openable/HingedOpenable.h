// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Template/Openable/OpenableActor.h"
#include "HingedOpenable.generated.h"

class USpringRotatorComponent;

/**
 * Hinged openable actor (doors, windows, hatches, cabinet doors).
 *
 * Behavior: Rotates on hinge with spring-damper motion.
 *
 * Hierarchy: AProjectWorldActor -> AInteractableActor -> AOpenableActor -> AHingedOpenable
 */
UCLASS(Blueprintable)
class PROJECTOBJECT_API AHingedOpenable : public AOpenableActor
{
	GENERATED_BODY()

public:
	AHingedOpenable();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Openable")
	TObjectPtr<USpringRotatorComponent> RotatorComponent;
};
