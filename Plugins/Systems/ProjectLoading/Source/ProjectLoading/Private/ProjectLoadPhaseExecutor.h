// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Types/ProjectLoadRequest.h"
#include "Types/ProjectLoadPhaseState.h"
#include "Engine/StreamableManager.h" // FStreamableHandle - needed for ReleaseHandle() calls

class UProjectLoadingSubsystem;

/**
 * Error codes for loading failures.
 * Used for telemetry and debugging.
 */
namespace ProjectLoadingErrors
{
	constexpr int32 None = 0;
	constexpr int32 InvalidRequest = 1;
	constexpr int32 AssetResolutionFailed = 100;
	constexpr int32 MountContentFailed = 200;
	constexpr int32 PreloadFailed = 300;
	constexpr int32 FeatureActivationFailed = 400;
	constexpr int32 TravelFailed = 500;
	constexpr int32 WarmupFailed = 600;
	constexpr int32 Cancelled = 900;
	constexpr int32 Timeout = 901;
	constexpr int32 GenericError = 999;
}

/**
 * Retry configuration for phase execution.
 */
struct FProjectPhaseRetryConfig
{
	/** Enable retry on failure */
	bool bEnableRetry = true;

	/** Maximum number of retry attempts */
	int32 MaxRetries = 3;

	/** Base delay between retries in seconds */
	float BaseRetryDelay = 1.0f;

	/** Exponential backoff multiplier (1.0 = linear, 2.0 = exponential) */
	float BackoffMultiplier = 2.0f;

	/** Maximum retry delay in seconds */
	float MaxRetryDelay = 10.0f;

	/** Calculate delay for retry attempt */
	float GetRetryDelay(int32 AttemptNumber) const
	{
		const float Delay = BaseRetryDelay * FMath::Pow(BackoffMultiplier, static_cast<float>(AttemptNumber));
		return FMath::Min(Delay, MaxRetryDelay);
	}
};

/**
 * Runtime state that persists across pipeline phases.
 * Used to share data between phases (e.g., preload handles that need release after Travel).
 */
struct FLoadPipelineRuntime
{
	/**
	 * Preload handles from Phase 3 (PreloadCriticalAssets).
	 * Kept alive through Travel + Warmup phases to prevent GC.
	 * Released in Warmup phase after WP streaming has started.
	 *
	 * Why: StreamableManager releases handles after delegate fires if bManageActiveHandle=false.
	 * We use bManageActiveHandle=true and store handles here to control lifetime.
	 * Without this, preloaded assets become unloadable before Travel even starts!
	 */
	TArray<TSharedPtr<FStreamableHandle>> PreloadHandles;

	/** Release all preload handles (call after WP streaming started) */
	void ReleasePreloadHandles()
	{
		for (TSharedPtr<FStreamableHandle>& Handle : PreloadHandles)
		{
			if (Handle.IsValid())
			{
				Handle->ReleaseHandle();
			}
		}
		PreloadHandles.Reset();
	}
};

/**
 * Phase execution context passed to all phase executors.
 * Contains request data, progress callbacks, and cancellation token.
 */
struct FProjectPhaseContext
{
	/** The load request being executed */
	FLoadRequest Request;

	/** Owning subsystem for callbacks */
	TWeakObjectPtr<UProjectLoadingSubsystem> Subsystem;

	/** Current retry attempt (0 = first attempt) */
	int32 RetryAttempt = 0;

	/** Retry configuration */
	FProjectPhaseRetryConfig RetryConfig;

	/** Cancellation flag (checked by phases) */
	std::atomic<bool> bCancellationRequested{false};

	/** Progress callback (normalized 0.0 to 1.0 within phase) */
	TFunction<void(float Progress, const FText& StatusMessage)> OnProgress;

	/**
	 * Frame pump callback (optional, injected by UI layer).
	 * Called during blocking waits to allow UI updates without Systems depending on Slate.
	 * If not bound, blocking phases will complete but UI won't animate.
	 */
	TFunction<void()> OnPumpFrame;

	/**
	 * Runtime state shared across phases.
	 * Stores preload handles, intermediate results, etc.
	 */
	FLoadPipelineRuntime Runtime;

