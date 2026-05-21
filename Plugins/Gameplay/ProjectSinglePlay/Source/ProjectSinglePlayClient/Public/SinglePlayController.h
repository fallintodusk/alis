#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "SinglePlayController.generated.h"

class UInputMappingContext;
class UInputAction;
class UVitalsViewModel;
class UMindJournalViewModel;
class UAbilitySystemComponent;
struct FInputActionValue;

// Lightweight observability signals. Gameplay code broadcasts these after the
// real state change runs; the controller itself takes no opinion on who is
// listening. External subscribers (analytics, recording tools, replay,
// tutorial scrubber, etc.) attach via these signals -- gameplay code stays
// free of subscriber-specific branches.
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSinglePlayPanelVisibilityChanged, FName /* PanelName */, bool /* bVisible */);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSinglePlayInteractionTriggered, AActor* /* TargetActor */, UActorComponent* /* RespondingComponent */);

/**
 * PlayerController for single-player gameplay.
 *
 * Handles input mode switching based on possessed pawn:
 * - First-person character: game-only mode with locked mouse
 * - Spectator/menu: UI mode with cursor
 */
UCLASS()
class PROJECTSINGLEPLAYCLIENT_API ASinglePlayController : public APlayerController
{
	GENERATED_BODY()

public:
	ASinglePlayController();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void SetupInputComponent() override;
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;

	// Switch to first-person game input (locked mouse, no cursor)
	void SetFirstPersonInputMode();

	// Switch to UI/menu input (cursor visible)
	void SetUIInputMode();

private:
	// Initialize ProjectUI layer host for local player.
	void InitializeVitalsUI(APawn* InPawn);

	// Initialize inventory UI bindings.
	void InitializeInventoryUI(APawn* InPawn);

	// Bind to VitalsViewModel from ProjectUI factory (retry if needed).
	void TryBindVitalsViewModel();
	void SyncVitalsViewModelSourceFromPawn();

	// Bind to InventoryViewModel from ProjectUI factory (retry if needed).
	void TryBindInventoryViewModel();

	bool EnsureInventoryViewModelReady();

	void SyncInventoryViewModelSourceFromPawn();

	// Bind to MindJournalViewModel from ProjectUI factory.
	void TryBindMindJournalViewModel();

	// React to ViewModel property changes (panel visibility).
	UFUNCTION()
	void HandleVitalsViewModelPropertyChanged(FName PropertyName);

	// React to inventory ViewModel property changes (panel visibility).
	UFUNCTION()
	void HandleInventoryViewModelPropertyChanged(FName PropertyName);

	// Create enhanced input assets for UI actions.
	void CreateUIInputAssets();

	// Toggle the expanded vitals panel.
	void HandleToggleVitalsAction(const FInputActionValue& Value);

	// Toggle the inventory panel.
	void HandleToggleInventoryAction(const FInputActionValue& Value);

	// Toggle the mind journal panel.
	void HandleToggleMindJournalAction(const FInputActionValue& Value);

public:
	// -------------------------------------------------------------------------
	// UI Panels - programmatic API
	// -------------------------------------------------------------------------
	// Generic gameplay capability for opening/closing the player's UI panels.
	// Same code path the H/I/J Enhanced Input handlers use, exposed publicly
	// for callers that need to drive panels without simulating input:
	//   - Dialogue runtime (NPC says "check your journal")
	//   - Tutorial / onboarding scripts
	//   - Save/load resume (restore panel state)
	//   - Integration tests (avoid synthesising input events)
	//   - External observability subscribers (see OnPanelVisibilityChanged below)
	//
	// Two paired methods per panel:
	//   - Toggle*  - flips state. Right for input handlers (keypress = flip).
	//   - SetPanel_*Visible(bool) - idempotent target state. Right for
	//                programmatic callers that need a specific end state
	//                (safe to call twice; second call is a no-op).
	//
	// All methods early-out on remote controllers via IsLocalController().
	// -------------------------------------------------------------------------

	/** Toggle the vitals panel (flips current state). Use SetPanel_VitalsVisible for Sequencer. */
	UFUNCTION(BlueprintCallable, Category = "Alis|UI Panels")
	void TogglePanel_Vitals();

	/** Toggle the inventory panel (flips current state). Use SetPanel_InventoryVisible for Sequencer. */
	UFUNCTION(BlueprintCallable, Category = "Alis|UI Panels")
	void TogglePanel_Inventory();

	/** Toggle the mind journal panel (flips current state). Use SetPanel_MindJournalVisible for Sequencer. */
	UFUNCTION(BlueprintCallable, Category = "Alis|UI Panels")
	void TogglePanel_MindJournal();

