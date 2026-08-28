// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Misc/AutomationTest.h"
#include "Presentation/ProjectWorldPlayableTourResidency.h"
#include "WorldPartition/WorldPartitionRuntimeCell.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldPlayableTourResidencyTest,
	"ProjectWorld.PlayableTour.Streaming.CenterCellResidency",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldPlayableTourResidencyTest::RunTest(const FString& Parameters)
{
	const FGuid CenterCell = FGuid::NewGuid();
	const FGuid UnrelatedCell = FGuid::NewGuid();
	FProjectWorldPlayableTourResidency Residency;
	FString Error;
	TestTrue(TEXT("A non-empty initial center-cell census freezes."),
		Residency.FreezeCenterCellIds({CenterCell}, Error));

	Residency.ObserveCellState(
		UnrelatedCell,
		EWorldPartitionRuntimeCellState::Unloaded,
		true,
		false);
	Residency.ObserveCellState(
		UnrelatedCell,
		EWorldPartitionRuntimeCellState::Loaded,
		true,
		true);
	TestFalse(TEXT("An unrelated cell cycle cannot satisfy playable-tour streaming."),
		Residency.HasCompleteCycle());

	Residency.ObserveCellState(
		CenterCell,
		EWorldPartitionRuntimeCellState::Unloaded,
		false,
		false);
	TestFalse(TEXT("A center unload before the edge is not accepted."),
		Residency.HasCompleteCycle());

	Residency.ObserveCellState(
		CenterCell,
		EWorldPartitionRuntimeCellState::Unloaded,
		true,
		false);
	TestFalse(TEXT("The center cell must reload after the real-input return."),
		Residency.HasCompleteCycle());

	Residency.ObserveCellState(
		CenterCell,
		EWorldPartitionRuntimeCellState::Loaded,
		true,
		true);
	TestTrue(TEXT("The same center cell unloading at edge and reloading on return passes."),
		Residency.HasCompleteCycle());
	TestEqual(TEXT("Exactly one center cell unloaded."), Residency.GetUnloadedCount(), 1);
	TestEqual(TEXT("Exactly that center cell reloaded."), Residency.GetReloadedCount(), 1);
	return true;
}

#endif
