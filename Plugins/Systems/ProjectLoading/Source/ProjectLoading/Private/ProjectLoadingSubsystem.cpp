// Copyright ALIS. All Rights Reserved.

#include "ProjectLoadingSubsystem.h"

#include "ProjectLoadingLog.h"
#include "ProjectLoadPhaseExecutor.h"
#include "ProjectLoadPhaseExecutors.h"
#include "ProjectServiceLocator.h"
#include "Experience/ProjectExperienceRegistry.h"

// SOLID Components
#include "Validation/LoadRequestValidator.h"
#include "Events/LoadEventBroadcaster.h"
#include "Cancellation/LoadCancellationManager.h"
#include "Pipeline/LoadProgressTracker.h"
#include "Pipeline/PhaseRetryStrategy.h"
#include "Pipeline/LoadPipelineOrchestrator.h"
#include "Pipeline/LoadingObservability.h"
#include "Experience/InitialExperienceLoader.h"

#include "Async/Async.h"
#include "Engine/AssetManager.h"
#include "Experience/ProjectExperienceDescriptorBase.h"

#define LOCTEXT_NAMESPACE "ProjectLoadingSubsystem"

// Adapter to bridge UObject subsystem with TSharedPtr-based ServiceLocator
class FLoadingServiceAdapter : public ILoadingService
{
public:
	explicit FLoadingServiceAdapter(UProjectLoadingSubsystem* InSubsystem)
		: Subsystem(InSubsystem)
	{
	}

	virtual TSharedPtr<ILoadingHandle> StartLoad(const FLoadRequest& Request) override
	{
		if (UProjectLoadingSubsystem* Sub = Subsystem.Get())
		{
			return Sub->StartLoad(Request);
		}
		return nullptr;
	}

	virtual bool CancelActiveLoad(bool bForce = false) override
	{
		if (UProjectLoadingSubsystem* Sub = Subsystem.Get())
		{
			return Sub->CancelActiveLoad(bForce);
		}
		return false;
	}

	virtual bool IsLoadInProgress() const override
	{
		if (UProjectLoadingSubsystem* Sub = Subsystem.Get())
		{
			return Sub->IsLoadInProgress();
		}
		return false;
	}

	virtual TSharedPtr<ILoadingHandle> GetActiveLoadHandle() const override
	{
		if (UProjectLoadingSubsystem* Sub = Subsystem.Get())
		{
			return Sub->GetActiveLoadHandle();
		}
		return nullptr;
	}

	virtual bool BuildLoadRequestForExperience(FName ExperienceName, FLoadRequest& OutRequest, FText& OutError) override
	{
		if (UProjectLoadingSubsystem* Sub = Subsystem.Get())
		{
			return Sub->BuildLoadRequestForExperience(ExperienceName, OutRequest, OutError);
		}
		OutError = LOCTEXT("SubsystemNotAvailable", "Loading subsystem not available");
		return false;
	}

private:
	TWeakObjectPtr<UProjectLoadingSubsystem> Subsystem;
};

namespace ProjectLoading
{
	class FProjectLoadingHandle : public ILoadingHandle, public TSharedFromThis<FProjectLoadingHandle>
	{
	public:
		explicit FProjectLoadingHandle(TWeakObjectPtr<UProjectLoadingSubsystem> InOwner)
			: Owner(MoveTemp(InOwner))
		{
		}

		//~ILoadingHandle interface
		virtual ELoadingState GetState() const override { return State; }
		virtual float GetProgress() const override { return Progress; }
		virtual FName GetCurrentPhase() const override { return CurrentPhase; }
		virtual FText GetStatusMessage() const override { return StatusMessage; }
		virtual bool IsInProgress() const override { return State == ELoadingState::InProgress; }
		virtual bool IsCompleted() const override { return State == ELoadingState::Completed; }
		virtual bool IsFailed() const override { return State == ELoadingState::Failed; }
		virtual bool IsCancelled() const override { return State == ELoadingState::Cancelled; }
		virtual bool Cancel(bool bForce) override
		{
			if (State == ELoadingState::Completed || State == ELoadingState::Failed || State == ELoadingState::Cancelled)
			{
				return false;
			}

			bCancellationRequested = true;
			if (UProjectLoadingSubsystem* Subsystem = Owner.Get())
			{
				Subsystem->HandleCancellationRequest(bForce);
			}

			return true;
		}

