// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "Components/ActorComponent.h"
#include "Scenario/SinglePlayScenarioStateMachine.h"
#include "SinglePlayScenarioPresentationComponent.generated.h"

class USinglePlayScenarioRunnerComponent;

UCLASS(ClassGroup=(UI))
class PROJECTSINGLEPLAYCLIENT_API USinglePlayScenarioPresentationComponent final : public UActorComponent
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandlePhaseChanged(ESinglePlayScenarioPhase Phase, const FString& Message);
	void ShowMessage(const FString& Message, float Duration, FName Type) const;

	TWeakObjectPtr<USinglePlayScenarioRunnerComponent> Runner;
	FDelegateHandle PhaseChangedHandle;
	bool bPreviewFlight = false;
};
