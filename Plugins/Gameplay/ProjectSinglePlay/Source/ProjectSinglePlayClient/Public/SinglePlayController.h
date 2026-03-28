#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "SinglePlayController.generated.h"

class UInputMappingContext;
class UInputAction;
class UVitalsViewModel;
class UMindJournalViewModel;
struct FInputActionValue;

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
