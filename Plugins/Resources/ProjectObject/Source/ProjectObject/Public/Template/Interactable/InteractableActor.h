// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProjectWorldActor.h"
#include "IDefinitionApplicable.h"
#include "Interfaces/IInteractableTarget.h"
#include "InteractableActor.generated.h"

/**
 * Base class for world actors that respond to player interaction.
 *
 * SOLID: Runtime container + definition application logic.
 * - Holds components and dispatches interaction
 * - Knows HOW to apply ObjectDefinition (ApplyDefinition implementation)
 * - Subsystem knows WHEN to update (definition regenerated, actor loaded)
 *
 * Pattern: On interaction, iterates cached components by priority (descending).
 * First component returning false blocks further processing.
 *
 * ## Definition Application (via IDefinitionApplicable interface)
 *
 * ApplyDefinition() updates actor from UObjectDefinition:
 * - Updates mesh assets, transforms, materials
 * - Updates capability component properties
 * - Uses DefMeshId tags for robust mesh matching (survives asset renames)
 *
 * Subsystem orchestrates WHEN to call this (no duplication with other definition logic).
 *
 * Example hierarchy:
 * - AInteractableActor (this - interaction dispatch + ObjectDefinition handling)
 *   - AHingedOpenable (adds Mesh + RotatorComponent + LockComponent)
 *   - ASlidingOpenable (adds Mesh + SliderComponent + LockComponent)
 *   - ALever (adds Mesh + RotatorComponent + ActivatableComponent)
 */
UCLASS(Blueprintable)
class PROJECTOBJECT_API AInteractableActor : public AProjectWorldActor, public IInteractableTargetInterface, public IDefinitionApplicable
{
	GENERATED_BODY()

public:
	AInteractableActor();

	// --- IInteractableTargetInterface ---
	virtual bool OnInteract_Implementation(AActor* InteractInstigator, UPrimitiveComponent* HitComponent) override;
	virtual FInteractionFocusInfo GetFocusInfo_Implementation(UPrimitiveComponent* HitComponent) const override;
	virtual FInteractionExecutionSpec GetInteractionExecutionSpec_Implementation(AActor* InteractInstigator, UPrimitiveComponent* HitComponent) const override;

	// --- IDefinitionApplicable ---
	virtual bool ApplyDefinition_Implementation(UPrimaryDataAsset* Definition) override;

	/** Rebuild cached component list. Call after adding/removing interactable components at runtime. */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void RefreshInteractableComponents();

protected:
	virtual void BeginPlay() override;

private:
	/** Cached and sorted interactable components. */
	UPROPERTY()
	TArray<TObjectPtr<UActorComponent>> CachedInteractableComponents;

	void CacheInteractableComponents();
};
