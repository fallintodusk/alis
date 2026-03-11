// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Template/Interactable/InteractableActor.h"
#include "OpenableActor.generated.h"

class UStaticMeshComponent;
class ULockableComponent;

/**
 * Base class for openable world actors (doors, windows, drawers, hatches).
 *
 * Provides common infrastructure:
 * - Mesh: Visual representation (designer sets mesh directly on component)
 * - LockComponent: Optional key/lock (empty LockTag = unlocked)
 *
 * Derived classes add motion components:
 * - AHingedOpenable: SpringRotatorComponent (rotation)
 * - ASlidingOpenable: SpringSliderComponent (linear)
 *
 * Hierarchy: AProjectWorldActor -> AInteractableActor -> AOpenableActor -> concrete
 */
UCLASS(Abstract, Blueprintable)
class PROJECTOBJECT_API AOpenableActor : public AInteractableActor
{
	GENERATED_BODY()

public:
	AOpenableActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Openable")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Openable")
	TObjectPtr<ULockableComponent> LockComponent;
};
