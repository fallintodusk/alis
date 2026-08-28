// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Scenario/SinglePlayScenarioRunnerComponent.h"

#include "Components/ActorComponent.h"
#include "EngineUtils.h"
#include "Interfaces/IInventoryReadOnly.h"
#include "Interfaces/IVitalsReadOnly.h"
#include "ProjectSinglePlayLog.h"
#include "SinglePlayerGameMode.h"
#include "TimerManager.h"
#include "UObject/PrimaryAssetId.h"

USinglePlayScenarioRunnerComponent::USinglePlayScenarioRunnerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickInterval = 0.25f;
}

bool USinglePlayScenarioRunnerComponent::Configure(FName ScenarioId, FString& OutError)
{
	bConfigured = FSinglePlayScenarioProfileLoader::Load(ScenarioId, Profile, OutError);
	SetComponentTickEnabled(false);
	return bConfigured;
}

void USinglePlayScenarioRunnerComponent::Start(APawn* InPawn)
{
	if (!bConfigured || InPawn == nullptr || !InPawn->HasAuthority())
	{
		return;
	}

	Pawn = InPawn;
	StateMachine.Start();
	SetComponentTickEnabled(true);
	PublishPhase(StateMachine.GetPhase());
	UE_LOG(LogProjectSinglePlay, Display,
		TEXT("[USinglePlayScenarioRunnerComponent::Start] Scenario started - id=%s pawn=%s"),
		*Profile.ScenarioId.ToString(), *InPawn->GetName());
}

void USinglePlayScenarioRunnerComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!Pawn.IsValid())
	{
		SetComponentTickEnabled(false);
		return;
	}

	const FSinglePlayScenarioTransition Transition =
		StateMachine.Evaluate(Observe(), Profile.RequiredHydrationFraction);
	if (!Transition.bChanged)
	{
		return;
	}

	PublishPhase(Transition.CurrentPhase);
	UE_LOG(LogProjectSinglePlay, Display,
		TEXT("[USinglePlayScenarioRunnerComponent::TickComponent] Phase changed - id=%s from=%d to=%d"),
		*Profile.ScenarioId.ToString(),
		static_cast<int32>(Transition.PreviousPhase),
		static_cast<int32>(Transition.CurrentPhase));

	if (Transition.CurrentPhase == ESinglePlayScenarioPhase::Succeeded)
	{
		SetComponentTickEnabled(false);
	}
	else if (Transition.bRequestRestart)
	{
		SetComponentTickEnabled(false);
		GetWorld()->GetTimerManager().SetTimer(
			RestartTimer,
			this,
			&ThisClass::HandleRestartTimer,
			Profile.FailureRestartDelaySeconds,
			false);
	}
}

FSinglePlayScenarioObservation USinglePlayScenarioRunnerComponent::Observe() const
{
	FSinglePlayScenarioObservation Observation;
	APawn* CurrentPawn = Pawn.Get();
	if (CurrentPawn == nullptr)
	{
		return Observation;
	}

	for (UActorComponent* Component : CurrentPawn->GetComponents())
	{
		if (const IInventoryReadOnly* Inventory = Cast<IInventoryReadOnly>(Component))
		{
			Observation.bHasRequiredRation = Inventory->ContainsItem(
				FPrimaryAssetId(
					FPrimaryAssetType(TEXT("ObjectDefinition")),
					Profile.RequiredRationObjectId));
		}
		if (const IVitalsReadOnly* Vitals = Cast<IVitalsReadOnly>(Component))
		{
			FVitalsReadOnlySnapshot Snapshot;
			if (Vitals->GetVitalsSnapshot(Snapshot))
			{
				Observation.HydrationFraction = Snapshot.GetHydrationFraction();
			}
		}
	}

	const FVector Location = CurrentPawn->GetActorLocation();
	if (AActor* Cache = FindAnchor(Profile.CacheActorTag))
	{
		Observation.bAtCache = FVector::DistSquared(Location, Cache->GetActorLocation()) <=
			FMath::Square(Profile.CacheInteractionRadius);
	}
	if (AActor* Shelter = FindAnchor(Profile.ShelterActorTag))
	{
		Observation.bAtShelter = FVector::DistSquared(Location, Shelter->GetActorLocation()) <=
			FMath::Square(Profile.ShelterInteractionRadius);
	}
	return Observation;
}

AActor* USinglePlayScenarioRunnerComponent::FindAnchor(FName ActorTag) const
{
	UWorld* World = GetWorld();
	if (World == nullptr || ActorTag.IsNone())
	{
		return nullptr;
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->ActorHasTag(ActorTag))
		{
			return *It;
		}
	}
	return nullptr;
}

void USinglePlayScenarioRunnerComponent::PublishPhase(ESinglePlayScenarioPhase Phase)
{
	switch (Phase)
	{
	case ESinglePlayScenarioPhase::SearchCache:
		CurrentMessage = Profile.StartMessage;
		break;
	case ESinglePlayScenarioPhase::Recover:
		CurrentMessage = Profile.RecoveryMessage;
		break;
	case ESinglePlayScenarioPhase::ReachShelter:
		CurrentMessage = Profile.RecoveredMessage;
		break;
	case ESinglePlayScenarioPhase::Succeeded:
		CurrentMessage = Profile.SuccessMessage;
		break;
	case ESinglePlayScenarioPhase::Failed:
		CurrentMessage = Profile.FailureMessage;
		break;
	default:
		return;
	}
	PhaseChanged.Broadcast(Phase, CurrentMessage);
}

void USinglePlayScenarioRunnerComponent::HandleRestartTimer()
{
	if (ASinglePlayerGameMode* GameMode = Cast<ASinglePlayerGameMode>(GetOwner()))
	{
		GameMode->RequestScenarioRestart();
	}
}
