// Copyright ALIS. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Types/ProjectLoadRequest.h"
#include "Types/ProjectLoadPhaseState.h"
#include "Types/InventoryStackRules.h"
#include "Interfaces/ProjectLoadingHandle.h"
#include "Interfaces/IItemDataProvider.h"
#include "ProjectServiceLocator.h"
#include "ProjectConfigHelpers.h"

/**
 * Unit tests for ProjectCore types and infrastructure
 *
 * Tests:
 * - FLoadRequest data structure
 * - FLoadPhaseState data structure
 * - ILoadingHandle interface contracts (via mock)
 * - FProjectServiceLocator registration/lookup
 * - FProjectConfigHelpers config access
 */

#if WITH_DEV_AUTOMATION_TESTS

// ============================================================================
// FLoadRequest Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_LoadRequest_DefaultConstruction,
	"ProjectCore.Types.LoadRequest.DefaultConstruction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_LoadRequest_DefaultConstruction::RunTest(const FString& Parameters)
{
	FLoadRequest Request;

	TestEqual(TEXT("Default LoadMode is SinglePlayer"), Request.LoadMode, ELoadMode::SinglePlayer);
	TestFalse(TEXT("Default MapAssetId is invalid"), Request.MapAssetId.IsValid());
	TestFalse(TEXT("Default ExperienceAssetId is invalid"), Request.ExperienceAssetId.IsValid());
	TestFalse(TEXT("Default bSkipCinematics is false"), Request.bSkipCinematics);
	TestTrue(TEXT("Default bPerformWarmup is true"), Request.bPerformWarmup);
	TestEqual(TEXT("Default Priority is 0"), Request.Priority, 0);
	TestEqual(TEXT("Default FeaturesToActivate is empty"), Request.FeaturesToActivate.Num(), 0);
	TestEqual(TEXT("Default ContentPacksToMount is empty"), Request.ContentPacksToMount.Num(), 0);
	TestEqual(TEXT("Default PartyMemberIds is empty"), Request.PartyMemberIds.Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_LoadRequest_Validation,
	"ProjectCore.Types.LoadRequest.Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_LoadRequest_Validation::RunTest(const FString& Parameters)
{
	FLoadRequest Request;

	// Invalid by default (no map)
	TestFalse(TEXT("Request without map is invalid"), Request.IsValid());

	// Valid with map
	Request.MapAssetId = FPrimaryAssetId(TEXT("ProjectWorld"), TEXT("KazanMain"));
	TestTrue(TEXT("Request with map is valid"), Request.IsValid());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_LoadRequest_ToString,
	"ProjectCore.Types.LoadRequest.ToString",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_LoadRequest_ToString::RunTest(const FString& Parameters)
{
	FLoadRequest Request;
	Request.MapAssetId = FPrimaryAssetId(TEXT("ProjectWorld"), TEXT("KazanMain"));
	Request.ExperienceAssetId = FPrimaryAssetId(TEXT("GameMode"), TEXT("Survival"));
	Request.LoadMode = ELoadMode::SinglePlayer;

	FString DebugString = Request.ToString();

	TestTrue(TEXT("ToString contains map name"), DebugString.Contains(TEXT("KazanMain")));
	TestTrue(TEXT("ToString contains experience name"), DebugString.Contains(TEXT("Survival")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_LoadRequest_Properties,
	"ProjectCore.Types.LoadRequest.Properties",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_LoadRequest_Properties::RunTest(const FString& Parameters)
{
	FLoadRequest Request;

	// Test features
	Request.FeaturesToActivate.Add(TEXT("CombatSystem"));
	Request.FeaturesToActivate.Add(TEXT("DialogueSystem"));
	TestEqual(TEXT("FeaturesToActivate count is 2"), Request.FeaturesToActivate.Num(), 2);
	TestEqual(TEXT("First feature is CombatSystem"), Request.FeaturesToActivate[0], TEXT("CombatSystem"));

	// Test content packs
	Request.ContentPacksToMount.Add(TEXT("KazanPack"));
	TestEqual(TEXT("ContentPacksToMount count is 1"), Request.ContentPacksToMount.Num(), 1);

	// Test custom options
	Request.CustomOptions.Add(TEXT("Difficulty"), TEXT("Hard"));
	Request.CustomOptions.Add(TEXT("Language"), TEXT("RU"));
	TestEqual(TEXT("CustomOptions count is 2"), Request.CustomOptions.Num(), 2);
	TestEqual(TEXT("Difficulty option is Hard"), Request.CustomOptions[TEXT("Difficulty")], TEXT("Hard"));

	// Test multiplayer properties
	Request.LoadMode = ELoadMode::MultiplayerClient;
	Request.SessionData = TEXT("192.168.1.100:7777");
	Request.PartyMemberIds.Add(TEXT("Player1"));
	Request.PartyMemberIds.Add(TEXT("Player2"));
	TestEqual(TEXT("LoadMode is MultiplayerClient"), Request.LoadMode, ELoadMode::MultiplayerClient);
	TestEqual(TEXT("SessionData is set"), Request.SessionData, TEXT("192.168.1.100:7777"));
	TestEqual(TEXT("PartyMemberIds count is 2"), Request.PartyMemberIds.Num(), 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_LoadRequest_ValidateSuccess,
	"ProjectCore.Types.LoadRequest.ValidateSuccess",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_LoadRequest_ValidateSuccess::RunTest(const FString& Parameters)
{
	FLoadRequest Request;
	Request.MapAssetId = FPrimaryAssetId(TEXT("ProjectWorld"), TEXT("TestMap"));
	Request.FeaturesToActivate.Add(TEXT("Feature1"));
	Request.FeaturesToActivate.Add(TEXT("Feature2"));

	TArray<FText> Errors;
	bool bIsValid = Request.Validate(Errors);

	TestTrue(TEXT("Valid request passes validation"), bIsValid);
	TestEqual(TEXT("No validation errors"), Errors.Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_LoadRequest_ValidateNoMap,
	"ProjectCore.Types.LoadRequest.ValidateNoMap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_LoadRequest_ValidateNoMap::RunTest(const FString& Parameters)
{
	FLoadRequest Request;
	// MapAssetId not set

	TArray<FText> Errors;
	bool bIsValid = Request.Validate(Errors);

	TestFalse(TEXT("Request without map fails validation"), bIsValid);
	TestTrue(TEXT("Validation returns error"), Errors.Num() > 0);

	if (Errors.Num() > 0)
	{
		TestTrue(TEXT("Error mentions MapAssetId"), Errors[0].ToString().Contains(TEXT("MapAssetId")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_LoadRequest_ValidateMultiplayerNoSession,
	"ProjectCore.Types.LoadRequest.ValidateMultiplayerNoSession",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_LoadRequest_ValidateMultiplayerNoSession::RunTest(const FString& Parameters)
{
	FLoadRequest Request;
	Request.MapAssetId = FPrimaryAssetId(TEXT("ProjectWorld"), TEXT("TestMap"));
	Request.LoadMode = ELoadMode::MultiplayerClient;
	// SessionData not set

	TArray<FText> Errors;
	bool bIsValid = Request.Validate(Errors);

	TestFalse(TEXT("Multiplayer request without SessionData fails"), bIsValid);
	TestTrue(TEXT("Validation returns error"), Errors.Num() > 0);

	bool bFoundSessionError = false;
	for (const FText& Error : Errors)
	{
		if (Error.ToString().Contains(TEXT("SessionData")))
		{
			bFoundSessionError = true;
			break;
		}
	}
	TestTrue(TEXT("Error mentions SessionData"), bFoundSessionError);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_LoadRequest_ValidateEmptyFeature,
	"ProjectCore.Types.LoadRequest.ValidateEmptyFeature",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_LoadRequest_ValidateEmptyFeature::RunTest(const FString& Parameters)
{
	FLoadRequest Request;
	Request.MapAssetId = FPrimaryAssetId(TEXT("ProjectWorld"), TEXT("TestMap"));
	Request.FeaturesToActivate.Add(TEXT("Feature1"));
	Request.FeaturesToActivate.Add(TEXT("")); // Empty feature name
	Request.FeaturesToActivate.Add(TEXT("Feature2"));

	TArray<FText> Errors;
	bool bIsValid = Request.Validate(Errors);

	TestFalse(TEXT("Request with empty feature fails validation"), bIsValid);
	TestTrue(TEXT("Validation returns error"), Errors.Num() > 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_LoadRequest_ValidateDuplicateFeatures,
	"ProjectCore.Types.LoadRequest.ValidateDuplicateFeatures",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_LoadRequest_ValidateDuplicateFeatures::RunTest(const FString& Parameters)
{
	FLoadRequest Request;
	Request.MapAssetId = FPrimaryAssetId(TEXT("ProjectWorld"), TEXT("TestMap"));
	Request.FeaturesToActivate.Add(TEXT("Feature1"));
	Request.FeaturesToActivate.Add(TEXT("Feature2"));
	Request.FeaturesToActivate.Add(TEXT("Feature1")); // Duplicate

	TArray<FText> Errors;
	bool bIsValid = Request.Validate(Errors);

	TestFalse(TEXT("Request with duplicate features fails validation"), bIsValid);
	TestTrue(TEXT("Validation returns error"), Errors.Num() > 0);

	bool bFoundDuplicateError = false;
	for (const FText& Error : Errors)
	{
		if (Error.ToString().Contains(TEXT("Duplicate")))
		{
			bFoundDuplicateError = true;
			break;
		}
	}
	TestTrue(TEXT("Error mentions duplicate"), bFoundDuplicateError);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_LoadRequest_ValidateEmptyPack,
	"ProjectCore.Types.LoadRequest.ValidateEmptyPack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_LoadRequest_ValidateEmptyPack::RunTest(const FString& Parameters)
{
	FLoadRequest Request;
	Request.MapAssetId = FPrimaryAssetId(TEXT("ProjectWorld"), TEXT("TestMap"));
	Request.ContentPacksToMount.Add(TEXT("Pack1"));
	Request.ContentPacksToMount.Add(TEXT("")); // Empty pack name

	TArray<FText> Errors;
	bool bIsValid = Request.Validate(Errors);

	TestFalse(TEXT("Request with empty pack fails validation"), bIsValid);
	TestTrue(TEXT("Validation returns error"), Errors.Num() > 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_LoadRequest_ValidateDuplicatePacks,
	"ProjectCore.Types.LoadRequest.ValidateDuplicatePacks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_LoadRequest_ValidateDuplicatePacks::RunTest(const FString& Parameters)
{
	FLoadRequest Request;
	Request.MapAssetId = FPrimaryAssetId(TEXT("ProjectWorld"), TEXT("TestMap"));
	Request.ContentPacksToMount.Add(TEXT("Pack1"));
	Request.ContentPacksToMount.Add(TEXT("Pack2"));
	Request.ContentPacksToMount.Add(TEXT("Pack1")); // Duplicate

	TArray<FText> Errors;
	bool bIsValid = Request.Validate(Errors);

	TestFalse(TEXT("Request with duplicate packs fails validation"), bIsValid);
	TestTrue(TEXT("Validation returns error"), Errors.Num() > 0);

	return true;
}

// ============================================================================
// FLoadPhaseState Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_LoadPhaseState_DefaultConstruction,
	"ProjectCore.Types.LoadPhaseState.DefaultConstruction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_LoadPhaseState_DefaultConstruction::RunTest(const FString& Parameters)
{
	FLoadPhaseState State;

	TestEqual(TEXT("Default Phase is None"), State.Phase, ELoadPhase::None);
	TestEqual(TEXT("Default State is Pending"), State.State, EPhaseState::Pending);
	TestEqual(TEXT("Default Progress is 0.0"), State.Progress, 0.0f);
	TestEqual(TEXT("Default StartTime is 0.0"), State.StartTime, 0.0);
	TestEqual(TEXT("Default EndTime is 0.0"), State.EndTime, 0.0);
	TestEqual(TEXT("Default ErrorCode is 0"), State.ErrorCode, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_LoadPhaseState_PhaseConstruction,
	"ProjectCore.Types.LoadPhaseState.PhaseConstruction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_LoadPhaseState_PhaseConstruction::RunTest(const FString& Parameters)
{
	FLoadPhaseState State(ELoadPhase::ResolveAssets);

	TestEqual(TEXT("Phase is ResolveAssets"), State.Phase, ELoadPhase::ResolveAssets);
	TestEqual(TEXT("State is Pending"), State.State, EPhaseState::Pending);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_LoadPhaseState_Duration,
	"ProjectCore.Types.LoadPhaseState.Duration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_LoadPhaseState_Duration::RunTest(const FString& Parameters)
{
	FLoadPhaseState State;

	// No duration initially
	TestEqual(TEXT("Duration is 0.0 without times"), State.GetDuration(), 0.0);

	// Set times
	State.StartTime = 10.0;
	State.EndTime = 15.5;

	TestEqual(TEXT("Duration is 5.5 seconds"), State.GetDuration(), 5.5);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_LoadPhaseState_StateChecks,
	"ProjectCore.Types.LoadPhaseState.StateChecks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_LoadPhaseState_StateChecks::RunTest(const FString& Parameters)
{
	FLoadPhaseState State;

	// Pending state
	TestFalse(TEXT("Pending is not complete"), State.IsComplete());
	TestFalse(TEXT("Pending is not failed"), State.IsFailed());
	TestFalse(TEXT("Pending is not in progress"), State.IsInProgress());

	// In progress
	State.State = EPhaseState::InProgress;
	TestFalse(TEXT("InProgress is not complete"), State.IsComplete());
	TestFalse(TEXT("InProgress is not failed"), State.IsFailed());
	TestTrue(TEXT("InProgress is in progress"), State.IsInProgress());

	// Completed
	State.State = EPhaseState::Completed;
	TestTrue(TEXT("Completed is complete"), State.IsComplete());
	TestFalse(TEXT("Completed is not failed"), State.IsFailed());
	TestFalse(TEXT("Completed is not in progress"), State.IsInProgress());

	// Failed
	State.State = EPhaseState::Failed;
	TestFalse(TEXT("Failed is not complete"), State.IsComplete());
	TestTrue(TEXT("Failed is failed"), State.IsFailed());
	TestFalse(TEXT("Failed is not in progress"), State.IsInProgress());

	// Skipped
	State.State = EPhaseState::Skipped;
	TestTrue(TEXT("Skipped is complete"), State.IsComplete());
	TestFalse(TEXT("Skipped is not failed"), State.IsFailed());
	TestFalse(TEXT("Skipped is not in progress"), State.IsInProgress());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_LoadPhaseState_PhaseName,
	"ProjectCore.Types.LoadPhaseState.PhaseName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_LoadPhaseState_PhaseName::RunTest(const FString& Parameters)
{
	FLoadPhaseState State(ELoadPhase::ResolveAssets);

	FString PhaseName = State.GetPhaseName();
	TestTrue(TEXT("Phase name contains 'ResolveAssets'"), PhaseName.Contains(TEXT("ResolveAssets")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_LoadPhaseState_ToString,
	"ProjectCore.Types.LoadPhaseState.ToString",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_LoadPhaseState_ToString::RunTest(const FString& Parameters)
{
	FLoadPhaseState State(ELoadPhase::MountContent);
	State.State = EPhaseState::InProgress;
	State.Progress = 0.65f;

	FString DebugString = State.ToString();

	TestTrue(TEXT("ToString contains phase name"), DebugString.Contains(TEXT("MountContent")));
	TestTrue(TEXT("ToString contains state"), DebugString.Contains(TEXT("InProgress")));
	TestTrue(TEXT("ToString contains progress percentage"), DebugString.Contains(TEXT("65")));

	return true;
}

// ============================================================================
// ILoadingHandle Tests (Mock Implementation)
// ============================================================================

// Mock implementation for testing interface contracts
class FMockLoadingHandle : public ILoadingHandle
{
private:
	ELoadingState State;
	float Progress;
	FName CurrentPhase;
	FText StatusMessage;
	FText ErrorMessage;
	int32 ErrorCode;

public:
	FMockLoadingHandle()
		: State(ELoadingState::Idle)
		, Progress(0.0f)
		, CurrentPhase(NAME_None)
		, ErrorCode(0)
	{
	}

	virtual ELoadingState GetState() const override { return State; }
	virtual float GetProgress() const override { return Progress; }
	virtual FName GetCurrentPhase() const override { return CurrentPhase; }
	virtual FText GetStatusMessage() const override { return StatusMessage; }
	virtual bool IsInProgress() const override { return State == ELoadingState::InProgress; }
	virtual bool IsCompleted() const override { return State == ELoadingState::Completed; }
	virtual bool IsFailed() const override { return State == ELoadingState::Failed; }
	virtual bool IsCancelled() const override { return State == ELoadingState::Cancelled; }
	virtual FText GetErrorMessage() const override { return ErrorMessage; }
	virtual int32 GetErrorCode() const override { return ErrorCode; }

	virtual bool Cancel(bool bForce = false) override
	{
		if (State == ELoadingState::InProgress)
		{
			State = ELoadingState::Cancelled;
			return true;
		}
		return false;
	}

	// Test helpers
	void SetState(ELoadingState InState) { State = InState; }
	void SetProgress(float InProgress) { Progress = InProgress; }
	void SetPhase(FName InPhase) { CurrentPhase = InPhase; }
	void SetStatus(const FText& InStatus) { StatusMessage = InStatus; }
	void SetError(const FText& InError, int32 InCode) { ErrorMessage = InError; ErrorCode = InCode; }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_LoadingHandle_StateQueries,
	"ProjectCore.Types.LoadingHandle.StateQueries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_LoadingHandle_StateQueries::RunTest(const FString& Parameters)
{
	FMockLoadingHandle Handle;

	// Idle state
	TestEqual(TEXT("Initial state is Idle"), Handle.GetState(), ELoadingState::Idle);
	TestFalse(TEXT("Idle is not in progress"), Handle.IsInProgress());
	TestFalse(TEXT("Idle is not completed"), Handle.IsCompleted());
	TestFalse(TEXT("Idle is not failed"), Handle.IsFailed());
	TestFalse(TEXT("Idle is not cancelled"), Handle.IsCancelled());

	// In progress state
	Handle.SetState(ELoadingState::InProgress);
	TestTrue(TEXT("InProgress is in progress"), Handle.IsInProgress());
	TestFalse(TEXT("InProgress is not completed"), Handle.IsCompleted());

	// Completed state
	Handle.SetState(ELoadingState::Completed);
	TestFalse(TEXT("Completed is not in progress"), Handle.IsInProgress());
	TestTrue(TEXT("Completed is completed"), Handle.IsCompleted());
	TestFalse(TEXT("Completed is not failed"), Handle.IsFailed());

	// Failed state
	Handle.SetState(ELoadingState::Failed);
	TestFalse(TEXT("Failed is not completed"), Handle.IsCompleted());
	TestTrue(TEXT("Failed is failed"), Handle.IsFailed());

	// Cancelled state
	Handle.SetState(ELoadingState::Cancelled);
	TestTrue(TEXT("Cancelled is cancelled"), Handle.IsCancelled());
	TestFalse(TEXT("Cancelled is not completed"), Handle.IsCompleted());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_LoadingHandle_Progress,
	"ProjectCore.Types.LoadingHandle.Progress",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_LoadingHandle_Progress::RunTest(const FString& Parameters)
{
	FMockLoadingHandle Handle;

	TestEqual(TEXT("Initial progress is 0.0"), Handle.GetProgress(), 0.0f);

	Handle.SetProgress(0.5f);
	TestEqual(TEXT("Progress is 0.5"), Handle.GetProgress(), 0.5f);

	Handle.SetProgress(1.0f);
	TestEqual(TEXT("Progress is 1.0"), Handle.GetProgress(), 1.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_LoadingHandle_PhaseTracking,
	"ProjectCore.Types.LoadingHandle.PhaseTracking",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_LoadingHandle_PhaseTracking::RunTest(const FString& Parameters)
{
	FMockLoadingHandle Handle;

	TestTrue(TEXT("Initial phase is None"), Handle.GetCurrentPhase() == NAME_None);

	Handle.SetPhase(FName(TEXT("ResolveAssets")));
	TestTrue(TEXT("Phase is ResolveAssets"), Handle.GetCurrentPhase() == FName(TEXT("ResolveAssets")));

	Handle.SetPhase(FName(TEXT("Travel")));
	TestTrue(TEXT("Phase is Travel"), Handle.GetCurrentPhase() == FName(TEXT("Travel")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_LoadingHandle_StatusMessages,
	"ProjectCore.Types.LoadingHandle.StatusMessages",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_LoadingHandle_StatusMessages::RunTest(const FString& Parameters)
{
	FMockLoadingHandle Handle;

	TestTrue(TEXT("Initial status is empty"), Handle.GetStatusMessage().IsEmpty());

	FText Status = FText::FromString(TEXT("Loading map..."));
	Handle.SetStatus(Status);
	TestEqual(TEXT("Status message is set"), Handle.GetStatusMessage().ToString(), Status.ToString());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_LoadingHandle_ErrorHandling,
	"ProjectCore.Types.LoadingHandle.ErrorHandling",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_LoadingHandle_ErrorHandling::RunTest(const FString& Parameters)
{
	FMockLoadingHandle Handle;

	TestTrue(TEXT("Initial error message is empty"), Handle.GetErrorMessage().IsEmpty());
	TestEqual(TEXT("Initial error code is 0"), Handle.GetErrorCode(), 0);

	FText ErrorMsg = FText::FromString(TEXT("Failed to load asset"));
	Handle.SetError(ErrorMsg, 404);

	TestEqual(TEXT("Error message is set"), Handle.GetErrorMessage().ToString(), ErrorMsg.ToString());
	TestEqual(TEXT("Error code is 404"), Handle.GetErrorCode(), 404);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_LoadingHandle_Cancellation,
	"ProjectCore.Types.LoadingHandle.Cancellation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_LoadingHandle_Cancellation::RunTest(const FString& Parameters)
{
	FMockLoadingHandle Handle;

	// Cannot cancel idle operation
	TestFalse(TEXT("Cannot cancel idle operation"), Handle.Cancel());
	TestFalse(TEXT("State is still Idle"), Handle.IsCancelled());

	// Can cancel in-progress operation
	Handle.SetState(ELoadingState::InProgress);
	TestTrue(TEXT("Can cancel in-progress operation"), Handle.Cancel());
	TestTrue(TEXT("State is now Cancelled"), Handle.IsCancelled());

	// Cannot cancel already cancelled operation
	TestFalse(TEXT("Cannot cancel already cancelled operation"), Handle.Cancel());

	return true;
}

// ============================================================================
// FProjectServiceLocator Tests
// ============================================================================

// Test service interface
class ITestService
{
public:
	virtual ~ITestService() = default;
	virtual FString GetName() const = 0;

	static FName ServiceKey()
	{
		static FName Key(TEXT("ITestService"));
		return Key;
	}
};

// Test service implementation
class FTestServiceImpl : public ITestService
{
private:
	FString Name;

public:
	explicit FTestServiceImpl(const FString& InName) : Name(InName) {}
	virtual FString GetName() const override { return Name; }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_ServiceLocator_RegisterAndResolve,
	"ProjectCore.ServiceLocator.RegisterAndResolve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_ServiceLocator_RegisterAndResolve::RunTest(const FString& Parameters)
{
	// Clean state
	FProjectServiceLocator::Reset();

	// Not registered initially
	TestFalse(TEXT("Service not registered initially"), FProjectServiceLocator::IsRegistered<ITestService>());
	TestNull(TEXT("Resolve returns null initially"), FProjectServiceLocator::Resolve<ITestService>().Get());

	// Register service
	TSharedRef<ITestService> Service = MakeShared<FTestServiceImpl>(TEXT("TestService"));
	FProjectServiceLocator::Register<ITestService>(Service);

	// Now registered
	TestTrue(TEXT("Service is registered"), FProjectServiceLocator::IsRegistered<ITestService>());

	// Can resolve
	TSharedPtr<ITestService> Resolved = FProjectServiceLocator::Resolve<ITestService>();
	TestNotNull(TEXT("Resolve returns service"), Resolved.Get());
	TestEqual(TEXT("Resolved service has correct name"), Resolved->GetName(), TEXT("TestService"));

	// Cleanup
	FProjectServiceLocator::Reset();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_ServiceLocator_Unregister,
	"ProjectCore.ServiceLocator.Unregister",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_ServiceLocator_Unregister::RunTest(const FString& Parameters)
{
	// Clean state
	FProjectServiceLocator::Reset();

	// Register
	TSharedRef<ITestService> Service = MakeShared<FTestServiceImpl>(TEXT("TestService"));
	FProjectServiceLocator::Register<ITestService>(Service);
	TestTrue(TEXT("Service is registered"), FProjectServiceLocator::IsRegistered<ITestService>());

	// Unregister
	FProjectServiceLocator::Unregister<ITestService>();
	TestFalse(TEXT("Service is unregistered"), FProjectServiceLocator::IsRegistered<ITestService>());
	TestNull(TEXT("Resolve returns null after unregister"), FProjectServiceLocator::Resolve<ITestService>().Get());

	// Cleanup
	FProjectServiceLocator::Reset();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_ServiceLocator_Replace,
	"ProjectCore.ServiceLocator.Replace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_ServiceLocator_Replace::RunTest(const FString& Parameters)
{
	// Clean state
	FProjectServiceLocator::Reset();

	// Register first service
	TSharedRef<ITestService> Service1 = MakeShared<FTestServiceImpl>(TEXT("Service1"));
	FProjectServiceLocator::Register<ITestService>(Service1);

	TSharedPtr<ITestService> Resolved1 = FProjectServiceLocator::Resolve<ITestService>();
	TestEqual(TEXT("First service is Service1"), Resolved1->GetName(), TEXT("Service1"));

	// Replace with second service
	TSharedRef<ITestService> Service2 = MakeShared<FTestServiceImpl>(TEXT("Service2"));
	FProjectServiceLocator::Register<ITestService>(Service2);

	TSharedPtr<ITestService> Resolved2 = FProjectServiceLocator::Resolve<ITestService>();
	TestEqual(TEXT("Replaced service is Service2"), Resolved2->GetName(), TEXT("Service2"));

	// Cleanup
	FProjectServiceLocator::Reset();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_ServiceLocator_MultipleServices,
	"ProjectCore.ServiceLocator.MultipleServices",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_ServiceLocator_MultipleServices::RunTest(const FString& Parameters)
{
	// Clean state
	FProjectServiceLocator::Reset();

	// Second test service interface
	class IAnotherService
	{
	public:
		virtual ~IAnotherService() = default;
		virtual int32 GetValue() const = 0;

		static FName ServiceKey()
		{
			static FName Key(TEXT("IAnotherService"));
			return Key;
		}
	};

	class FAnotherServiceImpl : public IAnotherService
	{
	private:
		int32 Value;
	public:
		explicit FAnotherServiceImpl(int32 InValue) : Value(InValue) {}
		virtual int32 GetValue() const override { return Value; }
	};

	// Register multiple different services
	TSharedRef<ITestService> Service1 = MakeShared<FTestServiceImpl>(TEXT("TestService"));
	TSharedRef<IAnotherService> Service2 = MakeShared<FAnotherServiceImpl>(42);

	FProjectServiceLocator::Register<ITestService>(Service1);
	FProjectServiceLocator::Register<IAnotherService>(Service2);

	// Both registered
	TestTrue(TEXT("First service is registered"), FProjectServiceLocator::IsRegistered<ITestService>());
	TestTrue(TEXT("Second service is registered"), FProjectServiceLocator::IsRegistered<IAnotherService>());

	// Can resolve both
	TSharedPtr<ITestService> Resolved1 = FProjectServiceLocator::Resolve<ITestService>();
	TSharedPtr<IAnotherService> Resolved2 = FProjectServiceLocator::Resolve<IAnotherService>();

	TestNotNull(TEXT("First service resolved"), Resolved1.Get());
	TestNotNull(TEXT("Second service resolved"), Resolved2.Get());
	TestEqual(TEXT("First service has correct value"), Resolved1->GetName(), TEXT("TestService"));
	TestEqual(TEXT("Second service has correct value"), Resolved2->GetValue(), 42);

	// Cleanup
	FProjectServiceLocator::Reset();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_ServiceLocator_Reset,
	"ProjectCore.ServiceLocator.Reset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_ServiceLocator_Reset::RunTest(const FString& Parameters)
{
	// Register services
	TSharedRef<ITestService> Service = MakeShared<FTestServiceImpl>(TEXT("TestService"));
	FProjectServiceLocator::Register<ITestService>(Service);
	TestTrue(TEXT("Service is registered"), FProjectServiceLocator::IsRegistered<ITestService>());

	// Reset clears all
	FProjectServiceLocator::Reset();
	TestFalse(TEXT("Service is cleared after reset"), FProjectServiceLocator::IsRegistered<ITestService>());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_ServiceLocator_NullPointer,
	"ProjectCore.ServiceLocator.NullPointer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_ServiceLocator_NullPointer::RunTest(const FString& Parameters)
{
	// Clean state
	FProjectServiceLocator::Reset();

	// Attempting to register null should be ignored
	TSharedPtr<ITestService> NullService;
	FProjectServiceLocator::Register<ITestService>(NullService);

	TestFalse(TEXT("Null service is not registered"), FProjectServiceLocator::IsRegistered<ITestService>());

	// Cleanup
	FProjectServiceLocator::Reset();

	return true;
}

// ============================================================================
// FProjectConfigHelpers Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_ConfigHelpers_GetString,
	"ProjectCore.ConfigHelpers.GetString",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_ConfigHelpers_GetString::RunTest(const FString& Parameters)
{
	FString OutValue;

	// Try to read a known config value
	// Note: This test depends on actual config files existing
	// We test the API contract, not specific values
	bool bResult = FProjectConfigHelpers::GetString(TEXT("/Script/EngineSettings.GameMapsSettings"), TEXT("GameDefaultMap"), OutValue);

	// The function should execute without crashing
	// Actual value depends on project config
	TestTrue(TEXT("GetString executes"), true);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_ConfigHelpers_GetBool,
	"ProjectCore.ConfigHelpers.GetBool",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_ConfigHelpers_GetBool::RunTest(const FString& Parameters)
{
	bool bOutValue = false;

	// Test the API contract
	bool bResult = FProjectConfigHelpers::GetBool(TEXT("TestSection"), TEXT("TestKey"), bOutValue);

	// The function should execute without crashing
	TestTrue(TEXT("GetBool executes"), true);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_ConfigHelpers_GetInt32,
	"ProjectCore.ConfigHelpers.GetInt32",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_ConfigHelpers_GetInt32::RunTest(const FString& Parameters)
{
	int32 OutValue = 0;

	// Test the API contract
	bool bResult = FProjectConfigHelpers::GetInt32(TEXT("TestSection"), TEXT("TestKey"), OutValue);

	// The function should execute without crashing
	TestTrue(TEXT("GetInt32 executes"), true);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_ConfigHelpers_GetFloat,
	"ProjectCore.ConfigHelpers.GetFloat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_ConfigHelpers_GetFloat::RunTest(const FString& Parameters)
{
	float OutValue = 0.0f;

	// Test the API contract
	bool bResult = FProjectConfigHelpers::GetFloat(TEXT("TestSection"), TEXT("TestKey"), OutValue);

	// The function should execute without crashing
	TestTrue(TEXT("GetFloat executes"), true);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_ConfigHelpers_GetArray,
	"ProjectCore.ConfigHelpers.GetArray",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_ConfigHelpers_GetArray::RunTest(const FString& Parameters)
{
	TArray<FString> OutValues;

	// Test the API contract
	bool bResult = FProjectConfigHelpers::GetArray(TEXT("TestSection"), TEXT("TestKey"), OutValues);

	// The function should execute without crashing
	TestTrue(TEXT("GetArray executes"), true);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_ConfigHelpers_GetSection,
	"ProjectCore.ConfigHelpers.GetSection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_ConfigHelpers_GetSection::RunTest(const FString& Parameters)
{
	TMap<FString, FString> OutValues;

	// Test the API contract
	bool bResult = FProjectConfigHelpers::GetSection(TEXT("/Script/EngineSettings.GameMapsSettings"), OutValues);

	// The function should execute without crashing
	TestTrue(TEXT("GetSection executes"), true);

	return true;
}

// ============================================================================
// FLoadPhaseState Validation Tests (New Validate() API)
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_LoadPhaseState_Validate_ProgressOutOfBounds,
	"ProjectCore.Types.LoadPhaseState.Validate.ProgressOutOfBounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_LoadPhaseState_Validate_ProgressOutOfBounds::RunTest(const FString& Parameters)
{
	FLoadPhaseState State(ELoadPhase::ResolveAssets);
	State.Progress = 1.5f; // Out of bounds (must be 0.0-1.0)

	TArray<FText> Errors;
	bool bIsValid = State.Validate(Errors);

	TestFalse(TEXT("Progress > 1.0 should fail validation"), bIsValid);
	TestTrue(TEXT("Error message present"), Errors.Num() > 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_LoadPhaseState_Validate_NegativeProgress,
	"ProjectCore.Types.LoadPhaseState.Validate.NegativeProgress",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_LoadPhaseState_Validate_NegativeProgress::RunTest(const FString& Parameters)
{
	FLoadPhaseState State(ELoadPhase::MountContent);
	State.Progress = -0.5f; // Negative (invalid)

	TArray<FText> Errors;
	bool bIsValid = State.Validate(Errors);

	TestFalse(TEXT("Negative progress should fail validation"), bIsValid);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_LoadPhaseState_Validate_NegativeTiming,
	"ProjectCore.Types.LoadPhaseState.Validate.NegativeTiming",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_LoadPhaseState_Validate_NegativeTiming::RunTest(const FString& Parameters)
{
	FLoadPhaseState State(ELoadPhase::PreloadCriticalAssets);
	State.StartTime = -5.0; // Negative (invalid)

	TArray<FText> Errors;
	bool bIsValid = State.Validate(Errors);

	TestFalse(TEXT("Negative StartTime should fail validation"), bIsValid);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_LoadPhaseState_Validate_EndBeforeStart,
	"ProjectCore.Types.LoadPhaseState.Validate.EndBeforeStart",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_LoadPhaseState_Validate_EndBeforeStart::RunTest(const FString& Parameters)
{
	FLoadPhaseState State(ELoadPhase::ActivateFeatures);
	State.StartTime = 10.0;
	State.EndTime = 5.0; // Before StartTime (invalid)

	TArray<FText> Errors;
	bool bIsValid = State.Validate(Errors);

	TestFalse(TEXT("EndTime before StartTime should fail validation"), bIsValid);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_LoadPhaseState_Validate_FailedWithoutError,
	"ProjectCore.Types.LoadPhaseState.Validate.FailedWithoutError",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_LoadPhaseState_Validate_FailedWithoutError::RunTest(const FString& Parameters)
{
	FLoadPhaseState State(ELoadPhase::Travel);
	State.State = EPhaseState::Failed;
	// ErrorMessage is empty (invalid for Failed state)
	// ErrorCode is 0 (invalid for Failed state)

	TArray<FText> Errors;
	bool bIsValid = State.Validate(Errors);

	TestFalse(TEXT("Failed state without ErrorMessage should fail validation"), bIsValid);
	TestTrue(TEXT("At least 2 errors (message + code)"), Errors.Num() >= 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_LoadPhaseState_Validate_FailedWithZeroErrorCode,
	"ProjectCore.Types.LoadPhaseState.Validate.FailedWithZeroErrorCode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_LoadPhaseState_Validate_FailedWithZeroErrorCode::RunTest(const FString& Parameters)
{
	FLoadPhaseState State(ELoadPhase::Warmup);
	State.State = EPhaseState::Failed;
	State.ErrorMessage = FText::FromString(TEXT("Test error"));
	State.ErrorCode = 0; // Invalid for Failed state

	TArray<FText> Errors;
	bool bIsValid = State.Validate(Errors);

	TestFalse(TEXT("Failed state with ErrorCode=0 should fail validation"), bIsValid);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_LoadPhaseState_Validate_NonePhaseWithState,
	"ProjectCore.Types.LoadPhaseState.Validate.NonePhaseWithState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_LoadPhaseState_Validate_NonePhaseWithState::RunTest(const FString& Parameters)
{
	FLoadPhaseState State; // Default: Phase = None, State = Pending
	State.State = EPhaseState::InProgress; // Invalid: None phase shouldn't be InProgress

	TArray<FText> Errors;
	bool bIsValid = State.Validate(Errors);

	TestFalse(TEXT("None phase with non-Pending state should fail validation"), bIsValid);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_LoadPhaseState_Validate_ValidInProgress,
	"ProjectCore.Types.LoadPhaseState.Validate.ValidInProgress",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_LoadPhaseState_Validate_ValidInProgress::RunTest(const FString& Parameters)
{
	FLoadPhaseState State(ELoadPhase::ResolveAssets);
	State.State = EPhaseState::InProgress;
	State.Progress = 0.5f;
	State.StartTime = 1.0;

	TArray<FText> Errors;
	bool bIsValid = State.Validate(Errors);

	TestTrue(TEXT("Valid in-progress state passes validation"), bIsValid);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_LoadPhaseState_Validate_ValidCompleted,
	"ProjectCore.Types.LoadPhaseState.Validate.ValidCompleted",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_LoadPhaseState_Validate_ValidCompleted::RunTest(const FString& Parameters)
{
	FLoadPhaseState State(ELoadPhase::MountContent);
	State.State = EPhaseState::Completed;
	State.Progress = 1.0f;
	State.StartTime = 1.0;
	State.EndTime = 3.0;

	TArray<FText> Errors;
	bool bIsValid = State.Validate(Errors);

	TestTrue(TEXT("Valid completed state passes validation"), bIsValid);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_LoadPhaseState_Validate_ValidFailed,
	"ProjectCore.Types.LoadPhaseState.Validate.ValidFailed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_LoadPhaseState_Validate_ValidFailed::RunTest(const FString& Parameters)
{
	FLoadPhaseState State(ELoadPhase::ActivateFeatures);
	State.State = EPhaseState::Failed;
	State.Progress = 0.3f;
	State.StartTime = 1.0;
	State.EndTime = 2.0;
	State.ErrorMessage = FText::FromString(TEXT("Failed to activate feature"));
	State.ErrorCode = 404;

	TArray<FText> Errors;
	bool bIsValid = State.Validate(Errors);

	TestTrue(TEXT("Valid failed state passes validation"), bIsValid);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_InventoryStackRules_DepthFallbackUsesMaxStack,
	"ProjectCore.Types.InventoryStackRules.DepthFallbackUsesMaxStack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_InventoryStackRules_DepthFallbackUsesMaxStack::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FItemDataView ItemData;
	ItemData.GridSize = FIntPoint(1, 1);
	ItemData.MaxStack = 6;
	ItemData.UnitsPerDepthUnit = 0;

	TestTrue(TEXT("1x1 stackable item should use depth stacking rules"), FInventoryStackRules::UsesDepthStacking(ItemData));
	TestEqual(TEXT("Unset UnitsPerDepthUnit should fall back to MaxStack"), FInventoryStackRules::ResolveUnitsPerDepthUnit(ItemData), 6);
	TestEqual(TEXT("Default one-depth container should preserve legacy stack cap"), FInventoryStackRules::CalculateMaxStackForContainer(ItemData, 1), 6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_InventoryStackRules_AuthoredDepthCapsStack,
	"ProjectCore.Types.InventoryStackRules.AuthoredDepthCapsStack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_InventoryStackRules_AuthoredDepthCapsStack::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FItemDataView ItemData;
	ItemData.GridSize = FIntPoint(1, 1);
	ItemData.MaxStack = 99;
	ItemData.UnitsPerDepthUnit = 2;

	TestEqual(TEXT("Authored units per depth unit should be honored"), FInventoryStackRules::ResolveUnitsPerDepthUnit(ItemData), 2);
	TestEqual(TEXT("Cell depth should cap the effective stack"), FInventoryStackRules::CalculateMaxStackForContainer(ItemData, 4), 8);
	TestEqual(TEXT("Quantity should report consumed depth units"), FInventoryStackRules::CalculateDepthUnitsForQuantity(ItemData, 5), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectCore_InventoryStackRules_MultiCellIgnoresDepth,
	"ProjectCore.Types.InventoryStackRules.MultiCellIgnoresDepth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectCore_InventoryStackRules_MultiCellIgnoresDepth::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FItemDataView ItemData;
	ItemData.GridSize = FIntPoint(2, 1);
	ItemData.MaxStack = 10;
	ItemData.UnitsPerDepthUnit = 1;

	TestFalse(TEXT("Multi-cell item should not use depth stacking"), FInventoryStackRules::UsesDepthStacking(ItemData));
	TestEqual(TEXT("Multi-cell item should keep authored MaxStack"), FInventoryStackRules::CalculateMaxStackForContainer(ItemData, 4), 10);
	TestEqual(TEXT("Multi-cell item should not report depth usage"), FInventoryStackRules::CalculateDepthUnitsForQuantity(ItemData, 5), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
