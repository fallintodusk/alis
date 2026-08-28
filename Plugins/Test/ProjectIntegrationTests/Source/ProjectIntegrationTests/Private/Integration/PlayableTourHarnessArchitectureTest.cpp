// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlayableTourHarnessArchitectureTest,
	"ProjectIntegrationTests.World.PlayableTour.RealInputArchitecture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPlayableTourHarnessArchitectureTest::RunTest(const FString& Parameters)
{
	FString Source;
	const FString SourcePath = FPaths::Combine(
		FPaths::ProjectPluginsDir(),
		TEXT("World/ProjectWorld/Source/ProjectWorld/Private/Presentation/ProjectWorldPlayableTourDriver.cpp"));
	TestTrue(TEXT("The existing packaged World harness has a focused playable-tour driver."),
		FFileHelper::LoadFileToString(Source, *SourcePath));
	TestTrue(TEXT("Playable-tour displacement enters through PlayerController input."),
		Source.Contains(TEXT("InputKey(FInputKeyEventArgs::CreateSimulated")));
	TestFalse(TEXT("Playable-tour acceptance never mutates transforms directly."),
		Source.Contains(TEXT("SetActorLocation")) || Source.Contains(TEXT("TeleportTo")));
	TestFalse(TEXT("The harness never bypasses production handlers with movement input."),
		Source.Contains(TEXT("AddMovementInput")));

	FString Runner;
	const FString RunnerPath = FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("scripts/ue/world/test/performance/run_kazan_playable_tour.ps1"));
	TestTrue(TEXT("The playable-tour package runner is readable."),
		FFileHelper::LoadFileToString(Runner, *RunnerPath));
	TestTrue(TEXT("The operator package is published as Candidate."),
		Runner.Contains(TEXT("KazanPlayableTour\\Candidate")));
	TestTrue(TEXT("One recoverable prior candidate is retained."),
		Runner.Contains(TEXT("KazanPlayableTour\\PreviousCandidate")));
	TestTrue(TEXT("Unattended acceptance disables unused network messaging."),
		Runner.Contains(TEXT("'-NoMessaging'")));
	TestFalse(TEXT("Technical acceptance does not pre-promote the package to Current."),
		Runner.Contains(TEXT("KazanPlayableTour\\Current")));
	return true;
}

#endif
