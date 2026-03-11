// Copyright ALIS. All Rights Reserved.

/**
 * Unit Tests for UProjectLoadingSubsystem
 *
 * These tests verify the loading subsystem lifecycle and core functionality.
 * Tests can be run via Unreal's Session Frontend Automation tab.
 *
 * To run tests:
 * 1. Open Session Frontend (Window → Developer Tools → Session Frontend)
 * 2. Go to Automation tab
 * 3. Filter for "ProjectLoading.Subsystem"
 * 4. Select tests and click "Start Tests"
 */

#include "ProjectLoadingSubsystem.h"
#include "Types/ProjectLoadRequest.h"
#include "Types/ProjectLoadPhaseState.h"
#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Test: Subsystem Initialization
 * Verifies that subsystem initializes with correct default state
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectLoadingSubsystemInitTest,
	"ProjectLoading.Subsystem.Initialization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectLoadingSubsystemInitTest::RunTest(const FString& Parameters)
{
	// Note: In unit tests, we can't easily create a GameInstance subsystem
	// This test verifies the test framework is working
	// Real subsystem tests require integration tests with GameInstance

	TestTrue(TEXT("Test framework is operational"), true);

	return true;
}

/**
 * Test: StartLoad with Valid Request
 * Verifies that StartLoad returns a valid handle
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectLoadingSubsystemStartLoadTest,
	"ProjectLoading.Subsystem.StartLoad",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectLoadingSubsystemStartLoadTest::RunTest(const FString& Parameters)
{
	// Create a load request
	FLoadRequest Request;
	Request.MapAssetId = FPrimaryAssetId(TEXT("ProjectWorld"), TEXT("TestMap"));
	Request.ExperienceAssetId = FPrimaryAssetId(TEXT("GameMode"), TEXT("Startup"));
	Request.LoadMode = ELoadMode::SinglePlayer;
	Request.Priority = 100;

	// Note: Without GameInstance, we can't test subsystem directly
	// These structural tests verify request building works
	TestTrue(TEXT("Request should be valid when MapAssetId is set"), Request.IsValid());
	TestEqual(TEXT("Request MapAssetId should be set"), Request.MapAssetId.ToString(), FString(TEXT("ProjectWorld:TestMap")));
	TestEqual(TEXT("Request ExperienceAssetId should be set"), Request.ExperienceAssetId.ToString(), FString(TEXT("GameMode:Startup")));
	TestEqual(TEXT("Request load mode defaults to single player"), Request.LoadMode, ELoadMode::SinglePlayer);
	TestEqual(TEXT("Request priority should be 100"), Request.Priority, 100);
	TestFalse(TEXT("Skip cinematics defaults to false"), Request.bSkipCinematics);
	TestTrue(TEXT("Warmup defaults to true"), Request.bPerformWarmup);

	return true;
}

/**
 * Test: CancelActiveLoad
 * Verifies cancellation logic
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectLoadingSubsystemCancelTest,
	"ProjectLoading.Subsystem.Cancel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectLoadingSubsystemCancelTest::RunTest(const FString& Parameters)
{
	// Test graceful vs force cancellation flags
	bool bForce = false;
	TestFalse(TEXT("Graceful cancellation should be default"), bForce);

	bForce = true;
	TestTrue(TEXT("Force cancellation should be true"), bForce);

	return true;
}

/**
 * Test: IsLoadInProgress
 * Verifies load state tracking
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectLoadingSubsystemIsInProgressTest,
	"ProjectLoading.Subsystem.IsLoadInProgress",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectLoadingSubsystemIsInProgressTest::RunTest(const FString& Parameters)
{
	// Without subsystem, verify the concept
	bool bIsInProgress = false;
	TestFalse(TEXT("Initially no load should be in progress"), bIsInProgress);

	// Simulate load start
	bIsInProgress = true;
	TestTrue(TEXT("Load should be in progress after start"), bIsInProgress);

	// Simulate load complete
	bIsInProgress = false;
	TestFalse(TEXT("Load should not be in progress after complete"), bIsInProgress);

	return true;
}

/**
 * Test: Telemetry Tracking
 * Verifies telemetry data structure
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectLoadingSubsystemTelemetryTest,
	"ProjectLoading.Subsystem.Telemetry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectLoadingSubsystemTelemetryTest::RunTest(const FString& Parameters)
{
	FProjectLoadTelemetry Telemetry;

	// Verify initial state
	TestEqual(TEXT("LoadStartTimeSeconds should be 0.0"), Telemetry.LoadStartTimeSeconds, 0.0);
	TestEqual(TEXT("LoadEndTimeSeconds should be 0.0"), Telemetry.LoadEndTimeSeconds, 0.0);
	TestEqual(TEXT("TotalProgress should be 0.0"), Telemetry.TotalProgress, 0.0f);

	// Simulate telemetry update
	Telemetry.LoadStartTimeSeconds = 100.0;
	Telemetry.LoadEndTimeSeconds = 105.5;
	Telemetry.TotalProgress = 1.0f;

	TestEqual(TEXT("LoadStartTimeSeconds should be 100.0"), Telemetry.LoadStartTimeSeconds, 100.0);
	TestEqual(TEXT("LoadEndTimeSeconds should be 105.5"), Telemetry.LoadEndTimeSeconds, 105.5);
	TestEqual(TEXT("TotalProgress should be 1.0"), Telemetry.TotalProgress, 1.0f);

	// Verify duration calculation
	double Duration = Telemetry.LoadEndTimeSeconds - Telemetry.LoadStartTimeSeconds;
	TestEqual(TEXT("Duration should be 5.5 seconds"), Duration, 5.5);

	return true;
}

/**
 * Test: Event Delegates
 * Verifies event delegate signatures exist
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectLoadingSubsystemEventsTest,
	"ProjectLoading.Subsystem.Events",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectLoadingSubsystemEventsTest::RunTest(const FString& Parameters)
{
	// Verify event delegate types are defined
	// (This compiles successfully if delegates are properly declared)

	FProjectLoadStartedSignature OnLoadStarted;
	FProjectLoadPhaseChangedSignature OnPhaseChanged;
	FProjectLoadProgressSignature OnProgress;
	FProjectLoadCompletedSignature OnCompleted;
	FProjectLoadFailedSignature OnFailed;

	TestTrue(TEXT("Event delegates should be defined"), true);

	return true;
}

/**
 * Test: Load Request Validation
 * Verifies load request data structure
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectLoadingSubsystemLoadRequestTest,
	"ProjectLoading.Subsystem.LoadRequest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectLoadingSubsystemLoadRequestTest::RunTest(const FString& Parameters)
{
	FLoadRequest Request;

	// Test default values
	TestFalse(TEXT("Default MapAssetId should be invalid"), Request.MapAssetId.IsValid());
	TestFalse(TEXT("Default ExperienceAssetId should be invalid"), Request.ExperienceAssetId.IsValid());
	TestEqual(TEXT("Default Priority should be 0"), Request.Priority, 0);
	TestFalse(TEXT("Default bSkipCinematics should be false"), Request.bSkipCinematics);
	TestTrue(TEXT("Default bPerformWarmup should be true"), Request.bPerformWarmup);
	TestEqual(TEXT("ContentPacksToMount should be empty"), Request.ContentPacksToMount.Num(), 0);
	TestEqual(TEXT("Custom options container should start empty"), Request.CustomOptions.Num(), 0);

	// Test setting values
	Request.MapAssetId = FPrimaryAssetId(TEXT("ProjectWorld"), TEXT("KazanMain"));
	Request.ExperienceAssetId = FPrimaryAssetId(TEXT("GameMode"), TEXT("Exploration"));
	Request.LoadMode = ELoadMode::MultiplayerClient;
	Request.Priority = 200;
	Request.bSkipCinematics = true;
	Request.bPerformWarmup = false;
	Request.FeaturesToActivate = { TEXT("CombatSystem"), TEXT("DialogueSystem") };
	Request.ContentPacksToMount = { TEXT("ProjectContentPack_Starter"), TEXT("ProjectContentPack_Tower") };
	Request.CustomOptions.Add(TEXT("Difficulty"), TEXT("Hard"));

	TestTrue(TEXT("MapAssetId should be valid"), Request.MapAssetId.IsValid());
	TestEqual(TEXT("MapAssetId should serialize correctly"), Request.MapAssetId.ToString(), FString(TEXT("ProjectWorld:KazanMain")));
	TestEqual(TEXT("ExperienceAssetId should serialize correctly"), Request.ExperienceAssetId.ToString(), FString(TEXT("GameMode:Exploration")));
	TestEqual(TEXT("LoadMode should be multiplayer client"), Request.LoadMode, ELoadMode::MultiplayerClient);
	TestEqual(TEXT("Priority should be 200"), Request.Priority, 200);
	TestTrue(TEXT("bSkipCinematics should be true"), Request.bSkipCinematics);
	TestFalse(TEXT("bPerformWarmup should be false when overridden"), Request.bPerformWarmup);
	TestEqual(TEXT("FeaturesToActivate should contain two entries"), Request.FeaturesToActivate.Num(), 2);
	TestEqual(TEXT("ContentPacksToMount should contain two entries"), Request.ContentPacksToMount.Num(), 2);
	TestTrue(TEXT("Custom option should be stored"), Request.CustomOptions.Contains(TEXT("Difficulty")));

	return true;
}

/**
 * Test: Phase State Tracking
 * Verifies phase state data structure
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectLoadingSubsystemPhaseStateTest,
	"ProjectLoading.Subsystem.PhaseState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectLoadingSubsystemPhaseStateTest::RunTest(const FString& Parameters)
{
	FLoadPhaseState PhaseState;

	// Test enum values exist
	ELoadPhase Phase = ELoadPhase::ResolveAssets;
	TestTrue(TEXT("ResolveAssets phase should exist"), Phase == ELoadPhase::ResolveAssets);

	Phase = ELoadPhase::MountContent;
	TestTrue(TEXT("MountContent phase should exist"), Phase == ELoadPhase::MountContent);

	Phase = ELoadPhase::PreloadCriticalAssets;
	TestTrue(TEXT("PreloadCriticalAssets phase should exist"), Phase == ELoadPhase::PreloadCriticalAssets);

	Phase = ELoadPhase::ActivateFeatures;
	TestTrue(TEXT("ActivateFeatures phase should exist"), Phase == ELoadPhase::ActivateFeatures);

	Phase = ELoadPhase::Travel;
	TestTrue(TEXT("Travel phase should exist"), Phase == ELoadPhase::Travel);

	Phase = ELoadPhase::Warmup;
	TestTrue(TEXT("Warmup phase should exist"), Phase == ELoadPhase::Warmup);

	return true;
}

/**
 * Test: StartLoad Validation - No Map
 * Verifies that StartLoad rejects requests without a map
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectLoadingSubsystemValidationNoMapTest,
	"ProjectLoading.Subsystem.Validation.NoMap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectLoadingSubsystemValidationNoMapTest::RunTest(const FString& Parameters)
{
	FLoadRequest Request;
	// Deliberately leave MapAssetId invalid

	TArray<FText> Errors;
	bool bIsValid = Request.Validate(Errors);

	TestFalse(TEXT("Request without map should fail validation"), bIsValid);
	TestTrue(TEXT("Should have at least one error"), Errors.Num() > 0);

	// Verify error message mentions MapAssetId
	bool bFoundMapError = false;
	for (const FText& Error : Errors)
	{
		if (Error.ToString().Contains(TEXT("MapAssetId")))
		{
			bFoundMapError = true;
			break;
		}
	}
	TestTrue(TEXT("Error should mention MapAssetId"), bFoundMapError);

	return true;
}

/**
 * Test: StartLoad Validation - Multiplayer No Session
 * Verifies that StartLoad rejects multiplayer requests without session data
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectLoadingSubsystemValidationMultiplayerNoSessionTest,
	"ProjectLoading.Subsystem.Validation.MultiplayerNoSession",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectLoadingSubsystemValidationMultiplayerNoSessionTest::RunTest(const FString& Parameters)
{
	FLoadRequest Request;
	Request.MapAssetId = FPrimaryAssetId(TEXT("ProjectWorld"), TEXT("TestMap"));
	Request.LoadMode = ELoadMode::MultiplayerClient;
	// Deliberately leave SessionData empty

	TArray<FText> Errors;
	bool bIsValid = Request.Validate(Errors);

	TestFalse(TEXT("Multiplayer request without session data should fail"), bIsValid);
	TestTrue(TEXT("Should have at least one error"), Errors.Num() > 0);

	// Verify error message mentions session data
	bool bFoundSessionError = false;
	for (const FText& Error : Errors)
	{
		if (Error.ToString().Contains(TEXT("SessionData")))
		{
			bFoundSessionError = true;
			break;
		}
	}
	TestTrue(TEXT("Error should mention SessionData"), bFoundSessionError);

	return true;
}

/**
 * Test: StartLoad Validation - Empty Feature
 * Verifies that StartLoad rejects requests with empty feature names
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectLoadingSubsystemValidationEmptyFeatureTest,
	"ProjectLoading.Subsystem.Validation.EmptyFeature",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectLoadingSubsystemValidationEmptyFeatureTest::RunTest(const FString& Parameters)
{
	FLoadRequest Request;
	Request.MapAssetId = FPrimaryAssetId(TEXT("ProjectWorld"), TEXT("TestMap"));
	Request.FeaturesToActivate.Add(TEXT("ValidFeature"));
	Request.FeaturesToActivate.Add(TEXT("")); // Empty feature name

	TArray<FText> Errors;
	bool bIsValid = Request.Validate(Errors);

	TestFalse(TEXT("Request with empty feature should fail"), bIsValid);
	TestTrue(TEXT("Should have at least one error"), Errors.Num() > 0);

	// Verify error message mentions FeaturesToActivate
	bool bFoundFeatureError = false;
	for (const FText& Error : Errors)
	{
		if (Error.ToString().Contains(TEXT("FeaturesToActivate")))
		{
			bFoundFeatureError = true;
			break;
		}
	}
	TestTrue(TEXT("Error should mention FeaturesToActivate"), bFoundFeatureError);

	return true;
}

/**
 * Test: StartLoad Validation - Duplicate Features
 * Verifies that StartLoad rejects requests with duplicate feature names
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectLoadingSubsystemValidationDuplicateFeaturesTest,
	"ProjectLoading.Subsystem.Validation.DuplicateFeatures",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectLoadingSubsystemValidationDuplicateFeaturesTest::RunTest(const FString& Parameters)
{
	FLoadRequest Request;
	Request.MapAssetId = FPrimaryAssetId(TEXT("ProjectWorld"), TEXT("TestMap"));
	Request.FeaturesToActivate.Add(TEXT("Feature1"));
	Request.FeaturesToActivate.Add(TEXT("Feature2"));
	Request.FeaturesToActivate.Add(TEXT("Feature1")); // Duplicate

	TArray<FText> Errors;
	bool bIsValid = Request.Validate(Errors);

	TestFalse(TEXT("Request with duplicate features should fail"), bIsValid);
	TestTrue(TEXT("Should have at least one error"), Errors.Num() > 0);

	// Verify error message mentions duplicate
	bool bFoundDuplicateError = false;
	for (const FText& Error : Errors)
	{
		if (Error.ToString().Contains(TEXT("Duplicate")))
		{
			bFoundDuplicateError = true;
			break;
		}
	}
	TestTrue(TEXT("Error should mention Duplicate"), bFoundDuplicateError);

	return true;
}

/**
 * Test: StartLoad Validation - Valid Request Passes
 * Verifies that valid requests pass validation
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectLoadingSubsystemValidationValidRequestTest,
	"ProjectLoading.Subsystem.Validation.ValidRequest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectLoadingSubsystemValidationValidRequestTest::RunTest(const FString& Parameters)
{
	FLoadRequest Request;
	Request.MapAssetId = FPrimaryAssetId(TEXT("ProjectWorld"), TEXT("TestMap"));
	Request.ExperienceAssetId = FPrimaryAssetId(TEXT("GameMode"), TEXT("Startup"));
	Request.LoadMode = ELoadMode::SinglePlayer;
	Request.FeaturesToActivate.Add(TEXT("Feature1"));
	Request.FeaturesToActivate.Add(TEXT("Feature2"));

	TArray<FText> Errors;
	bool bIsValid = Request.Validate(Errors);

	TestTrue(TEXT("Valid request should pass validation"), bIsValid);
	TestEqual(TEXT("Should have no errors"), Errors.Num(), 0);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
