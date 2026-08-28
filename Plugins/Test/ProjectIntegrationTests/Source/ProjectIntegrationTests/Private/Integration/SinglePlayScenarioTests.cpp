// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Interfaces/IVitalsReadOnly.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "ProjectPaths.h"
#include "Scenario/SinglePlayScenarioPolicy.h"
#include "Scenario/SinglePlayScenarioProfile.h"
#include "Scenario/SinglePlayScenarioStateMachine.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSinglePlayScenarioPolicyTest,
	"ProjectIntegrationTests.SinglePlay.Scenario.Policy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSinglePlayScenarioPolicyTest::RunTest(const FString& Parameters)
{
	const FSinglePlayScenarioSelection Missing =
		FSinglePlayScenarioPolicy::Resolve(TEXT("/Game/Map?Mode=SinglePlay"));
	TestEqual(TEXT("Missing Scenario is silent default."),
		Missing.Result, ESinglePlayScenarioParseResult::Absent);
	TestFalse(TEXT("Missing Scenario does not enable a scenario."), Missing.IsEnabled());

	const FSinglePlayScenarioSelection WrongCase =
		FSinglePlayScenarioPolicy::Resolve(TEXT("/Game/Map?scenario=UrbanSurvivalProofV1"));
	TestEqual(TEXT("Wrong-case key behaves as absence."),
		WrongCase.Result, ESinglePlayScenarioParseResult::Absent);

	const FSinglePlayScenarioSelection Unknown =
		FSinglePlayScenarioPolicy::Resolve(TEXT("/Game/Map?Scenario=Unknown"));
	TestEqual(TEXT("Unknown Scenario fails closed."),
		Unknown.Result, ESinglePlayScenarioParseResult::Unknown);
	TestFalse(TEXT("Unknown Scenario does not enable a scenario."), Unknown.IsEnabled());

	const FSinglePlayScenarioSelection Supported =
		FSinglePlayScenarioPolicy::Resolve(
			TEXT("/Game/Map?Scenario=UrbanSurvivalProofV1"));
	TestEqual(TEXT("Supported Scenario is selected exactly."),
		Supported.Result, ESinglePlayScenarioParseResult::Selected);
	TestEqual(TEXT("Supported Scenario preserves stable identity."),
		Supported.ScenarioId, FName(TEXT("UrbanSurvivalProofV1")));
	TestTrue(TEXT("Supported Scenario enables the runner."), Supported.IsEnabled());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSinglePlayScenarioProfileTest,
	"ProjectIntegrationTests.SinglePlay.Scenario.ProfileSchema",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSinglePlayScenarioProfileTest::RunTest(const FString& Parameters)
{
	FSinglePlayScenarioProfile Profile;
	FString Error;
	TestTrue(TEXT("Tracked survival-proof profile loads through ProjectSinglePlay."),
		FSinglePlayScenarioProfileLoader::Load(
			FName(TEXT("UrbanSurvivalProofV1")), Profile, Error));
	TestTrue(TEXT("Profile load emits no error."), Error.IsEmpty());
	TestEqual(TEXT("Profile identity is exact."),
		Profile.ScenarioId, FName(TEXT("UrbanSurvivalProofV1")));
	TestEqual(TEXT("Cache tag is generic."),
		Profile.CacheActorTag, FName(TEXT("ProjectScenario.Anchor.Cache")));
	TestEqual(TEXT("Shelter tag is generic."),
		Profile.ShelterActorTag, FName(TEXT("ProjectScenario.Anchor.Shelter")));
	TestEqual(TEXT("Required ration is exact."),
		Profile.RequiredRationObjectId, FName(TEXT("EmergencyRation")));
	TestTrue(TEXT("Recovery threshold matches the Empty hysteresis boundary."),
		Profile.RequiredHydrationFraction >= 0.20);
	TestTrue(TEXT("Interaction radii are positive."),
		Profile.CacheInteractionRadius > 0.0 && Profile.ShelterInteractionRadius > 0.0);
	TestTrue(TEXT("Cache discovery explains the inventory recovery action."),
		Profile.RecoveryMessage.Contains(TEXT("Press I")) &&
		Profile.RecoveryMessage.Contains(TEXT("pouch")) &&
		Profile.RecoveryMessage.Contains(TEXT("Right-click water")));

	const FString Path = FProjectPaths::GetPluginDataDir(TEXT("ProjectSinglePlay")) /
		TEXT("Scenarios/UrbanSurvivalProofV1.json");
	FString Json;
	TestTrue(TEXT("Tracked profile JSON is readable for strict runtime rejection tests."),
		FFileHelper::LoadFileToString(Json, *Path));
	const FString WrongSchema = Json.Replace(
		TEXT("../Schemas/single_play_scenario.schema.json"),
		TEXT("single_play_scenario.schema.json"));
	TestFalse(TEXT("Runtime rejects a non-canonical schema identity."),
		FSinglePlayScenarioProfileLoader::ParseForTests(
			FName(TEXT("UrbanSurvivalProofV1")), WrongSchema, Profile, Error));
	const FString WrongVersion = Json.Replace(
		TEXT("\"schemaVersion\": 2"), TEXT("\"schemaVersion\": 3"));
	TestFalse(TEXT("Runtime rejects an unsupported schema version."),
		FSinglePlayScenarioProfileLoader::ParseForTests(
			FName(TEXT("UrbanSurvivalProofV1")), WrongVersion, Profile, Error));
	const FString UnknownField = Json.Replace(
		TEXT("\n}"), TEXT(",\n  \"futureMagic\": true\n}"));
	TestFalse(TEXT("Runtime rejects an unknown root field."),
		FSinglePlayScenarioProfileLoader::ParseForTests(
			FName(TEXT("UrbanSurvivalProofV1")), UnknownField, Profile, Error));
	const TArray<FName> UnsafeScenarioIds = {
		FName(TEXT("../Foo")),
		FName(TEXT("Foo/Bar")),
		FName(TEXT("Foo:Bar")),
		FName(TEXT("Foo*Bar")),
		FName(TEXT("1Foo"))};
	for (const FName UnsafeScenarioId : UnsafeScenarioIds)
	{
		TestFalse(FString::Printf(TEXT("File loading rejects unsafe scenario identity '%s'."),
			*UnsafeScenarioId.ToString()), FSinglePlayScenarioProfileLoader::Load(
				UnsafeScenarioId, Profile, Error));
		TestTrue(TEXT("Unsafe identity rejection names the owner-local boundary."),
			Error.Contains(TEXT("safe owner-local grammar")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSinglePlayScenarioStateMachineTest,
	"ProjectIntegrationTests.SinglePlay.Scenario.StateMachine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSinglePlayScenarioStateMachineTest::RunTest(const FString& Parameters)
{
	FSinglePlayScenarioStateMachine Machine;
	Machine.Start();
	TestEqual(TEXT("Scenario starts at the cache-search phase."),
		Machine.GetPhase(), ESinglePlayScenarioPhase::SearchCache);

	FSinglePlayScenarioObservation Observation;
	Observation.bAtCache = true;
	FSinglePlayScenarioTransition Transition = Machine.Evaluate(Observation);
	TestTrue(TEXT("Finding the cache advances the scenario."), Transition.bChanged);
	TestEqual(TEXT("Cache discovery exposes the recovery phase."),
		Machine.GetPhase(), ESinglePlayScenarioPhase::Recover);

	Observation = {};
	Observation.HydrationFraction = 0.85 / 3.0;
	Observation.bHasRequiredRation = true;
	Transition = Machine.Evaluate(Observation);
	TestTrue(TEXT("Hydration plus ration advances toward shelter."), Transition.bChanged);
	TestEqual(TEXT("Recovered player is routed to shelter."),
		Machine.GetPhase(), ESinglePlayScenarioPhase::ReachShelter);

	Observation.bAtShelter = true;
	Transition = Machine.Evaluate(Observation);
	TestTrue(TEXT("Valid shelter arrival succeeds."), Transition.bChanged);
	TestEqual(TEXT("Valid shelter arrival is terminal success."),
		Machine.GetPhase(), ESinglePlayScenarioPhase::Succeeded);
	TestFalse(TEXT("Success does not request restart."), Transition.bRequestRestart);

	FSinglePlayScenarioStateMachine FailureMachine;
	FailureMachine.Start();
	Observation = {};
	Observation.bAtShelter = true;
	Transition = FailureMachine.Evaluate(Observation);
	TestEqual(TEXT("Premature shelter arrival fails closed."),
		FailureMachine.GetPhase(), ESinglePlayScenarioPhase::Failed);
	TestTrue(TEXT("Failure requests the existing reload path."),
		Transition.bRequestRestart);

	FVitalsReadOnlySnapshot Snapshot;
	Snapshot.Hydration = 0.85f;
	Snapshot.MaxHydration = 3.0;
	TestTrue(TEXT("Read-only vitals projection exposes normalized hydration."),
		Snapshot.GetHydrationFraction() > 0.20);
	return true;
}

#endif
