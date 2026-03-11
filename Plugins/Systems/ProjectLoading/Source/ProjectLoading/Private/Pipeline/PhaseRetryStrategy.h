// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProjectLoadPhaseExecutor.h"

class FLoadCancellationManager;

/**
 * FPhaseRetryStrategy
 *
 * Single Responsibility: Execute phases with retry logic and exponential backoff.
 *
 * Features:
 * - Configurable retry count and delays
 * - Exponential backoff with max delay cap
 * - Game thread dispatch for phases that require it
 * - Cancellation checking during retries
 * - Detailed retry logging for observability
 *
 * Retry Algorithm:
 * - Delay = BaseDelay * (BackoffMultiplier ^ AttemptNumber)
 * - Delay capped at MaxRetryDelay
 * - Checks cancellation between retries
 */
class FPhaseRetryStrategy
{
public:
	/**
	 * Construct retry strategy with cancellation manager reference.
	 *
	 * @param InCancellationManager Reference to cancellation manager for checking cancellation state
	 */
	explicit FPhaseRetryStrategy(TSharedPtr<FLoadCancellationManager> InCancellationManager);

	/**
	 * Execute a phase with retry logic.
	 *
	 * @param Executor The phase executor to run
	 * @param Context Execution context
	 * @return Phase execution result
	 */
	FProjectPhaseResult ExecuteWithRetry(FProjectLoadPhaseExecutor& Executor, FProjectPhaseContext& Context);

	/**
	 * Calculate retry delay for a given attempt.
	 *
	 * @param Config Retry configuration
	 * @param AttemptNumber Current attempt number (0-based)
	 * @return Delay in seconds
	 */
	static float CalculateRetryDelay(const FProjectPhaseRetryConfig& Config, int32 AttemptNumber);

private:
	/**
	 * Execute phase on game thread if required.
	 *
	 * @param Executor The phase executor
	 * @param Context Execution context
	 * @return Phase execution result
	 */
	FProjectPhaseResult ExecuteOnGameThreadIfNeeded(FProjectLoadPhaseExecutor& Executor, FProjectPhaseContext& Context);

	/**
	 * Wait for specified duration while checking for cancellation.
	 *
	 * @param Seconds Duration to wait
	 * @param Context Execution context for cancellation checking
	 * @return True if wait completed normally, false if cancelled
	 */
	bool WaitWithCancellationCheck(float Seconds, const FProjectPhaseContext& Context);

	/**
	 * Log retry attempt details.
	 */
	void LogRetryAttempt(const FProjectLoadPhaseExecutor& Executor, int32 Attempt, int32 MaxAttempts, float DelaySeconds);

	/**
	 * Log retry exhausted (all attempts failed).
	 */
	void LogRetryExhausted(const FProjectLoadPhaseExecutor& Executor, int32 TotalAttempts, const FProjectPhaseResult& LastResult);

	/** Cancellation manager reference */
	TSharedPtr<FLoadCancellationManager> CancellationManager;
};
