// Copyright ALIS. All Rights Reserved.

/**
 * Unit Tests for Load Phase System
 *
 * Tests verify phase state tracking, progress calculation, and phase transitions.
 */

#include "Types/ProjectLoadPhaseState.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Test: Phase State Initialization
 * Verifies phase state initializes with correct defaults
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectLoadPhaseStateInitTest,
	"ProjectLoading.Phase.Initialization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectLoadPhaseStateInitTest::RunTest(const FString& Parameters)
{
	// Test default constructor
	FLoadPhaseState PhaseState;
	TestEqual(TEXT("Default phase should be None"), PhaseState.Phase, ELoadPhase::None);
	TestEqual(TEXT("Default state should be Pending"), PhaseState.State, EPhaseState::Pending);
	TestEqual(TEXT("Default progress should be 0.0"), PhaseState.Progress, 0.0f);
	TestEqual(TEXT("Default StartTime should be 0.0"), PhaseState.StartTime, 0.0);
	TestEqual(TEXT("Default EndTime should be 0.0"), PhaseState.EndTime, 0.0);
	TestEqual(TEXT("Default ErrorCode should be 0"), PhaseState.ErrorCode, 0);

	// Test constructor with phase
	FLoadPhaseState ResolvePhase(ELoadPhase::ResolveAssets);
	TestEqual(TEXT("Phase should be ResolveAssets"), ResolvePhase.Phase, ELoadPhase::ResolveAssets);
	TestEqual(TEXT("State should still be Pending"), ResolvePhase.State, EPhaseState::Pending);

	return true;
}