	/** Check if cancellation was requested */
	bool IsCancelled() const { return bCancellationRequested.load(); }

	/** Request cancellation */
	void RequestCancellation() { bCancellationRequested.store(true); }
};

/**
 * Result of phase execution.
 */
struct FProjectPhaseResult
{
	/** True if phase succeeded */
	bool bSuccess = false;

	/** True if phase was skipped (not an error) */
	bool bSkipped = false;

	/** Error message if failed */
	FText ErrorMessage;

	/** Error code for telemetry */
	int32 ErrorCode = ProjectLoadingErrors::None;

	/** Create success result */
	static FProjectPhaseResult Success()
	{
		FProjectPhaseResult Result;
		Result.bSuccess = true;
		return Result;
	}

	/** Create skipped result */
	static FProjectPhaseResult Skipped()
	{
		FProjectPhaseResult Result;
		Result.bSuccess = true;
		Result.bSkipped = true;
		return Result;
	}

	/** Create failure result */
	static FProjectPhaseResult Failure(const FText& InErrorMessage, int32 InErrorCode = ProjectLoadingErrors::GenericError)
	{
		FProjectPhaseResult Result;
		Result.bSuccess = false;
		Result.ErrorMessage = InErrorMessage;
		Result.ErrorCode = InErrorCode;
		return Result;
	}

	/** Create cancellation result */
	static FProjectPhaseResult Cancelled()
	{
		return Failure(NSLOCTEXT("ProjectLoading", "Cancelled", "Operation cancelled"), ProjectLoadingErrors::Cancelled);
	}
};

/**
 * Base class for phase executors.
 * Each loading phase (ResolveAssets, MountContent, etc.) has a concrete implementation.
 *
 * Executors are responsible for:
 * - Executing their phase logic
 * - Reporting progress via context callback
 * - Checking cancellation token
 * - Returning success/failure result
 */
class FProjectLoadPhaseExecutor
{
public:
	virtual ~FProjectLoadPhaseExecutor() = default;

	/**
	 * Execute this phase.
	 * @param Context Execution context with request data and callbacks
	 * @return Phase execution result (success/failure/skipped)
	 */
	virtual FProjectPhaseResult Execute(FProjectPhaseContext& Context) = 0;

	/**
	 * Get the phase this executor handles.
	 */
	virtual ELoadPhase GetPhase() const = 0;

	/**
	 * Get human-readable phase name.
	 */
	virtual FText GetPhaseName() const = 0;

	/**
	 * Check if this phase should be skipped for the given request.
	 * Default: never skip. Override to implement conditional skipping.
	 */
	virtual bool ShouldSkip(const FLoadRequest& Request) const
	{
		return false;
	}

	/**
	 * Check if this phase supports retry on failure.
	 * Default: true. Override to disable retry for specific phases.
	 */
	virtual bool SupportsRetry() const
	{
		return true;
	}

	/**
	 * Check if this phase is critical for loading success.
	 * Critical phases abort the pipeline on failure.
	 * Non-critical phases log errors but allow pipeline to continue with degraded functionality.
	 * Default: true (all phases critical). Override to mark specific phases as optional.
	 */
	virtual bool IsCritical() const
	{
		return true;
	}

	/**
	 * Check if this phase must run on the game thread.
	 * Some phases (e.g., ResolveAssets) call game-thread-only APIs like AssetManager.
	 * Default: false (runs on worker thread). Override to force game thread execution.
	 */
	virtual bool RequiresGameThread() const
	{
		return false;
	}

protected:
	/**
	 * Helper to report progress within phase.
	 * @param Context Execution context
	 * @param Progress Normalized progress (0.0 to 1.0)
	 * @param StatusMessage Human-readable status
	 */
	void ReportProgress(FProjectPhaseContext& Context, float Progress, const FText& StatusMessage)
	{
		if (Context.OnProgress)
		{
			Context.OnProgress(FMath::Clamp(Progress, 0.0f, 1.0f), StatusMessage);
		}
	}

	/**
	 * Helper to check for cancellation.
	 * @param Context Execution context
	 * @return True if cancelled
	 */
	bool CheckCancellation(const FProjectPhaseContext& Context) const
	{
		return Context.IsCancelled();
	}
};
