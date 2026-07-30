// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Template/Interactable/InteractableActor.h"
#include "OpenableActor.generated.h"

class UStaticMeshComponent;
class ULockableComponent;

/**
 * Base class for openable world actors (doors, windows, drawers, hatches).
 *
 * Legacy actor template for editor placement. The data-driven path uses
 * AInteractableActor + capabilities from JSON definitions.
 *
 * Provides common infrastructure:
 * - Mesh: Visual representation (designer sets mesh directly on component)
 * - LockComponent: Optional key/lock (empty LockTag = unlocked)
 *
 * Derived classes add motion components:
 * - AHingedOpenable: SpringRotatorComponent (rotation)
 * - ASlidingOpenable: SpringSliderComponent (linear)
 */
UCLASS(Abstract, Blueprintable)
class PROJECTOPENABLETEMPLATES_API AOpenableActor : public AInteractableActor
{
	GENERATED_BODY()

public:
	AOpenableActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Openable")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Openable")
	TObjectPtr<ULockableComponent> LockComponent;
};
