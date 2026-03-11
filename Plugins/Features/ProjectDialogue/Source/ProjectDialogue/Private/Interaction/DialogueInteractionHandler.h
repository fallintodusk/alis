// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FDialogueServiceImpl;
class UProjectDialogueComponent;

/**
 * Handles dialogue-related interactions.
 * Subscribes to IInteractionService::OnInteraction() and routes to dialogue.
 *
 * Pattern: follows FInventoryInteractionHandler.
 *
 * When player presses E on an actor with UProjectDialogueComponent:
 * - If no active dialogue: start conversation
 * - If already in dialogue: end it (toggle - press E to leave)
 */
class FDialogueInteractionHandler : public TSharedFromThis<FDialogueInteractionHandler>
{
public:
	FDialogueInteractionHandler() = default;
	~FDialogueInteractionHandler();

	void Subscribe();
	void Unsubscribe();
	bool IsSubscribed() const { return InteractionHandle.IsValid(); }

	void SetService(TSharedPtr<FDialogueServiceImpl> InService);

private:
	void HandleInteraction(AActor* Target, AActor* Instigator);
	UProjectDialogueComponent* FindDialogueComponent(AActor* Target) const;

	FDelegateHandle InteractionHandle;
	TSharedPtr<FDialogueServiceImpl> Service;
};
