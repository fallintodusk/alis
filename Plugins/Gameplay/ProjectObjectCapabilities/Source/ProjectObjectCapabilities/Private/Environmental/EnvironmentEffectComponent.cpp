// Copyright ALIS. All Rights Reserved.

#include "Environmental/EnvironmentEffectComponent.h"
#include "Components/ShapeComponent.h"
#include "GameFramework/Pawn.h"
#include "ProjectServiceLocator.h"
#include "Services/IAttributeEffectService.h"
#include "Interfaces/IProjectActionReceiver.h"

DEFINE_LOG_CATEGORY_STATIC(LogEnvironmentEffect, Log, All);

UEnvironmentEffectComponent::UEnvironmentEffectComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

FPrimaryAssetId UEnvironmentEffectComponent::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(FPrimaryAssetType(TEXT("CapabilityComponent")), FName(TEXT("EnvironmentEffect")));
}

void UEnvironmentEffectComponent::BeginPlay()
{
	Super::BeginPlay();

	// Find the named UShapeComponent on the owning actor
	UShapeComponent* Trigger = nullptr;
	for (UActorComponent* Comp : GetOwner()->GetComponents())
	{
		UShapeComponent* Shape = Cast<UShapeComponent>(Comp);
		if (Shape && Shape->GetFName() == TriggerComponentName)
		{
			Trigger = Shape;
			break;
		}
	}

	if (!Trigger)
	{
		UE_LOG(LogEnvironmentEffect, Warning,
			TEXT("[%s::BeginPlay] No UShapeComponent named '%s' found on '%s'. Deactivating."),
			*GetName(), *TriggerComponentName.ToString(), *GetNameSafe(GetOwner()));
		Deactivate();
		return;
	}

	Trigger->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleBeginOverlap);
	Trigger->OnComponentEndOverlap.AddDynamic(this, &ThisClass::HandleEndOverlap);
	bIsEffectEnabled = true;

	ValidateConfig();
}

void UEnvironmentEffectComponent::EndPlay(EEndPlayReason::Type Reason)
{
	// Clear all per-actor timers
	if (UWorld* World = GetWorld())
	{
		for (auto& Pair : ActiveActors)
		{
			World->GetTimerManager().ClearTimer(Pair.Value.PeriodicTimerHandle);
		}
	}

	ActiveActors.Empty();
	PermanentAppliedActors.Empty();
	bIsEffectEnabled = false;

	Super::EndPlay(Reason);
}

void UEnvironmentEffectComponent::HandleBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!bIsEffectEnabled || !IsValidTarget(OtherActor))
	{
		return;
	}

	// Already hit persistent limit for this actor
	if (bPersistentApplicationLimit && PermanentAppliedActors.Contains(OtherActor))
	{
		return;
	}

	// Dedupe: already tracking this actor
	if (ActiveActors.Contains(OtherActor))
	{
		return;
	}

	FEffectState& State = ActiveActors.Add(OtherActor);

	// Apply entry magnitudes
	if (!EntryMagnitudes.IsEmpty())
	{
		if (ApplyMagnitudes(OtherActor, EntryMagnitudes))
		{
			State.ApplicationCount++;

			// Persist immediately if limit reached
			if (bPersistentApplicationLimit && MaxApplications > 0
				&& State.ApplicationCount >= MaxApplications)
			{
				PermanentAppliedActors.Add(OtherActor);
			}
		}
	}

	DispatchAction();

	// Start periodic timer if configured
	if (TickInterval > 0.0f && !PeriodicMagnitudes.IsEmpty())
	{
		// Skip if already at max
		if (MaxApplications > 0 && State.ApplicationCount >= MaxApplications)
		{
			return;
		}

		TWeakObjectPtr<AActor> WeakActor = OtherActor;
		GetWorld()->GetTimerManager().SetTimer(
			State.PeriodicTimerHandle,
			FTimerDelegate::CreateUObject(this, &ThisClass::HandlePeriodicTick, WeakActor),
			TickInterval,
			true);
	}
}

