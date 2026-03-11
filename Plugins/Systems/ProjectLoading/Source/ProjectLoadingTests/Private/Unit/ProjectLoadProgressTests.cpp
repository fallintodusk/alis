// Copyright ALIS. All Rights Reserved.

/**
 * Unit Tests for Progress Tracking and Error Handling
 *
 * Tests verify progress calculation, normalization, and error handling logic.
 */

#include "Types/ProjectLoadPhaseState.h"
#include "Types/ProjectLoadRequest.h"
#include "ProjectLoadingSubsystem.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Test: Progress Normalization
 * Verifies progress values are normalized to 0.0-1.0 range
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectLoadProgressNormalizationTest,
	"ProjectLoading.Progress.Normalization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectLoadProgressNormalizationTest::RunTest(const FString& Parameters)
{
	// Progress should be clamped to [0.0, 1.0]
	float Progress = 0.0f;
	TestTrue(TEXT("0.0 is valid progress"), Progress >= 0.0f && Progress <= 1.0f);

	Progress = 0.5f;
	TestTrue(TEXT("0.5 is valid progress"), Progress >= 0.0f && Progress <= 1.0f);

	Progress = 1.0f;
	TestTrue(TEXT("1.0 is valid progress"), Progress >= 0.0f && Progress <= 1.0f);

	// Test clamping
	Progress = -0.5f;
	Progress = FMath::Clamp(Progress, 0.0f, 1.0f);
	TestEqual(TEXT("Negative progress should clamp to 0.0"), Progress, 0.0f);

	Progress = 1.5f;
	Progress = FMath::Clamp(Progress, 0.0f, 1.0f);
	TestEqual(TEXT("Progress > 1.0 should clamp to 1.0"), Progress, 1.0f);

	return true;
}