		virtual FText GetErrorMessage() const override { return ErrorMessage; }
		virtual int32 GetErrorCode() const override { return ErrorCode; }

		void SetState(ELoadingState InState) { State = InState; }
		void SetProgress(float InProgress) { Progress = FMath::Clamp(InProgress, 0.0f, 1.0f); }
		void SetCurrentPhase(FName InPhase, FText InStatus)
		{
			CurrentPhase = InPhase;
			StatusMessage = MoveTemp(InStatus);
		}
		void SetError(int32 InErrorCode, FText InErrorMessage)
		{
			ErrorCode = InErrorCode;
			ErrorMessage = MoveTemp(InErrorMessage);
		}

		bool WasCancelRequested() const { return bCancellationRequested; }

	private:
		TWeakObjectPtr<UProjectLoadingSubsystem> Owner;
		ELoadingState State = ELoadingState::Idle;
		float Progress = 0.0f;
		FName CurrentPhase = NAME_None;
		FText StatusMessage;
		FText ErrorMessage;
		int32 ErrorCode = 0;
		bool bCancellationRequested = false;
	};
}

#if WITH_DEV_AUTOMATION_TESTS
// Test helper: builds a load request directly from a descriptor without registry lookup
PROJECTLOADING_API FLoadRequest BuildResolvedLoadRequest_ForTests(const UProjectExperienceDescriptorBase& Descriptor)
{
	FLoadRequest Request;
	Descriptor.BuildLoadRequest(Request);

	// Resolve map asset ID from soft path if available
	if (!Request.MapSoftPath.IsNull() && !Request.MapAssetId.IsValid())
	{
		UAssetManager& AssetManager = UAssetManager::Get();
		const FPrimaryAssetId MapId = AssetManager.GetPrimaryAssetIdForPath(Request.MapSoftPath);
		if (MapId.IsValid())
		{
			Request.MapAssetId = MapId;
		}
	}

	return Request;
}
#endif

UProjectLoadingSubsystem::UProjectLoadingSubsystem()
{
}

void UProjectLoadingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogProjectLoading, Display, TEXT("ProjectLoadingSubsystem: Initializing..."));

	// Initialize SOLID components
	InitializeComponents();

	// Initialize phase executors
	InitializePhaseExecutors();

	// Register as ILoadingService (DIP pattern)
	TSharedRef<ILoadingService> ServicePtr = MakeShared<FLoadingServiceAdapter>(this);
	FProjectServiceLocator::Register<ILoadingService>(ServicePtr);

	UE_LOG(LogProjectLoading, Display, TEXT("ProjectLoadingSubsystem: Initialized and registered as ILoadingService"));

	ScheduleInitialExperienceLoad();
}

void UProjectLoadingSubsystem::Deinitialize()
{
	UE_LOG(LogProjectLoading, Display, TEXT("ProjectLoadingSubsystem: Deinitializing..."));

	CancelActiveLoad(true);

	// Wait for pipeline task to complete
	if (PipelineTask.IsValid() && !PipelineTask->IsComplete())
	{
		UE_LOG(LogProjectLoading, Warning, TEXT("ProjectLoadingSubsystem: Waiting for pipeline task to complete..."));
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(PipelineTask);
	}

	// Shutdown components
	ShutdownPhaseExecutors();
	ShutdownComponents();

	// Unregister from ServiceLocator
	FProjectServiceLocator::Unregister<ILoadingService>();

	Super::Deinitialize();

	UE_LOG(LogProjectLoading, Display, TEXT("ProjectLoadingSubsystem: Deinitialized"));
}

void UProjectLoadingSubsystem::InitializeComponents()
{
	UE_LOG(LogProjectLoading, Verbose, TEXT("ProjectLoadingSubsystem: Initializing SOLID components..."));

	// Create cancellation manager
	CancellationManager = MakeShared<FLoadCancellationManager>();

	// Create progress tracker
	ProgressTracker = MakeShared<FLoadProgressTracker>();

	// Create retry strategy with cancellation manager
	RetryStrategy = MakeShared<FPhaseRetryStrategy>(CancellationManager);

	// Create event broadcaster and wire to subsystem delegates (single source of truth)
	EventBroadcaster = MakeShared<FLoadEventBroadcaster>();
	EventBroadcaster->Initialize(
		&OnLoadStarted,
		&OnProgress,
		&OnPhaseChanged,
		&OnCompleted,
		&OnFailed
	);

	// Create experience loader with registry
	UProjectExperienceRegistry* Registry = UProjectExperienceRegistry::Get();
	ExperienceLoader = MakeShared<FInitialExperienceLoader>(Registry);

	UE_LOG(LogProjectLoading, Verbose, TEXT("ProjectLoadingSubsystem: SOLID components initialized"));
}

