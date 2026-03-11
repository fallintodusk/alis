// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "MVVM/ProjectViewModel.h"
#include "Interfaces/IDialogueService.h"
#include "DialogueViewModel.generated.h"

/**
 * ViewModel for the dialogue panel widget.
 *
 * Subscribes to IDialogueService (via ServiceLocator).
 * Exposes current dialogue state to the widget.
 * True blackbox: no dependency on ProjectDialogue feature plugin.
 *
 * Usage:
 *   Widget creates ViewModel, calls Initialize(Context).
 *   ViewModel resolves IDialogueService, subscribes to state changes.
 *   Widget binds to OnPropertyChanged, reads properties.
 */
UCLASS()
class PROJECTDIALOGUEUI_API UDialogueViewModel : public UProjectViewModel
{
	GENERATED_BODY()

public:
	virtual void Initialize(UObject* Context) override;
	virtual void Shutdown() override;
	virtual bool GetBoolProperty(FName PropertyName) const override;

	// Commands (called by widget button clicks)
	UFUNCTION(BlueprintCallable, Category = "DialogueUI")
	void OnOptionSelected(int32 OptionIndex);

	UFUNCTION(BlueprintCallable, Category = "DialogueUI")
	void OnAdvanceClicked();

	UFUNCTION(BlueprintCallable, Category = "DialogueUI")
	void OnEndClicked();

	// ViewModel properties
	VIEWMODEL_PROPERTY(bool, bIsActive)
	VIEWMODEL_PROPERTY(FText, SpeakerName)
	VIEWMODEL_PROPERTY(FText, DialogueText)
	VIEWMODEL_PROPERTY(bool, bHasOptions)
	VIEWMODEL_PROPERTY(bool, bIsTerminal)
	VIEWMODEL_PROPERTY(bool, bHasSpeaker)
	VIEWMODEL_PROPERTY_ARRAY(FDialogueOptionView, Options)

private:
	void SubscribeToService();
	void UnsubscribeFromService();
	void StopServiceRetry();
	bool RetrySubscribeToService(float DeltaTime);
	void HandleDialogueStateChanged();
	void RefreshFromService();

	FDelegateHandle StateChangedHandle;
	FTSTicker::FDelegateHandle ServiceRetryHandle;
};