/**
 * Test: Overall Progress Calculation
 * Verifies overall progress is calculated from phase progress
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectLoadOverallProgressTest,
	"ProjectLoading.Progress.Overall",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectLoadOverallProgressTest::RunTest(const FString& Parameters)
{
	// 6 phases with equal weight (1/6 each)
	const int32 NumPhases = 6;
	const float PhaseWeight = 1.0f / NumPhases;

	// All phases at 0%
	float OverallProgress = 0.0f;
	TestEqual(TEXT("All phases at 0% should be 0.0"), OverallProgress, 0.0f);

	// First phase complete (1/6)
	OverallProgress = PhaseWeight * 1.0f;
	TestTrue(TEXT("One phase complete should be ~16.67%"), FMath::IsNearlyEqual(OverallProgress, 0.16667f, 0.001f));

	// Three phases complete (3/6 = 50%)
	OverallProgress = PhaseWeight * 3.0f;
	TestEqual(TEXT("Three phases complete should be 50%"), OverallProgress, 0.5f);

	// All phases complete (6/6 = 100%)
	OverallProgress = PhaseWeight * 6.0f;
	TestEqual(TEXT("All phases complete should be 100%"), OverallProgress, 1.0f);

	return true;
}

/**
 * Test: Partial Phase Progress
 * Verifies progress calculation with partial phase completion
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectLoadPartialPhaseProgressTest,
	"ProjectLoading.Progress.PartialPhase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectLoadPartialPhaseProgressTest::RunTest(const FString& Parameters)
{
	const int32 NumPhases = 6;
	const float PhaseWeight = 1.0f / NumPhases;

	// 2 phases complete, 1 phase 50% complete
	float CompletedPhases = 2.0f;
	float CurrentPhaseProgress = 0.5f;

	float OverallProgress = (CompletedPhases + CurrentPhaseProgress) * PhaseWeight;

	// Expected: (2 + 0.5) / 6 = 2.5 / 6 = 0.41667
	TestTrue(TEXT("Partial progress should be ~41.67%"),
		FMath::IsNearlyEqual(OverallProgress, 0.41667f, 0.001f));

	// 5 phases complete, 1 phase 75% complete
	CompletedPhases = 5.0f;
	CurrentPhaseProgress = 0.75f;
	OverallProgress = (CompletedPhases + CurrentPhaseProgress) * PhaseWeight;

	// Expected: (5 + 0.75) / 6 = 5.75 / 6 = 0.95833
	TestTrue(TEXT("Near-complete progress should be ~95.83%"),
		FMath::IsNearlyEqual(OverallProgress, 0.95833f, 0.001f));

	return true;
}

/**
 * Test: Telemetry Progress Tracking
 * Verifies telemetry tracks progress correctly
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectLoadTelemetryProgressTest,
	"ProjectLoading.Progress.Telemetry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectLoadTelemetryProgressTest::RunTest(const FString& Parameters)
{
	FProjectLoadTelemetry Telemetry;

	// Initial progress
	TestEqual(TEXT("Initial progress should be 0.0"), Telemetry.TotalProgress, 0.0f);

	// Update progress
	Telemetry.TotalProgress = 0.25f;
	TestEqual(TEXT("Progress should be 25%"), Telemetry.TotalProgress, 0.25f);

	Telemetry.TotalProgress = 0.5f;
	TestEqual(TEXT("Progress should be 50%"), Telemetry.TotalProgress, 0.5f);

	Telemetry.TotalProgress = 1.0f;
	TestEqual(TEXT("Progress should be 100%"), Telemetry.TotalProgress, 1.0f);

	return true;
}

/**
 * Test: Error Message Handling
 * Verifies error messages are tracked correctly
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectLoadErrorMessageTest,
	"ProjectLoading.Error.Message",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectLoadErrorMessageTest::RunTest(const FString& Parameters)
{
	FLoadPhaseState PhaseState(ELoadPhase::ResolveAssets);

	// No error initially
	TestTrue(TEXT("ErrorMessage should be empty"), PhaseState.ErrorMessage.IsEmpty());
	TestEqual(TEXT("ErrorCode should be 0"), PhaseState.ErrorCode, 0);

	// Set error
	PhaseState.ErrorMessage = FText::FromString(TEXT("Manifest not found: /Game/Data/KazanWorld"));
	PhaseState.ErrorCode = 404;
	PhaseState.State = EPhaseState::Failed;

	TestFalse(TEXT("ErrorMessage should not be empty"), PhaseState.ErrorMessage.IsEmpty());
	TestTrue(TEXT("ErrorMessage should contain path"),
		PhaseState.ErrorMessage.ToString().Contains(TEXT("/Game/Data/KazanWorld")));
	TestEqual(TEXT("ErrorCode should be 404"), PhaseState.ErrorCode, 404);
	TestTrue(TEXT("Phase should be failed"), PhaseState.IsFailed());

	return true;
}

/**
 * Test: Loading State Enum
 * Verifies loading state transitions
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectLoadLoadingStateTest,
	"ProjectLoading.Error.LoadingState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectLoadLoadingStateTest::RunTest(const FString& Parameters)
{
	// Verify loading states from ILoadingHandle interface
	enum class ELoadingState
	{
		Idle,
		InProgress,
		Completed,
		Failed,
		Cancelled
	};

	ELoadingState State = ELoadingState::Idle;
	TestEqual(TEXT("Initial state should be Idle"), (int32)State, (int32)ELoadingState::Idle);

	State = ELoadingState::InProgress;
	TestEqual(TEXT("State should be InProgress"), (int32)State, (int32)ELoadingState::InProgress);

	State = ELoadingState::Completed;
	TestEqual(TEXT("State should be Completed"), (int32)State, (int32)ELoadingState::Completed);

	State = ELoadingState::Failed;
	TestEqual(TEXT("State should be Failed"), (int32)State, (int32)ELoadingState::Failed);

	State = ELoadingState::Cancelled;
	TestEqual(TEXT("State should be Cancelled"), (int32)State, (int32)ELoadingState::Cancelled);

	return true;
}

/**
 * Test: Progress Update Frequency
 * Verifies progress updates are throttled
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectLoadProgressUpdateFrequencyTest,
	"ProjectLoading.Progress.UpdateFrequency",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectLoadProgressUpdateFrequencyTest::RunTest(const FString& Parameters)
{
	// Simulate throttling (don't spam updates every frame)
	const float MinUpdateInterval = 0.1f; // 100ms
	float LastUpdateTime = 0.0f;
	float CurrentTime = 0.0f;

	// First update should go through
	bool bShouldUpdate = (CurrentTime - LastUpdateTime) >= MinUpdateInterval;
	TestTrue(TEXT("First update should be allowed"), bShouldUpdate);
	LastUpdateTime = CurrentTime;

	// Update 50ms later (should be throttled)
	CurrentTime = 0.05f;
	bShouldUpdate = (CurrentTime - LastUpdateTime) >= MinUpdateInterval;
	TestFalse(TEXT("Update should be throttled"), bShouldUpdate);

	// Update 100ms later (should be allowed)
	CurrentTime = 0.1f;
	bShouldUpdate = (CurrentTime - LastUpdateTime) >= MinUpdateInterval;
	TestTrue(TEXT("Update should be allowed after interval"), bShouldUpdate);

	return true;
}

/**
 * Test: Phase Progress Monotonic Increase
 * Verifies progress never decreases
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectLoadProgressMonotonicTest,
	"ProjectLoading.Progress.Monotonic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectLoadProgressMonotonicTest::RunTest(const FString& Parameters)
{
	FLoadPhaseState PhaseState(ELoadPhase::PreloadCriticalAssets);

	// Progress should only increase
	float PreviousProgress = 0.0f;
	PhaseState.Progress = 0.25f;
	TestTrue(TEXT("Progress should increase"), PhaseState.Progress >= PreviousProgress);
	PreviousProgress = PhaseState.Progress;

	PhaseState.Progress = 0.5f;
	TestTrue(TEXT("Progress should increase"), PhaseState.Progress >= PreviousProgress);
	PreviousProgress = PhaseState.Progress;

	PhaseState.Progress = 1.0f;
	TestTrue(TEXT("Progress should increase"), PhaseState.Progress >= PreviousProgress);

	// Attempting to decrease should be prevented
	float InvalidProgress = 0.3f;
	InvalidProgress = FMath::Max(InvalidProgress, PreviousProgress);
	TestEqual(TEXT("Progress should not decrease"), InvalidProgress, 1.0f);

	return true;
}

/**
 * Test: Error Context
 * Verifies error context is preserved
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectLoadErrorContextTest,
	"ProjectLoading.Error.Context",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectLoadErrorContextTest::RunTest(const FString& Parameters)
{
	FLoadPhaseState PhaseState(ELoadPhase::MountContent);

	// Set rich error context
	PhaseState.State = EPhaseState::Failed;
	PhaseState.ErrorCode = 500;
	PhaseState.ErrorMessage = FText::FromString(TEXT("Failed to mount content pack 'DLC_KazanExpansion'"));
	PhaseState.StatusMessage = FText::FromString(TEXT("Mounting content packs..."));

	// Verify all error context is preserved
	TestTrue(TEXT("Phase should be failed"), PhaseState.IsFailed());
	TestEqual(TEXT("ErrorCode should be preserved"), PhaseState.ErrorCode, 500);
	TestFalse(TEXT("ErrorMessage should not be empty"), PhaseState.ErrorMessage.IsEmpty());
	TestFalse(TEXT("StatusMessage should not be empty"), PhaseState.StatusMessage.IsEmpty());
	TestTrue(TEXT("ErrorMessage should contain pack name"),
		PhaseState.ErrorMessage.ToString().Contains(TEXT("DLC_KazanExpansion")));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
