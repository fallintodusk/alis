// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "IInteractionService.generated.h"

/**
 * Delegate broadcast when an interaction occurs
 *
 * @param Target The actor that was interacted with
 * @param Instigator The actor that performed the interaction (usually player pawn)
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FInteractionDelegate, AActor*, AActor*);

/**
 * Delegate broadcast when interaction focus changes (global - all players).
 * Used by HUD to show "[E] Open" / "[E] Close" prompts.
 *
 * @param Instigator The pawn whose focus changed (player pawn)
 * @param FocusedActor The actor being focused (nullptr if no focus)
 * @param FocusedComponent The specific mesh part being focused (nullptr if no focus)
 * @param Label The interaction label ("Open", "Close", "Interact", etc.)
 */
DECLARE_MULTICAST_DELEGATE_FourParams(FInteractionFocusDelegate, APawn*, AActor*, UPrimitiveComponent*, FText);

/**
 * Delegate for filtered (per-player) focus events.
 * Subscriber only receives events for their registered pawn.
 * Cleaner API - no need to filter in subscriber code.
 *
 * @param FocusedComponent The component being focused (nullptr = lost focus)
 * @param Label The interaction label
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FInteractionFocusFilteredDelegate, UPrimitiveComponent*, FText);

/**
 * HUD-facing prompt state for the current focused interaction.
 * Owned by ProjectInteraction, consumed by ProjectHUD.
 */
USTRUCT(BlueprintType)
struct PROJECTCORE_API FInteractionPromptState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bHasFocus = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText Label;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bRequiresHold = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsInProgress = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Progress = 0.0f;

	bool IsVisible() const
	{
		return bHasFocus && !Label.IsEmpty();
	}
};

DECLARE_MULTICAST_DELEGATE_OneParam(FInteractionPromptStateFilteredDelegate, const FInteractionPromptState&);

/**
 * IInteractionComponent
 *
 * Interface for interaction detection components.
 * Character queries this without depending on ProjectInteraction directly.
 *
 * Usage (in ProjectCharacter):
 *   if (auto* Comp = FindComponentByClass<UInteractionComponent>())
 *   {
 *       if (Comp->Implements<UInteractionComponentInterface>())
 *       {
 *           IInteractionComponentInterface::Execute_TryInteract(Comp);
 *       }
 *   }
 *
 * Or simpler via TInlineComponentArray + Cast.
 */
UINTERFACE(MinimalAPI, BlueprintType)
class UInteractionComponentInterface : public UInterface
{
	GENERATED_BODY()
};

class PROJECTCORE_API IInteractionComponentInterface
{
	GENERATED_BODY()

public:
	/** Attempt interaction with focused actor */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool TryInteract();

	/** Begin the interact input lifecycle (press/hold path). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool BeginInteractInput();

	/** End the interact input lifecycle (release/cancel path). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void EndInteractInput();

	/** Get currently focused actor */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	AActor* GetFocusedActor() const;

	/** Check if has focused actor */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool HasFocusedActor() const;

	/** Get currently focused component (for HUD initial state) */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	UPrimitiveComponent* GetFocusedComponent() const;

	/** Get current interaction label (for HUD initial state) */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	FText GetFocusedLabel() const;

	/** Get current HUD-facing prompt state (for initial sync and tests). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	FInteractionPromptState GetInteractionPromptState() const;
};

/**
 * IInteractionService
 *
 * Minimal service interface for interaction event subscription.
 * Features subscribe to OnInteraction to handle relevant events.
 *
 * Implementation lives in ProjectInteraction plugin.
 * Register via FProjectServiceLocator.
 *
 * Usage:
 *   if (auto Service = FProjectServiceLocator::Resolve<IInteractionService>())
 *   {
 *       Service->OnInteraction().AddUObject(this, &UMyFeature::HandleInteraction);
 *   }
 */
class PROJECTCORE_API IInteractionService : public TSharedFromThis<IInteractionService>
{
public:
	virtual ~IInteractionService() = default;

	/** ServiceLocator key for registration/resolution */
	static FName ServiceKey() { return FName(TEXT("IInteractionService")); }

	/** Subscribe to interaction events */
	virtual FInteractionDelegate& OnInteraction() = 0;

	/** Subscribe to focus change events (for HUD prompts) - receives ALL players' events */
	virtual FInteractionFocusDelegate& OnFocusChanged() = 0;

	/**
	 * Subscribe to focus events for a specific pawn only.
	 * Service filters internally - subscriber receives only their pawn's events.
	 * Preferred API for UI - cleaner than filtering in ViewModel.
	 *
	 * @param Pawn The pawn to subscribe for (usually local player's pawn)
	 * @return Delegate to bind to. Service manages routing.
	 */
	virtual FInteractionFocusFilteredDelegate& OnFocusChangedForPawn(APawn* Pawn) = 0;

	/** Subscribe to HUD-facing prompt state for a specific pawn. */
	virtual FInteractionPromptStateFilteredDelegate& OnPromptStateChangedForPawn(APawn* Pawn) = 0;

	/**
	 * Unsubscribe from per-pawn focus events.
	 * Call when ViewModel shuts down to clean up.
	 *
	 * @param Pawn The pawn to unsubscribe
	 */
	virtual void UnsubscribeForPawn(APawn* Pawn) = 0;

	/**
	 * Broadcast focus change. Called by InteractionComponent.
	 * Routes to global OnFocusChanged() and per-pawn delegates.
	 */
	virtual void BroadcastFocusChanged(APawn* Instigator, AActor* FocusedActor, UPrimitiveComponent* FocusedComponent, FText Label) = 0;

	/** Broadcast prompt state. Called by InteractionComponent. */
	virtual void BroadcastPromptState(APawn* Instigator, const FInteractionPromptState& State) = 0;
};
