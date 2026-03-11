// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IDialogueService.h"

class UProjectDialogueComponent;

/**
 * IDialogueService implementation that bridges to the active dialogue component.
 *
 * Registered via FProjectServiceLocator in module startup.
 * Interaction handler calls SetActiveComponent() when dialogue starts.
 * Component events are forwarded as state changes and semantic signal tags.
 */
class FDialogueServiceImpl : public IDialogueService
{
public:
	FDialogueServiceImpl() = default;

	void SetActiveComponent(UProjectDialogueComponent* Component);
	void ClearActiveComponent();

	// IDialogueService
	virtual bool IsDialogueActive() const override;
	virtual FName GetCurrentTreeId() const override;
	virtual FName GetCurrentNodeId() const override;
	virtual FText GetCurrentSpeaker() const override;
	virtual FText GetCurrentText() const override;
	virtual void GetCurrentOptions(TArray<FDialogueOptionView>& OutOptions) const override;
	virtual bool IsCurrentNodeTerminal() const override;
	virtual bool CurrentNodeHasOptions() const override;
	virtual void SelectOption(int32 OptionIndex) override;
	virtual void AdvanceOrEnd() override;
	virtual void EndDialogue() override;
	virtual bool IsDialogueActiveForActor(const AActor* Actor) const override;
	virtual const AActor* GetActiveDialogueOwner() const override;
	virtual void ActivateDialogueComponent(class UActorComponent* Component) override;
	virtual FOnDialogueStateChanged& OnDialogueStateChanged() override;
	virtual FOnDialogueSignal& OnDialogueSignal() override;

private:
	void HandleNodeChanged(const FString& NodeId);
	void HandleOptionSelected(const FString& FromNodeId, int32 OptionIndex, const FString& NextNodeId);
	void HandleConversationEnded();
	void UnbindFromComponent();
	FName BuildSignalTagForNode(const FString& NodeId) const;
	FName BuildSignalTagForOptionSelection(const FString& FromNodeId, const FString& NextNodeId) const;

	TWeakObjectPtr<UProjectDialogueComponent> ActiveComponent;
	FOnDialogueStateChanged StateChangedDelegate;
	FOnDialogueSignal SignalDelegate;

	FDelegateHandle NodeChangedHandle;
	FDelegateHandle OptionSelectedHandle;
	FDelegateHandle ConversationEndedHandle;
};
