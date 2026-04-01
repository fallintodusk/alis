// Copyright ALIS. All Rights Reserved.

#include "SkeletalAssemblyComponent.h"
#include "ProjectSkeletalAssemblyModule.h"

namespace
{
	const TCHAR* StateToString(ESkeletalAssemblyState State)
	{
		switch (State)
		{
		case ESkeletalAssemblyState::Idle:         return TEXT("Idle");
		case ESkeletalAssemblyState::Assembling:   return TEXT("Assembling");
		case ESkeletalAssemblyState::Ready:        return TEXT("Ready");
		case ESkeletalAssemblyState::TearingDown:  return TEXT("TearingDown");
		default:                                   return TEXT("Unknown");
		}
	}

	FString GetOwnerName(const UActorComponent* Comp)
	{
		const AActor* Owner = Comp ? Comp->GetOwner() : nullptr;
		return Owner ? Owner->GetName() : TEXT("NoOwner");
	}
}

USkeletalAssemblyComponent::USkeletalAssemblyComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USkeletalAssemblyComponent::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogSkeletalAssembly, Verbose,
		TEXT("[%s] SkeletalAssemblyComponent BeginPlay. State: %s"),
		*GetOwnerName(this), StateToString(AssemblyState));
}

void USkeletalAssemblyComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Broadcast teardown so listeners can clean up before the actor is destroyed.
	// Skip if already Idle or already TearingDown (RequestTeardown was called earlier).
	if (AssemblyState == ESkeletalAssemblyState::Assembling
		|| AssemblyState == ESkeletalAssemblyState::Ready)
	{
		const ESkeletalAssemblyState OldState = AssemblyState;
		AssemblyState = ESkeletalAssemblyState::TearingDown;

		UE_LOG(LogSkeletalAssembly, Log,
			TEXT("[%s] EndPlay during state %s. Transitioning through TearingDown -> Idle."),
			*GetOwnerName(this), StateToString(OldState));

		OnAssemblyStateChanged.Broadcast(OldState, ESkeletalAssemblyState::TearingDown);
	}

	if (AssemblyState == ESkeletalAssemblyState::TearingDown)
	{
		AssemblyState = ESkeletalAssemblyState::Idle;
		OnAssemblyStateChanged.Broadcast(ESkeletalAssemblyState::TearingDown, ESkeletalAssemblyState::Idle);
	}

	Super::EndPlay(EndPlayReason);
}

bool USkeletalAssemblyComponent::RequestAssembly()
{
	return TransitionTo(ESkeletalAssemblyState::Assembling);
}

bool USkeletalAssemblyComponent::CompleteAssembly()
{
	return TransitionTo(ESkeletalAssemblyState::Ready);
}

bool USkeletalAssemblyComponent::RequestTeardown()
{
	if (!TransitionTo(ESkeletalAssemblyState::TearingDown))
	{
		return false;
	}

	// TearingDown is synchronous in Phase 1: all teardown work must complete
	// inside the OnAssemblyStateChanged delegate handler for TearingDown.
	// Re-entrant transitions (e.g. calling RequestAssembly from a TearingDown
	// handler) are rejected by IsValidTransition -- TearingDown only allows Idle.
	// When Phase 2 adds async feature deactivation, this should become deferred.
	TransitionTo(ESkeletalAssemblyState::Idle);
	return true;
}

bool USkeletalAssemblyComponent::TransitionTo(ESkeletalAssemblyState NewState)
{
	if (!IsValidTransition(AssemblyState, NewState))
	{
		UE_LOG(LogSkeletalAssembly, Warning,
			TEXT("[%s] Invalid state transition: %s -> %s"),
			*GetOwnerName(this),
			StateToString(AssemblyState),
			StateToString(NewState));
		return false;
	}

	const ESkeletalAssemblyState OldState = AssemblyState;
	AssemblyState = NewState;

	UE_LOG(LogSkeletalAssembly, Log,
		TEXT("[%s] State: %s -> %s"),
		*GetOwnerName(this),
		StateToString(OldState),
		StateToString(NewState));

	OnAssemblyStateChanged.Broadcast(OldState, NewState);
	return true;
}

bool USkeletalAssemblyComponent::IsValidTransition(ESkeletalAssemblyState From, ESkeletalAssemblyState To) const
{
	switch (From)
	{
	case ESkeletalAssemblyState::Idle:
		return To == ESkeletalAssemblyState::Assembling;

	case ESkeletalAssemblyState::Assembling:
		return To == ESkeletalAssemblyState::Ready
			|| To == ESkeletalAssemblyState::TearingDown;

	case ESkeletalAssemblyState::Ready:
		return To == ESkeletalAssemblyState::TearingDown;

	case ESkeletalAssemblyState::TearingDown:
		return To == ESkeletalAssemblyState::Idle;

	default:
		return false;
	}
}
