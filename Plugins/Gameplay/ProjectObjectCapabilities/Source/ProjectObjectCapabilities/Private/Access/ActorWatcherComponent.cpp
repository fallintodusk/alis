// Copyright ALIS. All Rights Reserved.

#include "Access/ActorWatcherComponent.h"
#include "Access/LockableComponent.h"
#include "ProjectObjectCapabilitiesModule.h"
#include "TimerManager.h"
#include "EngineUtils.h"

namespace ActorWatcherTimings
{
	constexpr float RetryInterval = 0.5f;
	constexpr float HeartbeatInterval = 5.0f;
}

UActorWatcherComponent::UActorWatcherComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

FPrimaryAssetId UActorWatcherComponent::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(FPrimaryAssetType(TEXT("CapabilityComponent")), FName(TEXT("ActorWatcher")));
}

bool UActorWatcherComponent::MatchesResolveFilters(const AActor* Candidate) const
{
	if (!Candidate || Candidate == GetOwner())
	{
		return false;
	}

	if (!WatchedActorTag.IsNone() && !Candidate->ActorHasTag(WatchedActorTag))
	{
		return false;
	}

	if (!WatchedActorNameContains.IsEmpty())
	{
		return Candidate->GetName().Contains(WatchedActorNameContains, ESearchCase::IgnoreCase);
	}

	return true;
}

AActor* UActorWatcherComponent::ResolveWatchedActor() const
{
	if (AActor* ExplicitActor = WatchedActor.Get())
	{
		return ExplicitActor;
	}

	if (WatchedActorTag.IsNone() && WatchedActorNameContains.IsEmpty())
	{
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	AActor* Best = nullptr;
	double BestDistSq = TNumericLimits<double>::Max();
	const FVector Origin = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Candidate = *It;
		if (!MatchesResolveFilters(Candidate))
		{
			continue;
		}

		const double DistSq = FVector::DistSquared(Origin, Candidate->GetActorLocation());
		if (!Best || DistSq < BestDistSq)
		{
			Best = Candidate;
			BestDistSq = DistSq;
		}
	}

	return Best;
}

AActor* UActorWatcherComponent::GetCurrentWatchedActor() const
{
	if (AActor* Actor = BoundActor.Get())
	{
		return Actor;
	}
	return ResolveWatchedActor();
}

void UActorWatcherComponent::BeginPlay()
{
	Super::BeginPlay();
	TryBindToWatchedActor();
}

void UActorWatcherComponent::TryBindToWatchedActor()
{
	AActor* CurrentActor = ResolveWatchedActor();
	AActor* OldActor = BoundActor.Get();

	if (bBound && OldActor && OldActor == CurrentActor)
	{
		ScheduleCheck(ActorWatcherTimings::HeartbeatInterval);
		return;
	}

	if (bBound)
	{
		if (OldActor && Trigger == EActorWatchTrigger::LockAccessDenied)
		{
			if (ULockableComponent* OldLock = OldActor->FindComponentByClass<ULockableComponent>())
			{
				OldLock->OnAccessDenied.RemoveDynamic(this, &ThisClass::HandleLockAccessDenied);
			}
		}
		bBound = false;
		BoundActor.Reset();

		UE_LOG(LogProjectObjectCapabilities, Log,
			TEXT("[%s::ActorWatcher] Unbound stale watched actor reference"),
			*GetNameSafe(GetOwner()));
	}

	if (!CurrentActor)
	{
		UE_LOG(LogProjectObjectCapabilities, Verbose,
			TEXT("[%s::ActorWatcher] Watched actor not resolved yet (Tag=%s, NameContains='%s'), retrying..."),
			*GetNameSafe(GetOwner()),
			WatchedActorTag.IsNone() ? TEXT("<none>") : *WatchedActorTag.ToString(),
			*WatchedActorNameContains);
		ScheduleCheck(ActorWatcherTimings::RetryInterval);
		return;
	}

	if (Trigger != EActorWatchTrigger::LockAccessDenied)
	{
		UE_LOG(LogProjectObjectCapabilities, Warning,
			TEXT("[%s::ActorWatcher] Unsupported trigger type '%d'"),
			*GetNameSafe(GetOwner()), static_cast<int32>(Trigger));
		return;
	}

	ULockableComponent* Lockable = CurrentActor->FindComponentByClass<ULockableComponent>();
	if (!Lockable)
	{
		UE_LOG(LogProjectObjectCapabilities, Warning,
			TEXT("[%s::ActorWatcher] Watched actor '%s' has no LockableComponent, retrying..."),
			*GetNameSafe(GetOwner()), *GetNameSafe(CurrentActor));
		ScheduleCheck(ActorWatcherTimings::HeartbeatInterval);
		return;
	}

	Lockable->OnAccessDenied.AddDynamic(this, &ThisClass::HandleLockAccessDenied);
	BoundActor = CurrentActor;
	bBound = true;

	UE_LOG(LogProjectObjectCapabilities, Log,
		TEXT("[%s::ActorWatcher] Bound to '%s' using trigger LockAccessDenied"),
		*GetNameSafe(GetOwner()), *GetNameSafe(CurrentActor));

	ScheduleCheck(ActorWatcherTimings::HeartbeatInterval);
}

