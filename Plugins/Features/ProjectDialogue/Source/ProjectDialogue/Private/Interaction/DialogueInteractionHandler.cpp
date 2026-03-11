// Copyright ALIS. All Rights Reserved.

#include "Interaction/DialogueInteractionHandler.h"
#include "Services/DialogueServiceImpl.h"
#include "Interfaces/IInteractionService.h"
#include "ProjectServiceLocator.h"
#include "Components/ProjectDialogueComponent.h"
#include "GameFramework/Pawn.h"

DEFINE_LOG_CATEGORY_STATIC(LogDialogueInteraction, Log, All);

FDialogueInteractionHandler::~FDialogueInteractionHandler()
{
	Unsubscribe();
}

void FDialogueInteractionHandler::Subscribe()
{
	if (InteractionHandle.IsValid())
	{
		return;
	}

	TSharedPtr<IInteractionService> InteractionService = FProjectServiceLocator::Resolve<IInteractionService>();
	if (!InteractionService.IsValid())
	{
		UE_LOG(LogDialogueInteraction, Log,
			TEXT("[DialogueInteractionHandler::Subscribe] IInteractionService not yet available"));
		return;
	}

	InteractionHandle = InteractionService->OnInteraction().AddSP(
		AsShared(), &FDialogueInteractionHandler::HandleInteraction);
}

void FDialogueInteractionHandler::Unsubscribe()
{
	if (!InteractionHandle.IsValid())
	{
		return;
	}

	TSharedPtr<IInteractionService> InteractionService = FProjectServiceLocator::Resolve<IInteractionService>();
	if (InteractionService.IsValid())
	{
		InteractionService->OnInteraction().Remove(InteractionHandle);
	}

	InteractionHandle.Reset();
}

void FDialogueInteractionHandler::SetService(TSharedPtr<FDialogueServiceImpl> InService)
{
	Service = InService;
}

void FDialogueInteractionHandler::HandleInteraction(AActor* Target, AActor* Instigator)
{
	if (!Target || !Instigator || !Service.IsValid())
	{
		return;
	}

	// Only handle on server (interaction is server-authoritative)
	if (!Instigator->HasAuthority())
	{
		return;
	}

	// Toggle: pressing E while dialogue is active ends it (like "Leave")
	if (Service->IsDialogueActive())
	{
		// Do not auto-close dialogue when the interaction was performed on
		// another actor (for example: locked door triggers NPC watcher dialogue).
		if (!Service->IsDialogueActiveForActor(Target))
		{
			UE_LOG(LogDialogueInteraction, Verbose,
				TEXT("[HandleInteraction] Dialogue active on another actor, ignoring interaction target '%s'"),
				*GetNameSafe(Target));
			return;
		}

		// Interaction execution and interaction broadcast happen in the same press.
		// Ignore same-frame rebroadcast for the just-started conversation to avoid
		// immediate close right after StartConversation().
		if (UProjectDialogueComponent* DialogueComp = FindDialogueComponent(Target))
		{
			if (DialogueComp->IsInConversation() && DialogueComp->GetConversationStartFrame() == GFrameCounter)
			{
				UE_LOG(LogDialogueInteraction, Verbose,
					TEXT("[HandleInteraction] Ignoring same-frame rebroadcast for '%s'"),
					*GetNameSafe(Target));
				return;
			}
		}

		UE_LOG(LogDialogueInteraction, Log,
			TEXT("[HandleInteraction] Dialogue active on '%s', ending on re-interact"),
			*GetNameSafe(Target));
		Service->EndDialogue();
		return;
	}

	UProjectDialogueComponent* DialogueComp = FindDialogueComponent(Target);
	if (!DialogueComp)
	{
		return;
	}

	Service->SetActiveComponent(DialogueComp);
}

UProjectDialogueComponent* FDialogueInteractionHandler::FindDialogueComponent(AActor* Target) const
{
	if (!Target)
	{
		return nullptr;
	}

	return Target->FindComponentByClass<UProjectDialogueComponent>();
}