void UProjectLoadingSubsystem::ShutdownComponents()
{
	UE_LOG(LogProjectLoading, Verbose, TEXT("ProjectLoadingSubsystem: Shutting down SOLID components..."));

	PipelineOrchestrator.Reset();
	ExperienceLoader.Reset();
	EventBroadcaster.Reset();
	RetryStrategy.Reset();
	ProgressTracker.Reset();
	CancellationManager.Reset();

	UE_LOG(LogProjectLoading, Verbose, TEXT("ProjectLoadingSubsystem: SOLID components shutdown"));
}

void UProjectLoadingSubsystem::InitializePhaseExecutors()
{
	UE_LOG(LogProjectLoading, Display, TEXT("ProjectLoadingSubsystem: Initializing phase executors..."));

	PhaseExecutors.Reset();

	// Create executors for each phase in order
	PhaseExecutors.Add(MakeShared<FResolveAssetsPhaseExecutor>());
	UE_LOG(LogProjectLoading, Verbose, TEXT("  [Phase 1] ResolveAssets (Retry=%s, Critical=%s)"),
		PhaseExecutors.Last()->SupportsRetry() ? TEXT("Yes") : TEXT("No"),
		PhaseExecutors.Last()->IsCritical() ? TEXT("Yes") : TEXT("No"));

	PhaseExecutors.Add(MakeShared<FMountContentPhaseExecutor>());
	UE_LOG(LogProjectLoading, Verbose, TEXT("  [Phase 2] MountContent (Retry=%s, Critical=%s)"),
		PhaseExecutors.Last()->SupportsRetry() ? TEXT("Yes") : TEXT("No"),
		PhaseExecutors.Last()->IsCritical() ? TEXT("Yes") : TEXT("No"));

	PhaseExecutors.Add(MakeShared<FPreloadCriticalAssetsPhaseExecutor>());
	UE_LOG(LogProjectLoading, Verbose, TEXT("  [Phase 3] PreloadCriticalAssets (Retry=%s, Critical=%s, GameThread=%s)"),
		PhaseExecutors.Last()->SupportsRetry() ? TEXT("Yes") : TEXT("No"),
		PhaseExecutors.Last()->IsCritical() ? TEXT("Yes") : TEXT("No"),
		PhaseExecutors.Last()->RequiresGameThread() ? TEXT("Yes") : TEXT("No"));

	PhaseExecutors.Add(MakeShared<FActivateFeaturesPhaseExecutor>());
	UE_LOG(LogProjectLoading, Verbose, TEXT("  [Phase 4] ActivateFeatures (Retry=%s, Critical=%s)"),
		PhaseExecutors.Last()->SupportsRetry() ? TEXT("Yes") : TEXT("No"),
		PhaseExecutors.Last()->IsCritical() ? TEXT("Yes") : TEXT("No"));

	PhaseExecutors.Add(MakeShared<FTravelPhaseExecutor>());
	UE_LOG(LogProjectLoading, Verbose, TEXT("  [Phase 5] Travel (Retry=%s, Critical=%s)"),
		PhaseExecutors.Last()->SupportsRetry() ? TEXT("Yes") : TEXT("No"),
		PhaseExecutors.Last()->IsCritical() ? TEXT("Yes") : TEXT("No"));

	PhaseExecutors.Add(MakeShared<FWarmupPhaseExecutor>());
	UE_LOG(LogProjectLoading, Verbose, TEXT("  [Phase 6] Warmup (Retry=%s, Critical=%s)"),
		PhaseExecutors.Last()->SupportsRetry() ? TEXT("Yes") : TEXT("No"),
		PhaseExecutors.Last()->IsCritical() ? TEXT("Yes") : TEXT("No"));

	UE_LOG(LogProjectLoading, Display, TEXT("ProjectLoadingSubsystem: Initialized %d phase executors"), PhaseExecutors.Num());
}

