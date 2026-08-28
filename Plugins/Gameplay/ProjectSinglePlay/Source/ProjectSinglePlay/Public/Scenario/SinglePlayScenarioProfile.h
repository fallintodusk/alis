// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

struct FSinglePlayScenarioProfile
{
	FName ScenarioId;
	FName CacheActorTag;
	FName ShelterActorTag;
	FName RequiredRationObjectId;
	double RequiredHydrationFraction = 0.0;
	double CacheInteractionRadius = 0.0;
	double ShelterInteractionRadius = 0.0;
	double FailureRestartDelaySeconds = 0.0;
	FString StartMessage;
	FString RecoveryMessage;
	FString RecoveredMessage;
	FString SuccessMessage;
	FString FailureMessage;
};

namespace FSinglePlayScenarioProfileLoader
{
#if WITH_DEV_AUTOMATION_TESTS
	PROJECTSINGLEPLAY_API bool ParseForTests(
		FName ScenarioId,
		const FString& Json,
		FSinglePlayScenarioProfile& OutProfile,
		FString& OutError);
#endif

	PROJECTSINGLEPLAY_API bool Load(
		FName ScenarioId,
		FSinglePlayScenarioProfile& OutProfile,
		FString& OutError);
}
