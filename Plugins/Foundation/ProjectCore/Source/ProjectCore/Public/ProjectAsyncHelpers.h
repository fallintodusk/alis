// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "TimerManager.h"

/**
 * Static helper functions for common async patterns in Project systems.
 *
 * Reduces boilerplate for timer-based delays, next-tick execution, and retry logic.
 * All helpers require valid UWorld* for timer manager access.
 *
 * Usage:
 *   FProjectAsyncHelpers::ExecuteAfterDelay(GetWorld(), 2.0f, []() { ... });
 *   FProjectAsyncHelpers::ExecuteNextTick(GetWorld(), []() { ... });
 */
class PROJECTCORE_API FProjectAsyncHelpers
{
public:
	/** Execute callback after delay (one-shot timer). Returns timer handle for cancellation. */
	static FTimerHandle ExecuteAfterDelay(UWorld* World, float DelaySeconds, TFunction<void()> Callback)
	{
		if (!World || !Callback)
		{
			return FTimerHandle();
		}

		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle, MoveTemp(Callback), DelaySeconds, false);
		return Handle;
	}

	/** Execute callback on next tick. Returns timer handle for cancellation. */
	static FTimerHandle ExecuteNextTick(UWorld* World, TFunction<void()> Callback)
	{
		if (!World || !Callback)
		{
			return FTimerHandle();
		}

		FTimerHandle Handle;
		FTimerDelegate Delegate;
		Delegate.BindLambda(Callback);
		World->GetTimerManager().SetTimerForNextTick(Delegate);
		return Handle;
	}

	/** Execute callback repeatedly at interval until cancelled. Returns timer handle. */
	static FTimerHandle ExecuteRepeating(UWorld* World, float IntervalSeconds, TFunction<void()> Callback)
	{
		if (!World || !Callback)
		{
			return FTimerHandle();
		}

		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle, MoveTemp(Callback), IntervalSeconds, true);
		return Handle;
	}

	/** Cancel timer if valid. Safe to call with invalid handle. */
	static void CancelTimer(UWorld* World, FTimerHandle& Handle)
	{
		if (World && Handle.IsValid())
		{
			World->GetTimerManager().ClearTimer(Handle);
			Handle.Invalidate();
		}
	}

	/** Check if timer is active. */
	static bool IsTimerActive(UWorld* World, const FTimerHandle& Handle)
	{
		return World && Handle.IsValid() && World->GetTimerManager().IsTimerActive(Handle);
	}

	/** Get remaining time on timer. Returns 0.0f if invalid. */
	static float GetTimerRemaining(UWorld* World, const FTimerHandle& Handle)
	{
		if (World && Handle.IsValid())
		{
			return World->GetTimerManager().GetTimerRemaining(Handle);
		}
		return 0.0f;
	}

	/** Execute callback with exponential backoff delay (for retry logic). */
	static FTimerHandle ExecuteWithBackoff(
		UWorld* World,
		int32 AttemptNumber,
		float BaseDelaySeconds,
		TFunction<void()> Callback)
	{
		if (!World || !Callback)
		{
			return FTimerHandle();
		}

		// Calculate delay: BaseDelay * 2^Attempt
		const float Delay = BaseDelaySeconds * FMath::Pow(2.0f, static_cast<float>(AttemptNumber));

		return ExecuteAfterDelay(World, Delay, Callback);
	}

	/** Execute callback after delay with success/failure result parameter. */
	static FTimerHandle ExecuteAfterDelayWithResult(
		UWorld* World,
		float DelaySeconds,
		bool bSuccess,
		TFunction<void(bool)> Callback)
	{
		if (!World || !Callback)
		{
			return FTimerHandle();
		}

		return ExecuteAfterDelay(World, DelaySeconds, [Callback, bSuccess]()
		{
			Callback(bSuccess);
		});
	}

	/**
	 * Execute callback on game thread (safe for async thread completion).
	 * Useful when async task completes on background thread but needs to update UObjects.
	 */
	static void ExecuteOnGameThread(TFunction<void()> Callback)
	{
		if (!Callback)
		{
			return;
		}

		if (IsInGameThread())
		{
			// Already on game thread, execute immediately
			Callback();
		}
		else
		{
			// Queue to game thread
			AsyncTask(ENamedThreads::GameThread, Callback);
		}
	}

	/** Execute callback on background thread (async task). */
	static void ExecuteAsync(TFunction<void()> Callback)
	{
		if (!Callback)
		{
			return;
		}

		AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, Callback);
	}

	/** Execute callback on game thread after async work completes. */
	static void ExecuteAsyncThenOnGameThread(
		TFunction<void()> AsyncWork,
		TFunction<void()> GameThreadCallback)
	{
		if (!AsyncWork || !GameThreadCallback)
		{
			return;
		}

		ExecuteAsync([AsyncWork, GameThreadCallback]()
		{
			// Do work on background thread
			AsyncWork();

			// Return to game thread for callback
			ExecuteOnGameThread(GameThreadCallback);
		});
	}
};