void UProjectLoadingSubsystem::ShutdownPhaseExecutors()
{
	UE_LOG(LogProjectLoading, Display, TEXT("ProjectLoadingSubsystem: Shutting down phase executors..."));
	PhaseExecutors.Reset();
}

void UProjectLoadingSubsystem::ScheduleInitialExperienceLoad()
{
	if (bInitialLoadScheduled)
	{
		return;
	}
	bInitialLoadScheduled = true;

	// Run on next tick to ensure registry and other subsystems are initialized
	if (UGameInstance* GI = GetGameInstance())
	{
		FTimerDelegate Delegate;
		Delegate.BindUObject(this, &UProjectLoadingSubsystem::StartInitialExperience);
		GI->GetTimerManager().SetTimerForNextTick(Delegate);
	}
	else
	{
		StartInitialExperience();
	}
}

void UProjectLoadingSubsystem::StartInitialExperience()
{
	static bool bInitialLoadStarted = false;
	if (bInitialLoadStarted)
	{
		return;
	}

	// In editor (including PIE), don't override the current map
	// Editor startup map and PIE map are managed by the editor, not by loading pipeline
	// Initial experience load only runs in standalone game / packaged builds
	if (GIsEditor)
	{
		UE_LOG(LogProjectLoading, Display,
			TEXT("ProjectLoadingSubsystem: Skipping initial experience in editor (map managed by editor)"));
		bInitialLoadStarted = true;
		return;
	}

	UE_LOG(LogProjectLoading, Display, TEXT("ProjectLoadingSubsystem: Starting initial experience load..."));

	if (!ExperienceLoader)
	{
		UE_LOG(LogProjectLoading, Error, TEXT("ProjectLoadingSubsystem: ExperienceLoader not initialized"));
		return;
	}

	// Resolve entry experience name
	const FName EntryExperience = ExperienceLoader->ResolveInitialExperienceName();
	if (EntryExperience.IsNone())
	{
		UE_LOG(LogProjectLoading, Warning, TEXT("ProjectLoadingSubsystem: Entry experience name is None - skipping"));
		return;
	}

	// Build load request
	FLoadRequest Request;
	FText Error;
	if (!ExperienceLoader->BuildLoadRequest(EntryExperience, Request, Error))
	{
		UE_LOG(LogProjectLoading, Error, TEXT("ProjectLoadingSubsystem: Failed to build load request: %s"), *Error.ToString());
		return;
	}

	// Check if already on target map
	if (ExperienceLoader->IsAlreadyOnTargetMap(Request, GetGameInstance()))
	{
		UE_LOG(LogProjectLoading, Display, TEXT("ProjectLoadingSubsystem: Already on target map - skipping redundant load"));
		bInitialLoadStarted = true;
		return;
	}

	// Start load
	StartLoad(Request);
	bInitialLoadStarted = true;
}

TSharedPtr<ILoadingHandle> UProjectLoadingSubsystem::StartLoad(const FLoadRequest& Request)
{
	UE_LOG(LogProjectLoading, Display, TEXT("ProjectLoadingSubsystem: StartLoad called for %s"), *Request.ToString());

	// Validate request
	TArray<FText> ValidationErrors;
	if (!FLoadRequestValidator::Validate(Request, ValidationErrors))
	{
		UE_LOG(LogProjectLoading, Error, TEXT("ProjectLoadingSubsystem: Request validation failed"));

		const FText CombinedError = FLoadRequestValidator::GetErrorSummary(ValidationErrors);
		if (EventBroadcaster)
		{
			EventBroadcaster->BroadcastFailed(Request, CombinedError, ProjectLoadingErrors::InvalidRequest);
		}
		return nullptr;
	}

	// Check if load already in progress
	if (IsLoadInProgress())
	{
		UE_LOG(LogProjectLoading, Warning, TEXT("ProjectLoadingSubsystem: Load already in progress"));
		return nullptr;
	}

	// Store active request
	ActiveRequest = Request;
	LoadStartTimeSeconds = FPlatformTime::Seconds();
	Telemetry = {};
	Telemetry.LoadStartTimeSeconds = LoadStartTimeSeconds;

	// Create loading handle
	const TSharedPtr<ProjectLoading::FProjectLoadingHandle> Handle = MakeShared<ProjectLoading::FProjectLoadingHandle>(TWeakObjectPtr<UProjectLoadingSubsystem>(this));
	ActiveHandle = Handle;
	Handle->SetState(ELoadingState::InProgress);
	Handle->SetCurrentPhase(StaticEnum<ELoadPhase>()->GetValueAsName(ELoadPhase::ResolveAssets),
		LOCTEXT("StartingPipeline", "Starting load pipeline"));
	Handle->SetProgress(0.0f);

	// Reset state for new load
	if (CancellationManager)
	{
		CancellationManager->Reset();
	}
	if (EventBroadcaster)
	{
		EventBroadcaster->Reset();
	}

	// Create pipeline orchestrator with all dependencies
	PipelineOrchestrator = MakeShared<FLoadPipelineOrchestrator>(
		TWeakObjectPtr<UProjectLoadingSubsystem>(this),
		PhaseExecutors,
		RetryStrategy,
		ProgressTracker,
		CancellationManager,
		EventBroadcaster
	);

	// Broadcast load started (through broadcaster - single source of truth)
	if (EventBroadcaster)
	{
		EventBroadcaster->BroadcastLoadStarted(Request);
		EventBroadcaster->BroadcastProgress(0.0f);
	}

	UE_LOG(LogProjectLoading, Display, TEXT("ProjectLoadingSubsystem: Launching async pipeline"));

	// Execute pipeline asynchronously
	PipelineTask = FFunctionGraphTask::CreateAndDispatchWhenReady(
		[this]()
		{
			ExecutePipeline();
		},
		TStatId(),
		nullptr,
		ENamedThreads::AnyBackgroundThreadNormalTask
	);

	return Handle;
}

