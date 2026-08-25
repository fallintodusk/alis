// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Misc/AutomationTest.h"
#include "ProjectLoadingTravelURL.h"
#include "Engine/EngineBaseTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectLoadingTravelURLTest,
	"ProjectLoading.Unit.TravelURL.Provenance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProjectLoadingTravelURLTest::RunTest(const FString& Parameters)
{
	TMap<FString, FString> Options;
	Options.Add(TEXT("Mode"), TEXT("Medium"));
	Options.Add(TEXT("ProjectLoadingRoute"), TEXT("spoofed"));
	Options.Add(TEXT("game"), TEXT("/Script/ProjectSinglePlay.SinglePlayerGameMode"));
	const FString URL = ProjectLoadingTravelURL::Build(TEXT("/Project/TestMap"), Options);

	TestTrue(TEXT("The route has authoritative ProjectLoading provenance."), URL.Contains(TEXT("?ProjectLoadingRoute=1")));
	TestFalse(TEXT("Caller provenance cannot override the owner."), URL.Contains(TEXT("spoofed")));
	TestTrue(TEXT("Normal custom options survive."), URL.Contains(TEXT("?Mode=Medium")));
	TestTrue(TEXT("The game override remains last."), URL.EndsWith(TEXT("?game=/Script/ProjectSinglePlay.SinglePlayerGameMode")));
	const FURL OwnedRoute(nullptr, *URL, TRAVEL_Absolute);
	const FURL DirectRoute(nullptr, TEXT("/Project/TestMap?Mode=Medium"), TRAVEL_Absolute);
	TestTrue(TEXT("A ProjectLoading destination recognizes its provenance."), ProjectLoadingTravelURL::HasProvenance(OwnedRoute));
	TestFalse(TEXT("A direct map route has no ProjectLoading provenance."), ProjectLoadingTravelURL::HasProvenance(DirectRoute));
	return true;
}

#endif
