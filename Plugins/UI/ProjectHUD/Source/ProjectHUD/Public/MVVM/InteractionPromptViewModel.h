// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MVVM/ProjectViewModel.h"
#include "InteractionPromptViewModel.generated.h"

class IInteractionService;
class APawn;
class UPrimitiveComponent;

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

private:
	// =========================================================================
	// Internal State
	// =========================================================================

	/** Local pawn for per-pawn service subscription */
	TWeakObjectPtr<APawn> LocalPawn;

	// =========================================================================
	// Service Integration
	// =========================================================================

	void SubscribeToService();
	void UnsubscribeFromService();
	void PullInitialFocusState();

	/** Handle filtered focus change (only our pawn's events) */
	void HandleFocusChanged(UPrimitiveComponent* FocusedComponent, FText Label);
};
