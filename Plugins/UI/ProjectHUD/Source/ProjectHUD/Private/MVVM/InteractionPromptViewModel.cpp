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
	Service->OnFocusChangedForPawn(Pawn).AddUObject(this, &UInteractionPromptViewModel::HandleFocusChanged);
	UE_LOG(LogInteractionPromptVM, Log, TEXT("SubscribeToService: Subscribed via OnFocusChangedForPawn"));

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

	// Find component implementing IInteractionComponentInterface
	TInlineComponentArray<UActorComponent*> Components;
	Pawn->GetComponents(Components);
	for (UActorComponent* Comp : Components)
	{
		if (Comp->Implements<UInteractionComponentInterface>())
		{
			UPrimitiveComponent* FocusedComp = IInteractionComponentInterface::Execute_GetFocusedComponent(Comp);
			if (FocusedComp)
			{
				FText Label = IInteractionComponentInterface::Execute_GetFocusedLabel(Comp);
				HandleFocusChanged(FocusedComp, Label);
				UE_LOG(LogInteractionPromptVM, Log, TEXT("PullInitialFocusState: Initial focus - Label='%s'"), *Label.ToString());
			}
			break;
		}
	}
}

void UInteractionPromptViewModel::HandleFocusChanged(UPrimitiveComponent* FocusedComponent, FText Label)
{
	// No filtering needed - service already filtered for our pawn

	if (FocusedComponent)
	{
		UpdatebHasFocus(true);
		UpdateFocusLabel(Label);

		FText Formatted = FText::Format(
			NSLOCTEXT("Interaction", "PromptFormat", "[E] {0}"),
			Label
		);
		UpdateFormattedPrompt(Formatted);

		UE_LOG(LogInteractionPromptVM, Log, TEXT("HandleFocusChanged: FOCUS ON - Label='%s'"), *Label.ToString());
	}
	else
	{
		UpdatebHasFocus(false);
		UpdateFocusLabel(FText::GetEmpty());
		UpdateFormattedPrompt(FText::GetEmpty());

		UE_LOG(LogInteractionPromptVM, Log, TEXT("HandleFocusChanged: FOCUS OFF"));
	}
}
