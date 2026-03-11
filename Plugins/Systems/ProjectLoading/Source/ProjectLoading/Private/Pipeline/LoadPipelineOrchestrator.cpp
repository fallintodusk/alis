// Copyright ALIS. All Rights Reserved.

#include "Pipeline/LoadPipelineOrchestrator.h"
#include "Pipeline/PhaseRetryStrategy.h"
#include "Pipeline/LoadProgressTracker.h"
#include "Cancellation/LoadCancellationManager.h"
#include "Events/LoadEventBroadcaster.h"
#include "ProjectLoadPhaseExecutor.h"
#include "ProjectLoadingLog.h"
#include "ProjectLoadingSubsystem.h"
#include "Async/Async.h"
#include "Misc/ScopeExit.h" // ON_SCOPE_EXIT for handle cleanup safety net

FLoadPipelineOrchestrator::FLoadPipelineOrchestrator(
	TWeakObjectPtr<UProjectLoadingSubsystem> InSubsystem,
	TArray<TSharedPtr<FProjectLoadPhaseExecutor>>& InExecutors,
	TSharedPtr<FPhaseRetryStrategy> InRetryStrategy,
	TSharedPtr<FLoadProgressTracker> InProgressTracker,
	TSharedPtr<FLoadCancellationManager> InCancellationManager,
	TSharedPtr<FLoadEventBroadcaster> InEventBroadcaster)
	: Subsystem(InSubsystem)
	, Executors(InExecutors)
	, RetryStrategy(MoveTemp(InRetryStrategy))
	, ProgressTracker(MoveTemp(InProgressTracker))
	, CancellationManager(MoveTemp(InCancellationManager))
	, EventBroadcaster(MoveTemp(InEventBroadcaster))
{
}

EPipelineResult FLoadPipelineOrchestrator::Execute(const FLoadRequest& Request)
{
	ActiveRequest = Request;

	// Start observability tracing
	LoadingObservability::FTraceContext TraceContext = LoadingObservability::FTraceContext::Generate(Request.ToString());
	Tracer.StartTrace(TraceContext);

	UE_LOG(LogProjectLoading, Display, TEXT("PipelineOrchestrator: Starting pipeline execution [%s]"), *TraceContext.GetShortId());

	// Initialize progress tracker with phases
	TArray<ELoadPhase> Phases;
	for (const TSharedPtr<FProjectLoadPhaseExecutor>& Executor : Executors)
	{
		if (Executor.IsValid())
		{
			Phases.Add(Executor->GetPhase());
		}
	}
	ProgressTracker->Reset(Phases);

	// Create execution context
	FProjectPhaseContext Context;
	InitializeContext(Context, Request);

	// CRITICAL: Always release preload handles when pipeline exits (success, failure, or cancel)
	// This prevents memory leaks if pipeline aborts before Warmup phase runs
	// Handles are normally released in Warmup, but this is a safety net for abnormal exits
	ON_SCOPE_EXIT
	{
		if (Context.Runtime.PreloadHandles.Num() > 0)
		{
			UE_LOG(LogProjectLoading, Warning, TEXT("PipelineOrchestrator: Releasing %d orphaned preload handle(s) on scope exit"),
				Context.Runtime.PreloadHandles.Num());
			Context.Runtime.ReleasePreloadHandles();
		}
	};

	// Execute each phase in sequence
	bool bPipelineSuccess = true;
	FString FailureReason;

	for (const TSharedPtr<FProjectLoadPhaseExecutor>& Executor : Executors)
	{
		if (!Executor.IsValid())
		{
			UE_LOG(LogProjectLoading, Error, TEXT("PipelineOrchestrator: Invalid executor encountered"));
			bPipelineSuccess = false;
			FailureReason = TEXT("Invalid phase executor");
			break;
		}

		// Check for cancellation before each phase
		if (Context.IsCancelled() || (CancellationManager && CancellationManager->IsCancellationRequested()))
		{
			UE_LOG(LogProjectLoading, Warning, TEXT("PipelineOrchestrator: Cancelled before phase %s"),
				*Executor->GetPhaseName().ToString());

			Tracer.CompleteCancelled();
			return EPipelineResult::Cancelled;
		}

		// Execute phase
		if (!ExecutePhase(*Executor, Context))
		{
			// Check if this was a cancellation
			if (Context.IsCancelled() || (CancellationManager && CancellationManager->IsCancellationRequested()))
			{
				Tracer.CompleteCancelled();
				return EPipelineResult::Cancelled;
			}

			// Critical phase failure - abort
			if (Executor->IsCritical())
			{
				bPipelineSuccess = false;
				FailureReason = FString::Printf(TEXT("Critical phase %s failed"), *Executor->GetPhaseName().ToString());
				break;
			}

			// Non-critical failure - continue with degraded functionality
			UE_LOG(LogProjectLoading, Warning, TEXT("PipelineOrchestrator: Continuing after non-critical phase %s failure"),
				*Executor->GetPhaseName().ToString());
		}
	}

	// Complete tracing and return result
	if (bPipelineSuccess)
	{
		UE_LOG(LogProjectLoading, Display, TEXT("PipelineOrchestrator: Pipeline completed successfully"));
		Tracer.CompleteSuccess();
		return EPipelineResult::Success;
	}
	else
	{
		UE_LOG(LogProjectLoading, Error, TEXT("PipelineOrchestrator: Pipeline failed: %s"), *FailureReason);
		Tracer.CompleteFailed(FailureReason);
		return EPipelineResult::Failed;
	}
}

