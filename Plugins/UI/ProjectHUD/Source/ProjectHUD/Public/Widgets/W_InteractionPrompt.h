// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/ProjectUserWidget.h"
#include "W_InteractionPrompt.generated.h"

class UTextBlock;
class UInteractionPromptViewModel;

/**
 * Simple HUD prompt showing "[E] {Label}" when focusing interactable parts.
 *
 * Architecture:
 * - Widget only reads from InteractionPromptViewModel
 * - ViewModel handles IInteractionService subscription and filtering
 * - Widget never accesses game entities (PC, Pawn, etc.)
 *
 * Layout: InteractionPrompt.json - Border with centered TextBlock.
 * Format: "[E] Open" / "[E] Close" / "[E] Interact"
 */
UCLASS()
class PROJECTHUD_API UW_InteractionPrompt : public UProjectUserWidget
{
	GENERATED_BODY()

public:
	UW_InteractionPrompt(const FObjectInitializer& ObjectInitializer);

	/**
	 * Set the ViewModel (called by factory or parent widget).
	 * Widget binds to ViewModel properties for updates.
	 */
	void SetInteractionViewModel(UInteractionPromptViewModel* InViewModel);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	/** ViewModel providing focus state and label */
	UPROPERTY()
	TObjectPtr<UInteractionPromptViewModel> InteractionVM;

	/** Text block showing "[E] {Label}" - found by name in JSON layout */
	TWeakObjectPtr<UTextBlock> PromptText;

	/** Handle ViewModel property changes */
	void HandleViewModelPropertyChanged(FName PropertyName);

	/** Update widget display based on ViewModel state */
	void RefreshFromViewModel();
};
