#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IInteractionService.h"

DECLARE_LOG_CATEGORY_EXTERN(LogInteractionService, Log, All);

/**
 * FInteractionService
 *
 * Implementation of IInteractionService.
 * Manages interaction event broadcasting to subscribers.
 */
class FInteractionService : public IInteractionService
{
public:
	FInteractionService()
	{
		UE_LOG(LogInteractionService, Log, TEXT("FInteractionService: Created"));
	}

	virtual ~FInteractionService()
	{
		UE_LOG(LogInteractionService, Log, TEXT("FInteractionService: Destroyed (Subscribers=%d)"),
			InteractionDelegate.IsBound() ? 1 : 0);
	}

	//~ IInteractionService interface
	virtual FInteractionDelegate& OnInteraction() override
	{
		return InteractionDelegate;
	}

	virtual FInteractionFocusDelegate& OnFocusChanged() override
	{
		return FocusDelegate;
	}

	virtual FInteractionFocusFilteredDelegate& OnFocusChangedForPawn(APawn* Pawn) override
	{
		if (!Pawn)
		{
			UE_LOG(LogInteractionService, Warning, TEXT("OnFocusChangedForPawn: Called with null Pawn"));
			static FInteractionFocusFilteredDelegate DummyDelegate;
			return DummyDelegate;
		}

		FInteractionFocusFilteredDelegate& Delegate = PerPawnDelegates.FindOrAdd(Pawn);
		UE_LOG(LogInteractionService, Log, TEXT("OnFocusChangedForPawn: Subscribed for Pawn=%s (TotalPawns=%d)"),
			*Pawn->GetName(), PerPawnDelegates.Num());
		return Delegate;
	}

	virtual void UnsubscribeForPawn(APawn* Pawn) override
	{
		if (!Pawn)
		{
			return;
		}

		if (PerPawnDelegates.Remove(Pawn) > 0)
		{
			UE_LOG(LogInteractionService, Log, TEXT("UnsubscribeForPawn: Unsubscribed Pawn=%s (TotalPawns=%d)"),
				*Pawn->GetName(), PerPawnDelegates.Num());
		}
	}

	virtual void BroadcastFocusChanged(APawn* Instigator, AActor* FocusedActor, UPrimitiveComponent* FocusedComponent, FText Label) override
	{
		UE_LOG(LogInteractionService, Verbose, TEXT("BroadcastFocusChanged: Instigator=%s Actor=%s Component=%s Label='%s'"),
			Instigator ? *Instigator->GetName() : TEXT("NULL"),
			FocusedActor ? *FocusedActor->GetName() : TEXT("NULL"),
			FocusedComponent ? *FocusedComponent->GetName() : TEXT("NULL"),
			*Label.ToString());

		// Broadcast to global subscribers (legacy API)
		FocusDelegate.Broadcast(Instigator, FocusedActor, FocusedComponent, Label);

		// Route to per-pawn subscriber (filtered API)
		if (Instigator)
		{
			if (FInteractionFocusFilteredDelegate* PawnDelegate = PerPawnDelegates.Find(Instigator))
			{
				PawnDelegate->Broadcast(FocusedComponent, Label);
			}
		}
	}

	/** Broadcast interaction event to all subscribers */
	void BroadcastInteraction(AActor* Target, AActor* Instigator)
	{
		if (!Target || !Instigator)
		{
			UE_LOG(LogInteractionService, Warning, TEXT("BroadcastInteraction: Invalid params - Target=%s, Instigator=%s"),
				Target ? *Target->GetName() : TEXT("NULL"),
				Instigator ? *Instigator->GetName() : TEXT("NULL"));
			return;
		}

		const int32 SubscriberCount = InteractionDelegate.IsBound() ? 1 : 0;
		UE_LOG(LogInteractionService, Log, TEXT("BroadcastInteraction: Target='%s' (Class=%s), Instigator='%s', Subscribers=%d"),
			*Target->GetName(), *Target->GetClass()->GetName(),
			*Instigator->GetName(), SubscriberCount);

		InteractionDelegate.Broadcast(Target, Instigator);
	}

private:
	FInteractionDelegate InteractionDelegate;
	FInteractionFocusDelegate FocusDelegate;

	/** Per-pawn delegates for filtered focus events */
	TMap<TWeakObjectPtr<APawn>, FInteractionFocusFilteredDelegate> PerPawnDelegates;
};
