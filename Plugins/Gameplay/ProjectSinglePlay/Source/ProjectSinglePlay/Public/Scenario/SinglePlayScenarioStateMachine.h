// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

enum class ESinglePlayScenarioPhase : uint8
{
	Inactive,
	SearchCache,
	Recover,
	ReachShelter,
	Succeeded,
	Failed
};

struct FSinglePlayScenarioObservation
{
	bool bAtCache = false;
	bool bAtShelter = false;
	bool bHasRequiredRation = false;
	double HydrationFraction = 0.0;
};

struct FSinglePlayScenarioTransition
{
	ESinglePlayScenarioPhase PreviousPhase = ESinglePlayScenarioPhase::Inactive;
	ESinglePlayScenarioPhase CurrentPhase = ESinglePlayScenarioPhase::Inactive;
	bool bChanged = false;
	bool bRequestRestart = false;
};

class PROJECTSINGLEPLAY_API FSinglePlayScenarioStateMachine
{
public:
	void Start();
	FSinglePlayScenarioTransition Evaluate(
		const FSinglePlayScenarioObservation& Observation,
		double RequiredHydrationFraction = 0.20);
	ESinglePlayScenarioPhase GetPhase() const { return Phase; }

private:
	ESinglePlayScenarioPhase Phase = ESinglePlayScenarioPhase::Inactive;
};
