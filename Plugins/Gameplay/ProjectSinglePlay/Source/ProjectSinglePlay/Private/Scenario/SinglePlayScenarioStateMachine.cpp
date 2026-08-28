// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Scenario/SinglePlayScenarioStateMachine.h"

void FSinglePlayScenarioStateMachine::Start()
{
	Phase = ESinglePlayScenarioPhase::SearchCache;
}

FSinglePlayScenarioTransition FSinglePlayScenarioStateMachine::Evaluate(
	const FSinglePlayScenarioObservation& Observation,
	double RequiredHydrationFraction)
{
	FSinglePlayScenarioTransition Result;
	Result.PreviousPhase = Phase;

	if (Phase != ESinglePlayScenarioPhase::Succeeded &&
		Phase != ESinglePlayScenarioPhase::Failed &&
		Observation.bAtShelter)
	{
		const bool bRecovered = Observation.HydrationFraction > RequiredHydrationFraction;
		Phase = bRecovered && Observation.bHasRequiredRation
			? ESinglePlayScenarioPhase::Succeeded
			: ESinglePlayScenarioPhase::Failed;
		Result.bRequestRestart = Phase == ESinglePlayScenarioPhase::Failed;
	}
	else if (Phase == ESinglePlayScenarioPhase::SearchCache && Observation.bAtCache)
	{
		Phase = ESinglePlayScenarioPhase::Recover;
	}
	else if (Phase == ESinglePlayScenarioPhase::Recover &&
		Observation.HydrationFraction > RequiredHydrationFraction &&
		Observation.bHasRequiredRation)
	{
		Phase = ESinglePlayScenarioPhase::ReachShelter;
	}

	Result.CurrentPhase = Phase;
	Result.bChanged = Result.PreviousPhase != Result.CurrentPhase;
	return Result;
}
