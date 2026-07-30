// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

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
	// Broadcast teardown and deactivate managed capabilities before destruction.
	// Skip if already Idle or already TearingDown (RequestTeardown was called earlier).
	if (AssemblyState == ESkeletalAssemblyState::Assembling
		|| AssemblyState == ESkeletalAssemblyState::Ready)
	{
		const ESkeletalAssemblyState OldState = AssemblyState;
		AssemblyState = ESkeletalAssemblyState::TearingDown;

		DeactivateManagedCapabilities();

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
	if (!TransitionTo(ESkeletalAssemblyState::Ready))
	{
		return false;
	}

	ActivateManagedCapabilities();
	return true;
}

bool USkeletalAssemblyComponent::RequestTeardown()
{
	if (!TransitionTo(ESkeletalAssemblyState::TearingDown))
	{
		return false;
	}

	DeactivateManagedCapabilities();

	// TearingDown is synchronous: all teardown work completes within this call.
	// Re-entrant transitions are rejected by IsValidTransition.
	TransitionTo(ESkeletalAssemblyState::Idle);
	return true;
}

void USkeletalAssemblyComponent::RegisterManagedCapability(UActorComponent* Capability)
{
	if (!Capability)
	{
		return;
	}

	ManagedCapabilities.Add(Capability);

	UE_LOG(LogSkeletalAssembly, Verbose,
		TEXT("[%s] Registered managed capability: %s"),
		*GetOwnerName(this), *Capability->GetClass()->GetName());
}

void USkeletalAssemblyComponent::ActivateManagedCapabilities()
{
	for (const TWeakObjectPtr<UActorComponent>& WeakCap : ManagedCapabilities)
	{
		if (UActorComponent* Cap = WeakCap.Get())
		{
			Cap->Activate();
			UE_LOG(LogSkeletalAssembly, Log,
				TEXT("[%s] Activated managed capability: %s"),
				*GetOwnerName(this), *Cap->GetClass()->GetName());
		}
	}
}

void USkeletalAssemblyComponent::DeactivateManagedCapabilities()
{
	for (const TWeakObjectPtr<UActorComponent>& WeakCap : ManagedCapabilities)
	{
		if (UActorComponent* Cap = WeakCap.Get())
		{
			Cap->Deactivate();
			UE_LOG(LogSkeletalAssembly, Log,
				TEXT("[%s] Deactivated managed capability: %s"),
				*GetOwnerName(this), *Cap->GetClass()->GetName());
		}
	}
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

	// Fire native delegate for IAssemblyCapability consumers (DIP boundary)
	const EAssemblyState MappedState = GetCurrentAssemblyState();
	OnAssemblyStateChangedNative.Broadcast(MappedState);

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

//////////////////////////////////////////////////////////////////////////
// IAssemblyCapability + IAssemblyViewConfigSource

EAssemblyState USkeletalAssemblyComponent::GetCurrentAssemblyState() const
{
	switch (AssemblyState)
	{
	case ESkeletalAssemblyState::Idle:         return EAssemblyState::Idle;
	case ESkeletalAssemblyState::Assembling:   return EAssemblyState::Assembling;
	case ESkeletalAssemblyState::Ready:        return EAssemblyState::Ready;
	case ESkeletalAssemblyState::TearingDown:  return EAssemblyState::TearingDown;
	default:                                   return EAssemblyState::Idle;
	}
}

FDelegateHandle USkeletalAssemblyComponent::AddAssemblyStateChanged(
	const FOnAssemblyStateChangedNative::FDelegate& Callback)
{
	return OnAssemblyStateChangedNative.Add(Callback);
}

void USkeletalAssemblyComponent::RemoveAssemblyStateChanged(FDelegateHandle Handle)
{
	OnAssemblyStateChangedNative.Remove(Handle);
}

void USkeletalAssemblyComponent::SetViewConfig(const FAssemblyViewConfig& Config)
{
	CachedViewConfig = Config;
	bHasViewConfig = true;
}

bool USkeletalAssemblyComponent::GetViewConfig(FAssemblyViewConfig& OutConfig) const
{
	if (!bHasViewConfig)
	{
		return false;
	}
	OutConfig = CachedViewConfig;
	return true;
}