	/** Set the vitals panel to a specific visibility. No-op if already in target state.
	 *  CallInEditor lets a Sequencer Event Track fire this during PIE-in-Sequencer
	 *  preview (production gating is MRQ-render-only in game world, but the
	 *  metadata is free and aids developer iteration). */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Alis|UI Panels")
	void SetPanel_VitalsVisible(bool bVisible);

	/** Set the inventory panel to a specific visibility. No-op if already in target state. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Alis|UI Panels")
	void SetPanel_InventoryVisible(bool bVisible);

	/** Set the mind journal panel to a specific visibility. No-op if already in target state. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Alis|UI Panels")
	void SetPanel_MindJournalVisible(bool bVisible);

	// -------------------------------------------------------------------------
	// Interaction - programmatic API
	// -------------------------------------------------------------------------
	// Same code path the E-key handler uses. Re-runs the focus trace through
	// the pawn's UInteractionComponent and fires the interaction on whatever
	// actor the recorded camera is looking at. Use from:
	//   - Tutorial / scripted gameplay (NPC's hand reaches for a doorknob)
	//   - Integration tests
	// Inventory panel close shortcut preserved (same as input behaviour).
	// -------------------------------------------------------------------------

	/** Fire the interaction on whatever actor is currently focused by the pawn's interaction component. */
	UFUNCTION(BlueprintCallable, Category = "Alis|Interaction")
	void TriggerFocusedInteraction();

	/**
	 * Fire the interaction on a specific target actor, bypassing the focus
	 * trace. Use when the caller has already resolved the target (e.g.
	 * scripted tutorial that walks the player up to a specific door and
	 * triggers it, integration test driving a deterministic interaction).
	 * If TargetActor is null or the pawn has no interaction component,
	 * this is a no-op.
	 */
	UFUNCTION(BlueprintCallable, Category = "Alis|Interaction")
	void TriggerInteractionWithActor(AActor* TargetActor);

	/**
	 * Broadcast after a panel's visibility flips (Toggle) or is set (SetVisible).
	 * PanelName is "Vitals" / "Inventory" / "MindJournal"; bVisible is the
	 * actual end state, not the input. Subscribe externally; the controller
	 * itself does nothing with this signal.
	 */
	FOnSinglePlayPanelVisibilityChanged OnPanelVisibilityChanged;

	/**
	 * Broadcast after `TriggerFocusedInteraction()` resolves a focused actor
	 * and fires the interaction code path. TargetActor may be null if the
	 * focus trace ran but landed on nothing.
	 */
	FOnSinglePlayInteractionTriggered OnInteractionTriggered;

private:

	// Attempt interaction with focused actor.
	void HandleInteractAction(const FInputActionValue& Value);

	// Release/cancel interaction input.
	void HandleInteractReleasedAction(const FInputActionValue& Value);

	// Swap items between hand slots.
	void HandleSwapHandsAction(const FInputActionValue& Value);

	// Notify mind runtime about local input activity (used to re-arm idle scan).
	void NotifyMindInputActivity();

	/** ViewModel shared by mini HUD and panel. */
	UPROPERTY()
	TObjectPtr<UVitalsViewModel> VitalsViewModel;

	/** Inventory ViewModel object (client-only, stored as UObject to avoid hard dependency). */
	UPROPERTY()
	TObjectPtr<UObject> InventoryViewModel;

	/** Mind journal view model (client-only). */
	UPROPERTY()
	TObjectPtr<UMindJournalViewModel> MindJournalViewModel;

	/** Enhanced Input context for UI actions. */
	UPROPERTY()
	TObjectPtr<UInputMappingContext> UIInputMappingContext;

	/** Toggle action for vitals panel. */
	UPROPERTY()
	TObjectPtr<UInputAction> ToggleVitalsAction;

	/** Toggle action for inventory panel. */
	UPROPERTY()
	TObjectPtr<UInputAction> ToggleInventoryAction;

	/** Toggle action for mind journal panel. */
	UPROPERTY()
	TObjectPtr<UInputAction> ToggleMindJournalAction;

	/** Interact action (E key). */
	UPROPERTY()
	TObjectPtr<UInputAction> InteractAction;

	/** Swap hands action (X key). */
	UPROPERTY()
	TObjectPtr<UInputAction> SwapHandsAction;

	FTimerHandle VitalsUIRetryHandle;
	int32 VitalsUIBindAttempts = 0;

	bool bInventoryPanelVisible = false;
	bool bInventoryPanelRequested = false;
};
