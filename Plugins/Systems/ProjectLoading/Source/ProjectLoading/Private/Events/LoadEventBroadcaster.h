// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Types/ProjectLoadRequest.h"
#include "Types/ProjectLoadPhaseState.h"
#include "Async/Async.h"

// Forward declarations
struct FProjectLoadTelemetry;

// Forward declare the subsystem delegate types (Blueprint-assignable)
// These are DECLARE_DYNAMIC_MULTICAST_DELEGATE_* which are different from TMulticastDelegate
class FProjectLoadStartedSignature;
class FProjectLoadPhaseChangedSignature;
class FProjectLoadProgressSignature;
class FProjectLoadCompletedSignature;
class FProjectLoadFailedSignature;

/**
 * FLoadEventBroadcaster
 *
 * Single Responsibility: Broadcast loading events to listeners.
 *
 * Features:
 * - Thread-safe broadcasting (ensures game thread execution)
 * - Centralized event dispatch
 * - Observability logging for each event
 *
 * Events:
 * - OnLoadStarted: Load operation began
 * - OnProgress: Progress percentage changed
 * - OnPhaseChanged: Phase state changed
 * - OnCompleted: Load operation completed successfully
 * - OnFailed: Load operation failed
 *
 * Note: Uses void* for delegate pointers to avoid circular include with subsystem.
 * The actual broadcast is done via reinterpret_cast in the .cpp file which includes
 * the full subsystem header.
 */
class FLoadEventBroadcaster
{
public:
	/**
	 * Initialize broadcaster with delegate references.
	 * Delegates are owned by the subsystem; we just broadcast through them.
	 *
	 * @param InOnLoadStarted Pointer to FProjectLoadStartedSignature
	 * @param InOnProgress Pointer to FProjectLoadProgressSignature
	 * @param InOnPhaseChanged Pointer to FProjectLoadPhaseChangedSignature
	 * @param InOnCompleted Pointer to FProjectLoadCompletedSignature
	 * @param InOnFailed Pointer to FProjectLoadFailedSignature
	 */
	void Initialize(
		void* InOnLoadStarted,
		void* InOnProgress,
		void* InOnPhaseChanged,
		void* InOnCompleted,
		void* InOnFailed);

	/** Broadcast load started event (thread-safe) */
	void BroadcastLoadStarted(const FLoadRequest& Request);

	/** Broadcast progress update (thread-safe) */
	void BroadcastProgress(float NormalizedProgress);

	/** Broadcast phase state change (thread-safe) */
	void BroadcastPhaseChanged(const FLoadPhaseState& PhaseState, float OverallProgress);

	/** Broadcast successful completion (thread-safe) */
	void BroadcastCompleted(const FLoadRequest& Request, const FProjectLoadTelemetry& Telemetry);

	/** Broadcast failure (thread-safe) */
	void BroadcastFailed(const FLoadRequest& Request, const FText& ErrorMessage, int32 ErrorCode);

	/** Reset state for new load (clears progress throttling) */
	void Reset();

private:
	/** Ensure execution on game thread */
	template<typename FuncType>
	void EnsureGameThread(FuncType&& Func);

	// Delegate pointers (not owned, stored as void* to avoid circular include)
	void* OnLoadStarted = nullptr;
	void* OnProgress = nullptr;
	void* OnPhaseChanged = nullptr;
	void* OnCompleted = nullptr;
	void* OnFailed = nullptr;

	// Progress throttling (reset per load)
	float LastLoggedProgress = -1.0f;
};

template<typename FuncType>
void FLoadEventBroadcaster::EnsureGameThread(FuncType&& Func)
{
	if (IsInGameThread())
	{
		Func();
	}
	else
	{
		AsyncTask(ENamedThreads::GameThread, MoveTemp(Func));
	}
}