void UActorWatcherComponent::ScheduleCheck(float Interval)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			RetryHandle, this, &ThisClass::TryBindToWatchedActor, Interval, false);
	}
}

void UActorWatcherComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RetryHandle);
	}

	if (bBound)
	{
		if (AActor* OldActor = BoundActor.Get())
		{
			if (ULockableComponent* Lockable = OldActor->FindComponentByClass<ULockableComponent>())
			{
				Lockable->OnAccessDenied.RemoveDynamic(this, &ThisClass::HandleLockAccessDenied);
			}
		}
		bBound = false;
		BoundActor.Reset();
	}

	Super::EndPlay(Reason);
}

bool UActorWatcherComponent::PassesEventFilters(const FActorWatchEvent& Event) const
{
	if (!RequiredEventName.IsNone() && Event.EventName != RequiredEventName)
	{
		return false;
	}

	if (RequiredEventTag.IsValid() && Event.EventTag != RequiredEventTag)
	{
		return false;
	}

	if (!RequiredEventTextContains.IsEmpty() &&
		!Event.EventText.Contains(RequiredEventTextContains, ESearchCase::IgnoreCase))
	{
		return false;
	}

	return true;
}

void UActorWatcherComponent::NotifyOwnerListeners(const FActorWatchEvent& Event) const
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	TInlineComponentArray<UActorComponent*> Components;
	OwnerActor->GetComponents(Components);
	for (UActorComponent* Comp : Components)
	{
		if (!Comp || Comp == this || !Comp->GetClass()->ImplementsInterface(UActorWatchEventListener::StaticClass()))
		{
			continue;
		}

		IActorWatchEventListener* Listener = Cast<IActorWatchEventListener>(Comp);
		if (!Listener)
		{
			UE_LOG(LogProjectObjectCapabilities, Warning,
				TEXT("[%s::ActorWatcher] Component '%s' reports IActorWatchEventListener but cast failed"),
				*GetNameSafe(OwnerActor), *Comp->GetClass()->GetName());
			continue;
		}

		Listener->HandleActorWatchEvent(Event);
	}
}

void UActorWatcherComponent::EmitWatchEvent(const FActorWatchEvent& Event)
{
	if (!PassesEventFilters(Event))
	{
		UE_LOG(LogProjectObjectCapabilities, Verbose,
			TEXT("[%s::ActorWatcher] Event filtered out (Name=%s, Tag=%s, Text='%s')"),
			*GetNameSafe(GetOwner()),
			*Event.EventName.ToString(),
			*Event.EventTag.ToString(),
			*Event.EventText);
		return;
	}

	UE_LOG(LogProjectObjectCapabilities, Log,
		TEXT("[%s::ActorWatcher] Event emitted (Name=%s, Source=%s, Instigator=%s)"),
		*GetNameSafe(GetOwner()),
		*Event.EventName.ToString(),
		*GetNameSafe(Event.SourceActor),
		*GetNameSafe(Event.Instigator));

	OnWatchEvent.Broadcast(Event);
	NotifyOwnerListeners(Event);
}

void UActorWatcherComponent::HandleLockAccessDenied(FGameplayTag RequiredKeyTag, AActor* Instigator)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

	FActorWatchEvent Event;
	Event.EventName = FName(TEXT("lock.access_denied"));
	Event.EventTag = RequiredKeyTag;
	Event.EventText = RequiredKeyTag.ToString();
	Event.SourceActor = BoundActor.Get();
	Event.Instigator = Instigator;

	EmitWatchEvent(Event);
}
