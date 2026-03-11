// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/ProjectUserWidget.h"
#include "W_DialoguePanel.generated.h"

class UDialogueViewModel;
class UTextBlock;
class UVerticalBox;
class UButton;

/**
 * Dialogue panel widget showing speaker, text, and choice buttons.
 *
 * Inherits UProjectUserWidget for data-driven JSON layout and automatic
 * ViewModel injection. Layout defined in Data/DialoguePanel.json.
 * See Plugins/UI/ProjectDialogueUI/README.md for architecture rationale.
 *
 * Architecture:
 * - Widget only reads from UDialogueViewModel
 * - ViewModel handles IDialogueService subscription
 * - Widget never accesses game entities or ProjectDialogue plugin
 *
 * Visibility is controlled by bIsActive ViewModel property (auto_visibility).
 * Speaker name row is hidden when empty (for object dialogues).
 * Choice buttons are dynamically created from Options array.
 * Continue/End button shown for auto-advance/terminal nodes.
 */
UCLASS()
class PROJECTDIALOGUEUI_API UW_DialoguePanel : public UProjectUserWidget
{
	GENERATED_BODY()

public:
	UW_DialoguePanel(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeConstruct() override;

	// UProjectUserWidget overrides
	virtual void BindCallbacks() override;
	virtual void OnViewModelChanged_Implementation(UProjectViewModel* OldViewModel, UProjectViewModel* NewViewModel) override;
	virtual void RefreshFromViewModel_Implementation() override;

private:
	// Widget references resolved by FProjectUIWidgetBinder from JSON layout
	UPROPERTY()
	TObjectPtr<UTextBlock> SpeakerText;

	UPROPERTY()
	TObjectPtr<UTextBlock> DialogueText;

	UPROPERTY()
	TObjectPtr<UVerticalBox> OptionsContainer;

	UPROPERTY()
	TObjectPtr<UButton> ContinueButton;

	UPROPERTY()
	TObjectPtr<UDialogueViewModel> DialogueVM;

	void RebuildOptionButtons();

	UFUNCTION()
	void HandleContinueClicked();

	UFUNCTION()
	void HandleOption0Clicked();
	UFUNCTION()
	void HandleOption1Clicked();
	UFUNCTION()
	void HandleOption2Clicked();
	UFUNCTION()
	void HandleOption3Clicked();
	UFUNCTION()
	void HandleOption4Clicked();
	UFUNCTION()
	void HandleOption5Clicked();

	void HandleOptionClicked(int32 Index);

	static constexpr int32 MaxVisibleOptions = 6;
};