void FLoadPipelineOrchestrator::GetTelemetry(FProjectLoadTelemetry& OutTelemetry) const
{
	if (ProgressTracker)
	{
		ProgressTracker->FillTelemetry(OutTelemetry);
	}
}

void FLoadPipelineOrchestrator::InitializeContext(FProjectPhaseContext& Context, const FLoadRequest& Request)
{
	Context.Request = Request;
	Context.Subsystem = Subsystem; // Pass subsystem for world context access
	Context.bCancellationRequested.store(false);
	Context.RetryAttempt = 0;

	// Configure retry settings
	Context.RetryConfig.bEnableRetry = true;
	Context.RetryConfig.MaxRetries = 3;
	Context.RetryConfig.BaseRetryDelay = 1.0f;
	Context.RetryConfig.BackoffMultiplier = 2.0f;

	// Wire up progress callback
	Context.OnProgress = [this](float Progress, const FText& StatusMessage)
	{
		// Record progress for tracing
		Tracer.RecordProgress(Progress, StatusMessage.ToString());

		// Get current phase from progress tracker
		const ELoadPhase CurrentPhase = ProgressTracker ? ProgressTracker->GetCurrentActivePhase() : ELoadPhase::None;

		if (CurrentPhase != ELoadPhase::None)
		{
			// Update progress tracker
			if (ProgressTracker)
			{
				ProgressTracker->UpdatePhaseState(CurrentPhase, EPhaseState::InProgress, Progress, StatusMessage);
			}

			// Broadcast to listeners (thread-safe)
			BroadcastPhaseState(CurrentPhase, EPhaseState::InProgress, Progress, StatusMessage);
		}
	};

	// Wire up frame pump callback (injected by UI layer for Slate pumping)
	// This allows blocking phases to update UI without Systems depending on Slate
	if (UProjectLoadingSubsystem* LoadingSubsystem = Subsystem.Get())
	{
		Context.OnPumpFrame = LoadingSubsystem->GetPumpFrameCallback();
	}
}

bool FLoadPipelineOrchestrator::ExecutePhase(FProjectLoadPhaseExecutor& Executor, FProjectPhaseContext& Context)
{
	const ELoadPhase Phase = Executor.GetPhase();
	const FText PhaseName = Executor.GetPhaseName();

	// Check if phase should be skipped
	if (Executor.ShouldSkip(Context.Request))
	{
		UE_LOG(LogProjectLoading, Verbose, TEXT("PipelineOrchestrator: Skipping phase %s"), *PhaseName.ToString());

		Tracer.BeginPhase(Phase);
		Tracer.EndPhaseSkipped(Phase);

		if (ProgressTracker)
		{
			ProgressTracker->UpdatePhaseState(Phase, EPhaseState::Skipped, 1.0f,
				FText::Format(NSLOCTEXT("ProjectLoading", "PhaseSkipped", "{0} skipped"), PhaseName));
		}

		BroadcastPhaseState(Phase, EPhaseState::Skipped, 1.0f,
			FText::Format(NSLOCTEXT("ProjectLoading", "PhaseSkipped", "{0} skipped"), PhaseName));

		return true;
	}

	// Start phase execution
	UE_LOG(LogProjectLoading, Verbose, TEXT("PipelineOrchestrator: Executing phase %s"), *PhaseName.ToString());
	Tracer.BeginPhase(Phase);

	if (ProgressTracker)
	{
		ProgressTracker->StartPhaseTiming(Phase);
		ProgressTracker->UpdatePhaseState(Phase, EPhaseState::InProgress, 0.0f,
			FText::Format(NSLOCTEXT("ProjectLoading", "PhaseStarting", "Starting {0}..."), PhaseName));
	}

	BroadcastPhaseState(Phase, EPhaseState::InProgress, 0.0f,
		FText::Format(NSLOCTEXT("ProjectLoading", "PhaseStarting", "Starting {0}..."), PhaseName));

	// Execute with retry strategy
	FProjectPhaseResult Result;
	if (RetryStrategy)
	{
		Result = RetryStrategy->ExecuteWithRetry(Executor, Context);
	}
	else
	{
		// Fallback: execute directly without retry
		Result = Executor.Execute(Context);
	}

	// Handle result
	HandlePhaseResult(Executor, Result, Context);

	return Result.bSuccess;
}

