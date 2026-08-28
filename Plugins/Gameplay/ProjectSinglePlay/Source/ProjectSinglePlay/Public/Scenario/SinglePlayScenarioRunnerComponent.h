// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "Components/ActorComponent.h"
#include "Scenario/SinglePlayScenarioProfile.h"
#include "Scenario/SinglePlayScenarioStateMachine.h"
#include "SinglePlayScenarioRunnerComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FOnSinglePlayScenarioPhaseChanged,
	ESinglePlayScenarioPhase,
	const FString&);

UCLASS(ClassGroup=(Gameplay))
class PROJECTSINGLEPLAY_API USinglePlayScenarioRunnerComponent final : public UActorComponent
{
	GENERATED_BODY()

public:
	USinglePlayScenarioRunnerComponent();

	bool Configure(FName ScenarioId, FString& OutError);
	void Start(APawn* InPawn);
	FName GetScenarioId() const { return Profile.ScenarioId; }
	ESinglePlayScenarioPhase GetPhase() const { return StateMachine.GetPhase(); }
	const FString& GetCurrentMessage() const { return CurrentMessage; }
	FOnSinglePlayScenarioPhaseChanged& OnPhaseChanged() { return PhaseChanged; }

protected:
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	FSinglePlayScenarioObservation Observe() const;
	AActor* FindAnchor(FName ActorTag) const;
	void PublishPhase(ESinglePlayScenarioPhase Phase);
	void HandleRestartTimer();

	FSinglePlayScenarioProfile Profile;
	FSinglePlayScenarioStateMachine StateMachine;
	TWeakObjectPtr<APawn> Pawn;
	FString CurrentMessage;
	FOnSinglePlayScenarioPhaseChanged PhaseChanged;
	FTimerHandle RestartTimer;
	bool bConfigured = false;
};