/**
 * Test: Phase State Transitions
 * Verifies phase state changes work correctly
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectLoadPhaseStateTransitionsTest,
	"ProjectLoading.Phase.StateTransitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectLoadPhaseStateTransitionsTest::RunTest(const FString& Parameters)
{
	FLoadPhaseState PhaseState(ELoadPhase::ResolveAssets);

	// Pending → InProgress
	PhaseState.State = EPhaseState::Pending;
	TestFalse(TEXT("Should not be in progress"), PhaseState.IsInProgress());
	TestFalse(TEXT("Should not be complete"), PhaseState.IsComplete());

	PhaseState.State = EPhaseState::InProgress;
	TestTrue(TEXT("Should be in progress"), PhaseState.IsInProgress());
	TestFalse(TEXT("Should not be complete"), PhaseState.IsComplete());

	// InProgress → Completed
	PhaseState.State = EPhaseState::Completed;
	TestFalse(TEXT("Should not be in progress"), PhaseState.IsInProgress());
	TestTrue(TEXT("Should be complete"), PhaseState.IsComplete());
	TestFalse(TEXT("Should not be failed"), PhaseState.IsFailed());

	// InProgress → Failed
	PhaseState.State = EPhaseState::InProgress;
	PhaseState.State = EPhaseState::Failed;
	TestTrue(TEXT("Should be failed"), PhaseState.IsFailed());
	TestFalse(TEXT("Should not be complete"), PhaseState.IsComplete());

	// Skipped state
	PhaseState.State = EPhaseState::Skipped;
	TestTrue(TEXT("Skipped should count as complete"), PhaseState.IsComplete());

	return true;
}

/**
 * Test: Phase Progress Tracking
 * Verifies progress values are tracked correctly
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectLoadPhaseProgressTest,
	"ProjectLoading.Phase.Progress",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectLoadPhaseProgressTest::RunTest(const FString& Parameters)
{
	FLoadPhaseState PhaseState(ELoadPhase::PreloadCriticalAssets);

	// Test progress from 0% to 100%
	PhaseState.Progress = 0.0f;
	TestEqual(TEXT("Progress should be 0%"), PhaseState.Progress, 0.0f);

	PhaseState.Progress = 0.25f;
	TestEqual(TEXT("Progress should be 25%"), PhaseState.Progress, 0.25f);

	PhaseState.Progress = 0.5f;
	TestEqual(TEXT("Progress should be 50%"), PhaseState.Progress, 0.5f);

	PhaseState.Progress = 0.75f;
	TestEqual(TEXT("Progress should be 75%"), PhaseState.Progress, 0.75f);

	PhaseState.Progress = 1.0f;
	TestEqual(TEXT("Progress should be 100%"), PhaseState.Progress, 1.0f);

	return true;
}

/**
 * Test: Phase Duration Calculation
 * Verifies phase duration is calculated correctly
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectLoadPhaseDurationTest,
	"ProjectLoading.Phase.Duration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectLoadPhaseDurationTest::RunTest(const FString& Parameters)
{
	FLoadPhaseState PhaseState(ELoadPhase::Travel);

	// Initial duration should be 0
	TestEqual(TEXT("Initial duration should be 0"), PhaseState.GetDuration(), 0.0);

	// Set start time
	PhaseState.StartTime = 100.0;
	TestEqual(TEXT("Duration should still be 0 without end time"), PhaseState.GetDuration(), 0.0);

	// Set end time
	PhaseState.EndTime = 105.5;
	TestEqual(TEXT("Duration should be 5.5 seconds"), PhaseState.GetDuration(), 5.5);

	// Test different durations
	PhaseState.StartTime = 0.0;
	PhaseState.EndTime = 1.0;
	TestEqual(TEXT("Duration should be 1 second"), PhaseState.GetDuration(), 1.0);

	PhaseState.StartTime = 50.0;
	PhaseState.EndTime = 50.250;
	TestEqual(TEXT("Duration should be 0.250 seconds"), PhaseState.GetDuration(), 0.250);

	return true;
}

/**
 * Test: All Phase Types
 * Verifies all 6 loading phases are defined
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectLoadPhaseTypesTest,
	"ProjectLoading.Phase.Types",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectLoadPhaseTypesTest::RunTest(const FString& Parameters)
{
	// Verify all 6 phases exist
	FLoadPhaseState Phase1(ELoadPhase::ResolveAssets);
	TestEqual(TEXT("ResolveAssets phase should exist"), Phase1.Phase, ELoadPhase::ResolveAssets);

	FLoadPhaseState Phase2(ELoadPhase::MountContent);
	TestEqual(TEXT("MountContent phase should exist"), Phase2.Phase, ELoadPhase::MountContent);

	FLoadPhaseState Phase3(ELoadPhase::PreloadCriticalAssets);
	TestEqual(TEXT("PreloadCriticalAssets phase should exist"), Phase3.Phase, ELoadPhase::PreloadCriticalAssets);

	FLoadPhaseState Phase4(ELoadPhase::ActivateFeatures);
	TestEqual(TEXT("ActivateFeatures phase should exist"), Phase4.Phase, ELoadPhase::ActivateFeatures);

	FLoadPhaseState Phase5(ELoadPhase::Travel);
	TestEqual(TEXT("Travel phase should exist"), Phase5.Phase, ELoadPhase::Travel);

	FLoadPhaseState Phase6(ELoadPhase::Warmup);
	TestEqual(TEXT("Warmup phase should exist"), Phase6.Phase, ELoadPhase::Warmup);

	// Verify Complete phase
	FLoadPhaseState Phase7(ELoadPhase::Complete);
	TestEqual(TEXT("Complete phase should exist"), Phase7.Phase, ELoadPhase::Complete);

	return true;
}

/**
 * Test: Phase State Types
 * Verifies all phase state types are defined
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectLoadPhaseStateTypesTest,
	"ProjectLoading.Phase.StateTypes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectLoadPhaseStateTypesTest::RunTest(const FString& Parameters)
{
	FLoadPhaseState PhaseState;

	// Test all state types
	PhaseState.State = EPhaseState::Pending;
	TestEqual(TEXT("Pending state should exist"), PhaseState.State, EPhaseState::Pending);

	PhaseState.State = EPhaseState::InProgress;
	TestEqual(TEXT("InProgress state should exist"), PhaseState.State, EPhaseState::InProgress);

	PhaseState.State = EPhaseState::Completed;
	TestEqual(TEXT("Completed state should exist"), PhaseState.State, EPhaseState::Completed);

	PhaseState.State = EPhaseState::Failed;
	TestEqual(TEXT("Failed state should exist"), PhaseState.State, EPhaseState::Failed);

	PhaseState.State = EPhaseState::Skipped;
	TestEqual(TEXT("Skipped state should exist"), PhaseState.State, EPhaseState::Skipped);

	return true;
}

/**
 * Test: Phase Error Tracking
 * Verifies error message and error code tracking
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectLoadPhaseErrorTrackingTest,
	"ProjectLoading.Phase.ErrorTracking",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectLoadPhaseErrorTrackingTest::RunTest(const FString& Parameters)
{
	FLoadPhaseState PhaseState(ELoadPhase::ResolveAssets);

	// Initial error state
	TestTrue(TEXT("ErrorMessage should be empty"), PhaseState.ErrorMessage.IsEmpty());
	TestEqual(TEXT("ErrorCode should be 0"), PhaseState.ErrorCode, 0);

	// Set error
	PhaseState.State = EPhaseState::Failed;
	PhaseState.ErrorMessage = FText::FromString(TEXT("Manifest not found"));
	PhaseState.ErrorCode = 404;

	TestFalse(TEXT("ErrorMessage should not be empty"), PhaseState.ErrorMessage.IsEmpty());
	TestEqual(TEXT("ErrorCode should be 404"), PhaseState.ErrorCode, 404);
	TestTrue(TEXT("Phase should be failed"), PhaseState.IsFailed());

	return true;
}

/**
 * Test: Phase Status Messages
 * Verifies status message tracking
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectLoadPhaseStatusMessageTest,
	"ProjectLoading.Phase.StatusMessage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectLoadPhaseStatusMessageTest::RunTest(const FString& Parameters)
{
	FLoadPhaseState PhaseState(ELoadPhase::PreloadCriticalAssets);

	// Initial status
	TestTrue(TEXT("StatusMessage should be empty"), PhaseState.StatusMessage.IsEmpty());

	// Set status messages
	PhaseState.StatusMessage = FText::FromString(TEXT("Loading textures..."));
	TestFalse(TEXT("StatusMessage should not be empty"), PhaseState.StatusMessage.IsEmpty());

	PhaseState.StatusMessage = FText::FromString(TEXT("Loading audio..."));
	TestEqual(TEXT("StatusMessage should be updated"),
		PhaseState.StatusMessage.ToString(),
		FString(TEXT("Loading audio...")));

	return true;
}

/**
 * Test: Phase ToString
 * Verifies debug string generation
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectLoadPhaseToStringTest,
	"ProjectLoading.Phase.ToString",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectLoadPhaseToStringTest::RunTest(const FString& Parameters)
{
	FLoadPhaseState PhaseState(ELoadPhase::Travel);
	PhaseState.State = EPhaseState::InProgress;
	PhaseState.Progress = 0.5f;

	FString DebugString = PhaseState.ToString();

	// Should contain phase name and progress
	TestTrue(TEXT("Debug string should contain phase info"), !DebugString.IsEmpty());
	TestTrue(TEXT("Debug string should contain progress"), DebugString.Contains(TEXT("50")));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
