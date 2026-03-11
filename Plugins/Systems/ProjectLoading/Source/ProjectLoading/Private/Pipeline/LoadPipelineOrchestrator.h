// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Types/ProjectLoadRequest.h"
#include "Types/ProjectLoadPhaseState.h"
#include "Pipeline/LoadingObservability.h"
#include "ProjectLoadPhaseExecutor.h"

// Forward declarations
class FPhaseRetryStrategy;
class FLoadProgressTracker;
class FLoadCancellationManager;
class FLoadEventBroadcaster;
class UProjectLoadingSubsystem;
struct FProjectLoadTelemetry;

/**
 * Pipeline execution result
 */
enum class EPipelineResult : uint8
{
	Success,
	Failed,
	Cancelled
};

/**
 * FLoadPipelineOrchestrator
 *
 * Single Responsibility: Execute the loading pipeline (sequence of phases).
 *
 * Features:
 * - Sequential phase execution
 * - Cancellation checking between phases
 * - Integration with retry strategy
 * - Progress tracking and event broadcasting
 * - Critical vs non-critical phase handling
 * - Full observability with structured tracing
 *
 * Architecture:
 * - Coordinates all SOLID components
 * - Delegates specific responsibilities to focused components
 * - Provides clean async execution interface
 *
 * Thread Model:
 * - ExecutePipeline() runs on background thread
 * - Progress/phase events dispatched to game thread via EventBroadcaster
 * - Individual phases may dispatch to game thread if required
 */
class FLoadPipelineOrchestrator
{
public:
	/**
	 * Construct orchestrator with all required dependencies.
	 *
	 * @param InSubsystem Weak reference to the owning subsystem (for world context)
	 */
	FLoadPipelineOrchestrator(
		TWeakObjectPtr<UProjectLoadingSubsystem> InSubsystem,
		TArray<TSharedPtr<FProjectLoadPhaseExecutor>>& InExecutors,
		TSharedPtr<FPhaseRetryStrategy> InRetryStrategy,
		TSharedPtr<FLoadProgressTracker> InProgressTracker,
		TSharedPtr<FLoadCancellationManager> InCancellationManager,
		TSharedPtr<FLoadEventBroadcaster> InEventBroadcaster);

	/**
	 * Execute the full pipeline.
	 * Should be called from a background thread.
	 *
	 * @param Request The load request to execute
	 * @return Pipeline execution result
	 */
	EPipelineResult Execute(const FLoadRequest& Request);

	/**
	 * Get telemetry data after execution.
	 *
	 * @param OutTelemetry Telemetry structure to populate
	 */
	void GetTelemetry(FProjectLoadTelemetry& OutTelemetry) const;

	/**
	 * Get the tracer for observability data.
	 */
	const LoadingObservability::FLoadingTracer& GetTracer() const { return Tracer; }

private:
	/**
	 * Initialize phase context for execution.
	 */
	void InitializeContext(FProjectPhaseContext& Context, const FLoadRequest& Request);

	/**
	 * Execute a single phase with all handling.
	 *
	 * @param Executor The phase executor
	 * @param Context Execution context
	 * @return True if phase succeeded or was skipped, false if failed
	 */
	bool ExecutePhase(FProjectLoadPhaseExecutor& Executor, FProjectPhaseContext& Context);

	/**
	 * Handle phase completion (success, skip, or failure).
	 */
	void HandlePhaseResult(FProjectLoadPhaseExecutor& Executor, const FProjectPhaseResult& Result, FProjectPhaseContext& Context);

	/**
	 * Check if pipeline should abort based on phase result.
	 */
	bool ShouldAbortPipeline(const FProjectLoadPhaseExecutor& Executor, const FProjectPhaseResult& Result) const;

	/**
	 * Broadcast phase state change to listeners.
	 */
	void BroadcastPhaseState(ELoadPhase Phase, EPhaseState State, float Progress, const FText& StatusMessage);

	// Dependencies (injected via constructor)
	TWeakObjectPtr<UProjectLoadingSubsystem> Subsystem;
	TArray<TSharedPtr<FProjectLoadPhaseExecutor>>& Executors;
	TSharedPtr<FPhaseRetryStrategy> RetryStrategy;
	TSharedPtr<FLoadProgressTracker> ProgressTracker;
	TSharedPtr<FLoadCancellationManager> CancellationManager;
	TSharedPtr<FLoadEventBroadcaster> EventBroadcaster;

	// Observability
	LoadingObservability::FLoadingTracer Tracer;

	// Active request (for error reporting)
	FLoadRequest ActiveRequest;
};