void FLoadPipelineOrchestrator::HandlePhaseResult(FProjectLoadPhaseExecutor& Executor, const FProjectPhaseResult& Result, FProjectPhaseContext& Context)
{
	const ELoadPhase Phase = Executor.GetPhase();
	const FText PhaseName = Executor.GetPhaseName();

	if (ProgressTracker)
	{
		ProgressTracker->StopPhaseTiming(Phase);
	}

	if (Result.bSuccess)
	{
		if (Result.bSkipped)
		{
			Tracer.EndPhaseSkipped(Phase);

			if (ProgressTracker)
			{
				ProgressTracker->UpdatePhaseState(Phase, EPhaseState::Skipped, 1.0f,
					FText::Format(NSLOCTEXT("ProjectLoading", "PhaseSkipped", "{0} skipped"), PhaseName));
			}

			BroadcastPhaseState(Phase, EPhaseState::Skipped, 1.0f,
				FText::Format(NSLOCTEXT("ProjectLoading", "PhaseSkipped", "{0} skipped"), PhaseName));
		}
		else
		{
			Tracer.EndPhaseSuccess(Phase);

			if (ProgressTracker)
			{
				ProgressTracker->UpdatePhaseState(Phase, EPhaseState::Completed, 1.0f,
					FText::Format(NSLOCTEXT("ProjectLoading", "PhaseCompleted", "{0} completed"), PhaseName));
			}

			BroadcastPhaseState(Phase, EPhaseState::Completed, 1.0f,
				FText::Format(NSLOCTEXT("ProjectLoading", "PhaseCompleted", "{0} completed"), PhaseName));
		}

		UE_LOG(LogProjectLoading, Display, TEXT("PipelineOrchestrator: Phase %s succeeded"), *PhaseName.ToString());
	}
	else
	{
		Tracer.EndPhaseFailed(Phase, Result.ErrorCode, Result.ErrorMessage.ToString());

		if (ProgressTracker)
		{
			FLoadPhaseState* PhaseState = ProgressTracker->GetPhaseStateMutable(Phase);
			if (PhaseState)
			{
				PhaseState->State = EPhaseState::Failed;
				PhaseState->ErrorMessage = Result.ErrorMessage;
				PhaseState->ErrorCode = Result.ErrorCode;
			}
		}

		BroadcastPhaseState(Phase, EPhaseState::Failed, 0.0f, Result.ErrorMessage);

		UE_LOG(LogProjectLoading, Error, TEXT("PipelineOrchestrator: Phase %s failed: %s (Code: %d)"),
			*PhaseName.ToString(),
			*Result.ErrorMessage.ToString(),
			Result.ErrorCode);
	}
}

bool FLoadPipelineOrchestrator::ShouldAbortPipeline(const FProjectLoadPhaseExecutor& Executor, const FProjectPhaseResult& Result) const
{
	// Abort if phase failed and is critical
	return !Result.bSuccess && Executor.IsCritical();
}

void FLoadPipelineOrchestrator::BroadcastPhaseState(ELoadPhase Phase, EPhaseState State, float Progress, const FText& StatusMessage)
{
	if (!EventBroadcaster)
	{
		return;
	}

	// Build phase state for broadcast
	FLoadPhaseState PhaseState(Phase);
	PhaseState.State = State;
	PhaseState.Progress = Progress;
	PhaseState.StatusMessage = StatusMessage;

	// Calculate overall progress
	const float OverallProgress = ProgressTracker ? ProgressTracker->CalculateOverallProgress() : 0.0f;

	// Broadcast (ensures game thread)
	EventBroadcaster->BroadcastPhaseChanged(PhaseState, OverallProgress);
	EventBroadcaster->BroadcastProgress(OverallProgress);
}
