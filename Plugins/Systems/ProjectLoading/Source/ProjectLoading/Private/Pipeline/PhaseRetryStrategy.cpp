// Copyright ALIS. All Rights Reserved.

#include "Pipeline/PhaseRetryStrategy.h"
#include "Cancellation/LoadCancellationManager.h"
#include "ProjectLoadingLog.h"
#include "Async/Async.h"

FPhaseRetryStrategy::FPhaseRetryStrategy(TSharedPtr<FLoadCancellationManager> InCancellationManager)
	: CancellationManager(MoveTemp(InCancellationManager))
{
}

FProjectPhaseResult FPhaseRetryStrategy::ExecuteWithRetry(FProjectLoadPhaseExecutor& Executor, FProjectPhaseContext& Context)
{
	const bool bSupportsRetry = Executor.SupportsRetry() && Context.RetryConfig.bEnableRetry;
	const int32 MaxAttempts = bSupportsRetry ? (Context.RetryConfig.MaxRetries + 1) : 1;

	FProjectPhaseResult LastResult;

	for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
	{
		Context.RetryAttempt = Attempt;

		// Log retry attempt (skip first attempt)
		if (Attempt > 0)
		{
			LogRetryAttempt(Executor, Attempt, Context.RetryConfig.MaxRetries, 0.0f);
		}

		// Execute phase
		LastResult = ExecuteOnGameThreadIfNeeded(Executor, Context);

		// Check for success
		if (LastResult.bSuccess)
		{
			if (LastResult.bSkipped)
			{
				UE_LOG(LogProjectLoading, Display, TEXT("PhaseRetryStrategy: Phase %s skipped"),
					*Executor.GetPhaseName().ToString());
			}
			else
			{
				UE_LOG(LogProjectLoading, Display, TEXT("PhaseRetryStrategy: Phase %s succeeded%s"),
					*Executor.GetPhaseName().ToString(),
					Attempt > 0 ? *FString::Printf(TEXT(" (after %d retries)"), Attempt) : TEXT(""));
			}
			return LastResult;
		}

		// Phase failed - log error
		UE_LOG(LogProjectLoading, Error, TEXT("PhaseRetryStrategy: Phase %s failed: %s (ErrorCode: %d)"),
			*Executor.GetPhaseName().ToString(),
			*LastResult.ErrorMessage.ToString(),
			LastResult.ErrorCode);

		// Check if we should retry
		const bool bIsLastAttempt = (Attempt >= MaxAttempts - 1);
		const bool bWasCancelled = (LastResult.ErrorCode == ProjectLoadingErrors::Cancelled);

		if (bIsLastAttempt || !bSupportsRetry || bWasCancelled)
		{
			if (bWasCancelled)
			{
				UE_LOG(LogProjectLoading, Warning, TEXT("PhaseRetryStrategy: Phase %s cancelled, aborting retries"),
					*Executor.GetPhaseName().ToString());
			}
			else if (!bSupportsRetry)
			{
				UE_LOG(LogProjectLoading, Warning, TEXT("PhaseRetryStrategy: Phase %s does not support retry"),
					*Executor.GetPhaseName().ToString());
			}
			else
			{
				LogRetryExhausted(Executor, MaxAttempts, LastResult);
			}

			return LastResult;
		}

		// Calculate retry delay with exponential backoff
		const float RetryDelay = CalculateRetryDelay(Context.RetryConfig, Attempt);
		LogRetryAttempt(Executor, Attempt + 1, Context.RetryConfig.MaxRetries, RetryDelay);

		UE_LOG(LogProjectLoading, Display, TEXT("PhaseRetryStrategy: Waiting %.2fs before retry..."), RetryDelay);

		// Wait with cancellation checking
		if (!WaitWithCancellationCheck(RetryDelay, Context))
		{
			UE_LOG(LogProjectLoading, Warning, TEXT("PhaseRetryStrategy: Cancelled during retry delay"));
			return FProjectPhaseResult::Cancelled();
		}
	}

	// Should not reach here
	return LastResult;
}

float FPhaseRetryStrategy::CalculateRetryDelay(const FProjectPhaseRetryConfig& Config, int32 AttemptNumber)
{
	return Config.GetRetryDelay(AttemptNumber);
}

FProjectPhaseResult FPhaseRetryStrategy::ExecuteOnGameThreadIfNeeded(FProjectLoadPhaseExecutor& Executor, FProjectPhaseContext& Context)
{
	if (Executor.RequiresGameThread() && !IsInGameThread())
	{
		UE_LOG(LogProjectLoading, Display, TEXT("PhaseRetryStrategy: Dispatching phase %s to game thread"),
			*Executor.GetPhaseName().ToString());

		FProjectPhaseResult Result;
		FEvent* CompletionEvent = FPlatformProcess::GetSynchEventFromPool();

		AsyncTask(ENamedThreads::GameThread, [&Executor, &Context, &Result, CompletionEvent]()
		{
			Result = Executor.Execute(Context);
			CompletionEvent->Trigger();
		});

		CompletionEvent->Wait();
		FPlatformProcess::ReturnSynchEventToPool(CompletionEvent);

		return Result;
	}

	// Execute directly on current thread
	return Executor.Execute(Context);
}

bool FPhaseRetryStrategy::WaitWithCancellationCheck(float Seconds, const FProjectPhaseContext& Context)
{
	constexpr float CheckIntervalSeconds = 0.1f;
	float RemainingSeconds = Seconds;

	while (RemainingSeconds > 0.0f)
	{
		// Check cancellation
		if (Context.IsCancelled() || (CancellationManager && CancellationManager->IsCancellationRequested()))
		{
			return false;
		}

		// Sleep for interval or remaining time
		const float SleepTime = FMath::Min(CheckIntervalSeconds, RemainingSeconds);
		FPlatformProcess::Sleep(SleepTime);
		RemainingSeconds -= SleepTime;
	}

	return true;
}

void FPhaseRetryStrategy::LogRetryAttempt(const FProjectLoadPhaseExecutor& Executor, int32 Attempt, int32 MaxAttempts, float DelaySeconds)
{
	UE_LOG(LogProjectLoading, Warning, TEXT("PhaseRetryStrategy: Retry attempt %d/%d for phase %s%s"),
		Attempt,
		MaxAttempts,
		*Executor.GetPhaseName().ToString(),
		DelaySeconds > 0.0f ? *FString::Printf(TEXT(" (delay: %.1fs)"), DelaySeconds) : TEXT(""));
}

void FPhaseRetryStrategy::LogRetryExhausted(const FProjectLoadPhaseExecutor& Executor, int32 TotalAttempts, const FProjectPhaseResult& LastResult)
{
	UE_LOG(LogProjectLoading, Error, TEXT("PhaseRetryStrategy: Phase %s exhausted all %d retry attempts"),
		*Executor.GetPhaseName().ToString(),
		TotalAttempts);

	UE_LOG(LogProjectLoading, Error, TEXT("PhaseRetryStrategy: Final error: %s (Code: %d)"),
		*LastResult.ErrorMessage.ToString(),
		LastResult.ErrorCode);
}
