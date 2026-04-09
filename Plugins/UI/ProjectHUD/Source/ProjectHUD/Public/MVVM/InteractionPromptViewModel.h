// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "MVVM/ProjectViewModel.h"
#include "InteractionPromptViewModel.generated.h"

class IInteractionService;
class APawn;
class APlayerController;
struct FInteractionPromptState;

/**
 * ViewModel for interaction prompt widget.
 *
 * Responsibilities:
 * - Subscribe to IInteractionService (per-pawn filtered API)
 * - Expose bHasFocus and FocusLabel to widget
 *
 * Service handles filtering - ViewModel receives only local player's events.
 * Widget reads from this ViewModel only, never accesses game entities.
 */
UCLASS()
class PROJECTHUD_API UInteractionPromptViewModel : public UProjectViewModel
{
	GENERATED_BODY()

public:
	// =========================================================================
	// Lifecycle
	// =========================================================================

	/**
	 * Initialize with context to resolve local pawn.
	 * @param Context - APawn*, APlayerController*, or UUserWidget*
	 */
	virtual void Initialize(UObject* Context) override;
	virtual void Shutdown() override;

	// =========================================================================
	// ViewModel Properties (read by widget)
	// =========================================================================

	VIEWMODEL_PROPERTY(bool, bHasFocus)
	VIEWMODEL_PROPERTY(FText, FocusLabel)
	VIEWMODEL_PROPERTY(FText, FormattedPrompt)
	VIEWMODEL_PROPERTY(bool, bShowProgress)
	VIEWMODEL_PROPERTY(float, ProgressPercent)

private:
	// =========================================================================
	// Internal State
	// =========================================================================

	/** Local pawn for per-pawn service subscription */
	TWeakObjectPtr<APawn> LocalPawn;

	/** Owning local controller used to track respawn / pawn replacement. */
	TWeakObjectPtr<APlayerController> OwningController;

	/** Handle for controller pawn-change notifications. */
	FDelegateHandle PawnChangedHandle;

	/** Deferred retry when controller briefly reports no pawn during respawn. */
	FTSTicker::FDelegateHandle PendingPawnRefreshHandle;

	// =========================================================================
	// Service Integration
	// =========================================================================

	void BindToOwningController();
	void UnbindFromOwningController();
	void ScheduleObservedPawnRefresh();
	void ClearPendingPawnRefresh();
	bool RefreshObservedPawnFromController();
	void SetObservedPawn(APawn* NewPawn);
	void SubscribeToService();
	void UnsubscribeFromService();
	void PullInitialFocusState();
	void HandleObservedPawnChanged(APawn* NewPawn);

	/** Handle filtered prompt state change (only our pawn's events). */
	void HandlePromptStateChanged(const FInteractionPromptState& State);
};
