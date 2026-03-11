// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "IInteractableTarget.generated.h"

/**
 * Data for interaction confirmation prompt.
 * If IsValid() returns false, interaction proceeds immediately (no prompt).
 */
USTRUCT(BlueprintType)
struct PROJECTCORE_API FInteractionPrompt
{
	GENERATED_BODY()

	/** Prompt title shown to player. Empty = no prompt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText Title;

	/** Options for player to choose. Default: "Yes", "No". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FText> Options;

	/** Index of the "confirm" option (default 0 = first option). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ConfirmIndex = 0;

	/** If true, skip prompt entirely and interact immediately. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bSkipPrompt = true;

	bool IsValid() const { return !bSkipPrompt && !Title.IsEmpty(); }
};

/**
 * Focus info returned by actor when player looks at a component.
 * Allows actor to control highlighting/labeling without exposing capability internals.
 */
USTRUCT(BlueprintType)
struct PROJECTCORE_API FInteractionFocusInfo
{
	GENERATED_BODY()

	/** Label for HUD (e.g., "Open", "Close"). Empty = not interactable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText Label;

	/** Mesh to highlight. Null = use hit component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UPrimitiveComponent> HighlightMesh = nullptr;

	/** Whether this hit component can be interacted with. */
	bool IsValid() const { return !Label.IsEmpty(); }
};

/**
 * IInteractableTarget
 *
 * Interface for actors that can be interacted with.
 * Interaction system calls this on target actors.
 */
UINTERFACE(MinimalAPI, BlueprintType)
class UInteractableTargetInterface : public UInterface
{
	GENERATED_BODY()
};

class PROJECTCORE_API IInteractableTargetInterface
{
	GENERATED_BODY()

public:
	/**
	 * Called when this actor is interacted with.
	 * Implementation should forward to components via IInteractableComponentTarget.
	 *
	 * @param Instigator The actor performing the interaction (usually player pawn)
	 * @param HitComponent Optional: the specific mesh/component that was hit.
	 *        If provided, only the capability targeting this mesh should respond.
	 *        If nullptr, all capabilities are triggered (legacy behavior).
	 * @return true if interaction was handled
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool OnInteract(AActor* Instigator, UPrimitiveComponent* HitComponent);

	/**
	 * Get focus info for a hit component (label + highlight mesh).
	 * Allows interaction system to query what to display without knowing capability internals.
	 *
	 * @param HitComponent The component player is looking at
	 * @return Focus info (label, mesh to highlight). Invalid if not interactable.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	FInteractionFocusInfo GetFocusInfo(UPrimitiveComponent* HitComponent) const;
};

/**
 * IInteractableComponentTarget
 *
 * Interface for components that respond to interaction on their owner actor.
 * Components implement this to receive interaction events with priority ordering.
 *
 * Usage pattern:
 * - Actor implements IInteractableTargetInterface
 * - Actor's OnInteract() collects all components implementing this interface
 * - Sorts by priority (descending) and calls each in order
 * - Stops if any component returns false (blocking)
 *
 * Example priorities:
 * - ULockableComponent: 100 (check access first)
 * - USpringRotatorComponent: 0 (toggle after access check)
 */
UINTERFACE(MinimalAPI, BlueprintType)
class UInteractableComponentTargetInterface : public UInterface
{
	GENERATED_BODY()
};

class PROJECTCORE_API IInteractableComponentTargetInterface
{
	GENERATED_BODY()

public:
	/**
	 * Get interaction priority. Higher = executes first.
	 * Use 100+ for access control, 0-50 for actions.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	int32 GetInteractPriority() const;

	/**
	 * Handle interaction on this component.
	 *
	 * @param Instigator The actor performing the interaction
	 * @return true to continue to next component, false to block further interaction
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool OnComponentInteract(AActor* Instigator);

	/**
	 * Get interaction prompt for this component.
	 * Called before OnComponentInteract to check if confirmation UI is needed.
	 *
	 * Default: returns invalid prompt (bSkipPrompt=true), meaning immediate interaction.
	 * Override to show confirmation like "Use Key?" [Yes/No].
	 *
	 * @param Instigator The actor performing the interaction
	 * @return Prompt data. If IsValid() is false, no UI shown.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	FInteractionPrompt GetInteractionPrompt(AActor* Instigator) const;

	/**
	 * Get interaction label for HUD display (e.g., "Open", "Close", "Pickup").
	 * Shown as "[E] {Label}" when player focuses this component's target mesh.
	 *
	 * Default: returns "Interact".
	 * Override to provide context-specific labels:
	 * - SpringRotator/Slider: "Open" / "Close" (state-based)
	 * - Lockable (locked): "Unlock"
	 * - Pickup: "Pickup"
	 *
	 * @return Label text for HUD. Should never be empty.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	FText GetInteractionLabel() const;

	/**
	 * Optional explicit target mesh for this capability.
	 *
	 * Return non-null for mesh-scoped interaction (focus and execute resolve against this mesh).
	 * Return null for actor-scoped interaction.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	UPrimitiveComponent* GetInteractTargetMesh() const;

	/**
	 * Optional setter for mesh-scoped interaction.
	 *
	 * Object spawn/apply paths use this to bind capability instances to a mesh.
	 * Components that are actor-scoped can keep the default no-op behavior.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void SetInteractTargetMesh(UPrimitiveComponent* TargetMesh);
};