bool UProjectLoadingSubsystem::CancelActiveLoad(bool bForce)
{
	if (!ActiveHandle.IsValid() || !ActiveHandle->IsInProgress())
	{
		return false;
	}

	HandleCancellationRequest(bForce);
	return true;
}

bool UProjectLoadingSubsystem::BuildLoadRequestForExperience(FName ExperienceName, FLoadRequest& OutRequest, FText& OutError)
{
	UE_LOG(LogProjectLoading, Display, TEXT("BuildLoadRequestForExperience: Building request for '%s'"), *ExperienceName.ToString());

	if (!ExperienceLoader.IsValid())
	{
		OutError = LOCTEXT("NoExperienceLoader", "Experience loader not initialized");
		UE_LOG(LogProjectLoading, Error, TEXT("BuildLoadRequestForExperience: %s"), *OutError.ToString());
		return false;
	}

	// Delegates to FInitialExperienceLoader which:
	// 1. Looks up descriptor from registry
	// 2. Calls EnsureAssetScans() for runtime asset registration
	// 3. Resolves MapAssetId from SoftPath via GetPrimaryAssetIdForPath()
	// 4. Populates CriticalAssetIds, WarmupAssetIds, etc.
	const bool bSuccess = ExperienceLoader->BuildLoadRequest(ExperienceName, OutRequest, OutError);

	if (bSuccess)
	{
		UE_LOG(LogProjectLoading, Display, TEXT("BuildLoadRequestForExperience: Success - MapAssetId=%s"),
			*OutRequest.MapAssetId.ToString());
	}
	else
	{
		UE_LOG(LogProjectLoading, Error, TEXT("BuildLoadRequestForExperience: Failed - %s"), *OutError.ToString());
	}

	return bSuccess;
}

void UProjectLoadingSubsystem::HandleCancellationRequest(bool bForce)
{
	UE_LOG(LogProjectLoading, Warning, TEXT("ProjectLoadingSubsystem: Cancellation requested (Force=%s)"),
		bForce ? TEXT("true") : TEXT("false"));

	// Log context
	if (CancellationManager && ActiveHandle.IsValid())
	{
		CancellationManager->LogCancellationContext(
			ActiveHandle->GetCurrentPhase().ToString(),
			ActiveHandle->GetProgress(),
			ActiveRequest.MapAssetId.IsValid() ? ActiveRequest.MapAssetId.ToString() : TEXT("<Unknown>")
		);
	}

	// Request cancellation
	if (CancellationManager)
	{
		CancellationManager->RequestCancellation(bForce);
	}

	// Force immediate cancellation
	if (bForce)
	{
		CompleteActiveLoad(ELoadingState::Cancelled,
			LOCTEXT("ForceCancelled", "Load cancelled by request (forced)"),
			ProjectLoadingErrors::Cancelled);
	}
}

