// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Scenario/SinglePlayScenarioPresentationComponent.h"

#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"
#include "ProjectSinglePlayLog.h"
#include "Scenario/SinglePlayScenarioRunnerComponent.h"
#include "SinglePlayerGameMode.h"
#include "Subsystems/ProjectUILayerHostSubsystem.h"
#include "Subsystems/ProjectToastSubsystem.h"

namespace
{
	const FName PreviewFlightHintsDefinitionId(TEXT("ProjectSinglePlay.PreviewFlightHints"));
}

void USinglePlayScenarioPresentationComponent::BeginPlay()
{
	Super::BeginPlay();
	const APlayerController* Controller = Cast<APlayerController>(GetOwner());
	if (Controller == nullptr || !Controller->IsLocalController())
	{
		return;
	}

	ASinglePlayerGameMode* GameMode = GetWorld()->GetAuthGameMode<ASinglePlayerGameMode>();
	if (GameMode == nullptr)
	{
		return;
	}

	bPreviewFlight = GameMode->GetTraversalMode() == ESinglePlayTraversalMode::PreviewFlight;
	if (bPreviewFlight)
	{
		SetPreviewFlightHintsVisible(true);
	}

	USinglePlayScenarioRunnerComponent* ScenarioRunner =
		GameMode->GetScenarioRunner();
	if (ScenarioRunner == nullptr)
	{
		return;
	}

	Runner = ScenarioRunner;
	PhaseChangedHandle = ScenarioRunner->OnPhaseChanged().AddUObject(
		this,
		&ThisClass::HandlePhaseChanged);
	if (!bPreviewFlight && !ScenarioRunner->GetCurrentMessage().IsEmpty())
	{
		HandlePhaseChanged(
			ScenarioRunner->GetPhase(),
			ScenarioRunner->GetCurrentMessage());
	}
}

void USinglePlayScenarioPresentationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Runner.IsValid() && PhaseChangedHandle.IsValid())
	{
		Runner->OnPhaseChanged().Remove(PhaseChangedHandle);
	}
	Runner.Reset();
	PhaseChangedHandle.Reset();
	if (bPreviewFlight)
	{
		SetPreviewFlightHintsVisible(false);
	}
	Super::EndPlay(EndPlayReason);
}

void USinglePlayScenarioPresentationComponent::HandlePhaseChanged(
	ESinglePlayScenarioPhase Phase,
	const FString& Message)
{
	if (bPreviewFlight)
	{
		return;
	}

	const float Duration = Phase == ESinglePlayScenarioPhase::Succeeded ||
		Phase == ESinglePlayScenarioPhase::Failed
		? 7.0f
		: 15.0f;
	const FName Type = Phase == ESinglePlayScenarioPhase::Failed
		? FName(TEXT("Error"))
		: FName(TEXT("Info"));
	ShowMessage(Message, Duration, Type);
}

void USinglePlayScenarioPresentationComponent::ShowMessage(
	const FString& Message,
	float Duration,
	FName Type) const
{
	UGameInstance* GameInstance = GetWorld() == nullptr ? nullptr : GetWorld()->GetGameInstance();
	UProjectToastSubsystem* Toast = GameInstance == nullptr
		? nullptr
		: GameInstance->GetSubsystem<UProjectToastSubsystem>();
	if (Toast == nullptr || Message.IsEmpty())
	{
		return;
	}

	Toast->ShowToast(FText::FromString(Message), Duration, Type);
}

void USinglePlayScenarioPresentationComponent::SetPreviewFlightHintsVisible(bool bVisible) const
{
	APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	UGameInstance* GameInstance = GetWorld() == nullptr ? nullptr : GetWorld()->GetGameInstance();
	UProjectUILayerHostSubsystem* LayerHost = GameInstance == nullptr
		? nullptr
		: GameInstance->GetSubsystem<UProjectUILayerHostSubsystem>();
	if (LayerHost == nullptr)
	{
		if (bVisible)
		{
			UE_LOG(LogProjectSinglePlay, Error,
				TEXT("PreviewFlight controls cannot resolve the ProjectUI layer host."));
		}
		return;
	}

	if (bVisible)
	{
		if (PlayerController == nullptr || !PlayerController->IsLocalController())
		{
			UE_LOG(LogProjectSinglePlay, Error,
				TEXT("PreviewFlight controls cannot resolve the local player controller."));
			return;
		}
		LayerHost->InitializeForPlayer(PlayerController);
		if (LayerHost->ShowDefinition(PreviewFlightHintsDefinitionId) == nullptr)
		{
			UE_LOG(LogProjectSinglePlay, Error,
				TEXT("PreviewFlight controls failed to show registered definition %s."),
				*PreviewFlightHintsDefinitionId.ToString());
		}
	}
	else
	{
		LayerHost->HideDefinition(PreviewFlightHintsDefinitionId);
	}
}