void UEnvironmentEffectComponent::HandleEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	if (!bIsEffectEnabled)
	{
		return;
	}

	FEffectState* State = ActiveActors.Find(OtherActor);
	if (!State)
	{
		return;
	}

	// Stop periodic timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(State->PeriodicTimerHandle);
	}

	// Apply exit magnitudes
	if (!ExitMagnitudes.IsEmpty())
	{
		ApplyMagnitudes(OtherActor, ExitMagnitudes);
	}

	ActiveActors.Remove(OtherActor);
}

void UEnvironmentEffectComponent::HandlePeriodicTick(TWeakObjectPtr<AActor> WeakActor)
{
	if (!WeakActor.IsValid())
	{
		// Actor was destroyed while inside the volume -- clear timer before removing state
		if (FEffectState* DeadState = ActiveActors.Find(WeakActor))
		{
			GetWorld()->GetTimerManager().ClearTimer(DeadState->PeriodicTimerHandle);
		}
		ActiveActors.Remove(WeakActor);
		return;
	}

	AActor* Actor = WeakActor.Get();

	FEffectState* State = ActiveActors.Find(Actor);
	if (!State)
	{
		return;
	}

	if (ApplyMagnitudes(Actor, PeriodicMagnitudes))
	{
		State->ApplicationCount++;

		DispatchAction();

		// Check if max reached
		if (MaxApplications > 0 && State->ApplicationCount >= MaxApplications)
		{
			GetWorld()->GetTimerManager().ClearTimer(State->PeriodicTimerHandle);

			if (bPersistentApplicationLimit)
			{
				PermanentAppliedActors.Add(Actor);
			}
		}
	}
}

bool UEnvironmentEffectComponent::ApplyMagnitudes(
	AActor* TargetActor,
	const TMap<FGameplayTag, float>& Magnitudes)
{
	TSharedPtr<IAttributeEffectService> EffectService =
		FProjectServiceLocator::Resolve<IAttributeEffectService>();
	if (!EffectService)
	{
		UE_LOG(LogEnvironmentEffect, Warning,
			TEXT("[%s] IAttributeEffectService not available (ProjectGAS not loaded?)"),
			*GetNameSafe(GetOwner()));
		return false;
	}

	return EffectService->ApplyMagnitudes(TargetActor, Magnitudes);
}

bool UEnvironmentEffectComponent::IsValidTarget(AActor* Actor) const
{
	if (!Actor || Actor == GetOwner())
	{
		return false;
	}

	if (bAffectPawnsOnly && !Actor->IsA<APawn>())
	{
		return false;
	}

	return true;
}

void UEnvironmentEffectComponent::DispatchAction()
{
	if (EntryAction.IsEmpty())
	{
		return;
	}

	// Copy component list before iterating -- overlap callbacks can modify the array
	TInlineComponentArray<UActorComponent*, 16> Components(GetOwner());
	for (UActorComponent* Comp : Components)
	{
		if (IProjectActionReceiver* Receiver = Cast<IProjectActionReceiver>(Comp))
		{
			Receiver->HandleAction(TEXT("EnvironmentEffect"), EntryAction);
		}
	}
}

void UEnvironmentEffectComponent::ValidateConfig() const
{
	if (TickInterval <= 0.0f && !PeriodicMagnitudes.IsEmpty())
	{
		UE_LOG(LogEnvironmentEffect, Warning,
			TEXT("[%s::BeginPlay] PeriodicMagnitudes set but TickInterval <= 0 on '%s'"),
			*GetName(), *GetNameSafe(GetOwner()));
	}

	if (MaxApplications < 0)
	{
		UE_LOG(LogEnvironmentEffect, Warning,
			TEXT("[%s::BeginPlay] MaxApplications < 0 on '%s'"),
			*GetName(), *GetNameSafe(GetOwner()));
	}

	if (EntryMagnitudes.IsEmpty() && PeriodicMagnitudes.IsEmpty() && ExitMagnitudes.IsEmpty())
	{
		UE_LOG(LogEnvironmentEffect, Warning,
			TEXT("[%s::BeginPlay] No magnitudes configured on '%s'"),
			*GetName(), *GetNameSafe(GetOwner()));
	}
}
