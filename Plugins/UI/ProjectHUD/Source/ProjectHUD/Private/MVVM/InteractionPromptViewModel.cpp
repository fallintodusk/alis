// Copyright ALIS. All Rights Reserved.

#include "MVVM/InteractionPromptViewModel.h"
#include "Interfaces/IInteractionService.h"
#include "ProjectServiceLocator.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogInteractionPromptVM, Log, All);

void UInteractionPromptViewModel::Initialize(UObject* Context)
{
	Super::Initialize(Context);

	// Resolve local pawn from context (needed for per-pawn subscription)
	APawn* ResolvedPawn = nullptr;

	if (APawn* DirectPawn = Cast<APawn>(Context))
	{
		ResolvedPawn = DirectPawn;
	}
	else if (APlayerController* PC = Cast<APlayerController>(Context))
	{
		ResolvedPawn = PC->GetPawn();
	}
	else if (UUserWidget* Widget = Cast<UUserWidget>(Context))
	{
		if (APlayerController* OwningPC = Widget->GetOwningPlayer())
		{
			ResolvedPawn = OwningPC->GetPawn();
		}
	}

	if (ResolvedPawn)
	{
		LocalPawn = ResolvedPawn;
		UE_LOG(LogInteractionPromptVM, Log, TEXT("Initialize: LocalPawn=%s"), *GetNameSafe(ResolvedPawn));
		SubscribeToService();
	}
	else
	{
		UE_LOG(LogInteractionPromptVM, Warning, TEXT("Initialize: Could not resolve LocalPawn from context [%s]"), *GetNameSafe(Context));
	}
}

void UInteractionPromptViewModel::Shutdown()
{
	UnsubscribeFromService();
	LocalPawn.Reset();
	Super::Shutdown();
}

void UInteractionPromptViewModel::SubscribeToService()
{
	APawn* Pawn = LocalPawn.Get();
	if (!Pawn)
	{
		return;
	}

	TSharedPtr<IInteractionService> Service = FProjectServiceLocator::Resolve<IInteractionService>();
	if (!Service.IsValid())
	{
		UE_LOG(LogInteractionPromptVM, Warning, TEXT("SubscribeToService: IInteractionService not available"));
		return;
	}

	// Use per-pawn filtered API - service handles event routing
	Service->OnPromptStateChangedForPawn(Pawn).AddUObject(this, &UInteractionPromptViewModel::HandlePromptStateChanged);
	UE_LOG(LogInteractionPromptVM, Log, TEXT("SubscribeToService: Subscribed via OnPromptStateChangedForPawn"));

	// Sync with current state
	PullInitialFocusState();
}

void UInteractionPromptViewModel::UnsubscribeFromService()
{
	APawn* Pawn = LocalPawn.Get();
	if (!Pawn)
	{
		return;
	}

	TSharedPtr<IInteractionService> Service = FProjectServiceLocator::Resolve<IInteractionService>();
	if (Service.IsValid())
	{
		Service->UnsubscribeForPawn(Pawn);
		UE_LOG(LogInteractionPromptVM, Log, TEXT("UnsubscribeFromService: Unsubscribed"));
	}
}

void UInteractionPromptViewModel::PullInitialFocusState()
{
	APawn* Pawn = LocalPawn.Get();
	if (!Pawn)
	{
		return;
	}

	HandlePromptStateChanged(FInteractionPromptState());

	// Find component implementing IInteractionComponentInterface
	TInlineComponentArray<UActorComponent*> Components;
	Pawn->GetComponents(Components);
	for (UActorComponent* Comp : Components)
	{
		if (Comp->Implements<UInteractionComponentInterface>())
		{
			const FInteractionPromptState PromptState = IInteractionComponentInterface::Execute_GetInteractionPromptState(Comp);
			HandlePromptStateChanged(PromptState);
			break;
		}
	}
}

void UInteractionPromptViewModel::HandlePromptStateChanged(const FInteractionPromptState& State)
{
	if (State.IsVisible())
	{
		UpdatebHasFocus(true);
		UpdateFocusLabel(State.Label);
		UpdatebShowProgress(State.bIsInProgress);
		UpdateProgressPercent(FMath::Clamp(State.Progress, 0.0f, 1.0f));

		FText Formatted;
		if (State.bIsInProgress)
		{
			Formatted = State.Label;
		}
		else if (State.bRequiresHold)
		{
			Formatted = FText::Format(
				NSLOCTEXT("Interaction", "HoldPromptFormat", "[Hold E] {0}"),
				State.Label);
		}
		else
		{
			Formatted = FText::Format(
				NSLOCTEXT("Interaction", "PromptFormat", "[E] {0}"),
				State.Label);
		}
		UpdateFormattedPrompt(Formatted);

		UE_LOG(LogInteractionPromptVM, Verbose, TEXT("HandlePromptStateChanged: Visible - Label='%s' Progress=%.2f InProgress=%s"),
			*State.Label.ToString(),
			State.Progress,
			State.bIsInProgress ? TEXT("true") : TEXT("false"));
	}
	else
	{
		UpdatebHasFocus(false);
		UpdateFocusLabel(FText::GetEmpty());
		UpdateFormattedPrompt(FText::GetEmpty());
		UpdatebShowProgress(false);
		UpdateProgressPercent(0.0f);

		UE_LOG(LogInteractionPromptVM, Verbose, TEXT("HandlePromptStateChanged: Hidden"));
	}
}
