// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include <atomic>

/**
 * FLoadCancellationManager
 *
 * Single Responsibility: Manage cancellation state for load operations.
 *
 * Features:
 * - Thread-safe cancellation token
 * - Force vs graceful cancellation modes
 * - Detailed cancellation logging with context
 * - Stack trace capture for debugging unexpected cancellations
 *
 * Thread Safety:
 * - All methods are thread-safe
 * - Uses std::atomic for lock-free access
 */
class FLoadCancellationManager
{
public:
	FLoadCancellationManager();

	/**
	 * Reset cancellation state for a new load operation.
	 * Must be called at the start of each load.
	 */
	void Reset();

	/**
	 * Request cancellation of the current load.
	 *
	 * @param bForce If true, abort immediately. If false, graceful cancellation after current phase.
	 */
	void RequestCancellation(bool bForce = false);

	/**
	 * Check if cancellation has been requested.
	 * Thread-safe, can be called from any thread.
	 *
	 * @return True if cancellation was requested
	 */
	bool IsCancellationRequested() const;

	/**
	 * Check if force cancellation was requested.
	 *
	 * @return True if force cancellation (immediate abort) was requested
	 */
	bool IsForceCancellation() const;

	/**
	 * Log detailed cancellation context for debugging.
	 * Captures caller stack trace and current state.
	 *
	 * @param CurrentPhase Name of the phase when cancellation occurred
	 * @param Progress Current progress percentage
	 * @param MapPath Target map being loaded
	 */
	void LogCancellationContext(const FString& CurrentPhase, float Progress, const FString& MapPath);

	/**
	 * Get cancellation timestamp (0 if not cancelled).
	 */
	double GetCancellationTime() const;

	/**
	 * Get reason string for cancellation.
	 */
	FString GetCancellationReason() const;

private:
	/** Cancellation requested flag (any type) */
	std::atomic<bool> bCancellationRequested{false};

	/** Force cancellation flag (immediate abort) */
	std::atomic<bool> bForceCancellation{false};

	/** Time when cancellation was requested */
	std::atomic<double> CancellationTimeSeconds{0.0};

	/** Reason for cancellation (for telemetry) */
	FString CancellationReason;

	/** Critical section for reason string access */
	mutable FCriticalSection ReasonLock;
};