void UProjectLoadingSubsystem::ExecutePipeline()
{
	UE_LOG(LogProjectLoading, Display, TEXT("ProjectLoadingSubsystem: Executing pipeline..."));

	if (!PipelineOrchestrator)
	{
		UE_LOG(LogProjectLoading, Error, TEXT("ProjectLoadingSubsystem: PipelineOrchestrator not initialized"));
		AsyncTask(ENamedThreads::GameThread, [this]()
		{
			CompleteActiveLoad(ELoadingState::Failed,
				LOCTEXT("OrchestratorError", "Pipeline orchestrator not initialized"),
				ProjectLoadingErrors::GenericError);
		});
		return;
	}

	// Execute pipeline
	const EPipelineResult Result = PipelineOrchestrator->Execute(ActiveRequest);

	// Get telemetry from orchestrator
	PipelineOrchestrator->GetTelemetry(Telemetry);

	// Complete on game thread
	AsyncTask(ENamedThreads::GameThread, [this, Result]()
	{
		switch (Result)
		{
		case EPipelineResult::Success:
			CompleteActiveLoad(ELoadingState::Completed);
			break;

		case EPipelineResult::Cancelled:
			CompleteActiveLoad(ELoadingState::Cancelled,
				LOCTEXT("PipelineCancelled", "Pipeline cancelled"),
				ProjectLoadingErrors::Cancelled);
			break;

		case EPipelineResult::Failed:
		default:
			CompleteActiveLoad(ELoadingState::Failed,
				LOCTEXT("PipelineFailed", "Pipeline failed"),
				ProjectLoadingErrors::GenericError);
			break;
		}
	});
}

void UProjectLoadingSubsystem::CompleteActiveLoad(ELoadingState FinalState, const FText& ErrorMessage, int32 ErrorCode)
{
	UE_LOG(LogProjectLoading, Display, TEXT("ProjectLoadingSubsystem: Completing load with state %s"),
		*StaticEnum<ELoadingState>()->GetNameStringByValue(static_cast<int64>(FinalState)));

	TSharedPtr<ProjectLoading::FProjectLoadingHandle> ConcreteHandle = StaticCastSharedPtr<ProjectLoading::FProjectLoadingHandle>(ActiveHandle);
	if (!ConcreteHandle.IsValid())
	{
		ActiveHandle.Reset();
		return;
	}

	// Update telemetry
	const double EndTime = FPlatformTime::Seconds();
	Telemetry.LoadEndTimeSeconds = EndTime;
	if (FinalState == ELoadingState::Completed)
	{
		Telemetry.TotalProgress = 1.0f;
	}

	// Update handle
	ConcreteHandle->SetState(FinalState);
	ConcreteHandle->SetProgress(Telemetry.TotalProgress);
	ConcreteHandle->SetError(ErrorCode, ErrorMessage);

	// Log completion
	const double Duration = Telemetry.LoadEndTimeSeconds - Telemetry.LoadStartTimeSeconds;
	UE_LOG(LogProjectLoading, Display, TEXT("ProjectLoadingSubsystem: Load completed in %.2fs (State=%s, Progress=%.1f%%)"),
		Duration,
		*StaticEnum<ELoadingState>()->GetNameStringByValue(static_cast<int64>(FinalState)),
		Telemetry.TotalProgress * 100.0f);

	// Broadcast appropriate event (through broadcaster - single source of truth)
	if (EventBroadcaster)
	{
		switch (FinalState)
		{
		case ELoadingState::Completed:
			EventBroadcaster->BroadcastProgress(1.0f);
			EventBroadcaster->BroadcastCompleted(ActiveRequest, Telemetry);
			break;

		case ELoadingState::Cancelled:
		{
			const FText CancelText = ErrorMessage.IsEmpty() ? LOCTEXT("LoadCancelled", "Load cancelled") : ErrorMessage;
			EventBroadcaster->BroadcastFailed(ActiveRequest, CancelText, ErrorCode);
			break;
		}

		case ELoadingState::Failed:
		default:
			EventBroadcaster->BroadcastFailed(ActiveRequest, ErrorMessage, ErrorCode);
			break;
		}
	}

	// Reset state
	ActiveHandle.Reset();
	ActiveRequest = FLoadRequest();
	PipelineOrchestrator.Reset();
}

#undef LOCTEXT_NAMESPACE
