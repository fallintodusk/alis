// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Types/ProjectLoadPhaseState.h"

// Forward declarations
struct FProjectLoadTelemetry;

/**
 * FLoadProgressTracker
 *
 * Single Responsibility: Track phase states and calculate overall progress.
 *
 * Features:
 * - Phase state management
 * - Overall progress calculation (0.0 to 1.0)
 * - Phase timing metrics
 * - Telemetry data collection
 *
 * Progress Calculation:
 * - Each phase contributes equally (1/N where N = number of phases)
 * - Completed/Skipped phases contribute full 1.0
 * - InProgress phases contribute their current progress
 * - Pending/Failed phases contribute 0.0
 *
 * Thread Safety:
 * - NOT thread-safe by design (caller must synchronize)
 * - Progress updates should be dispatched to game thread before calling
 */
class FLoadProgressTracker
{
public:
	/**
	 * Initialize/reset tracker for a new load operation.
	 *
	 * @param InPhaseCount Number of phases in the pipeline
	 */
	void Reset(int32 InPhaseCount);

	/**
	 * Reset with specific phases.
	 *
	 * @param Phases Array of phases to track
	 */
	void Reset(const TArray<ELoadPhase>& Phases);

	/**
	 * Update state for a specific phase.
	 *
	 * @param Phase The phase to update
	 * @param State New state for the phase
	 * @param Progress Progress within the phase (0.0 to 1.0)
	 * @param StatusMessage Human-readable status
	 */
	void UpdatePhaseState(ELoadPhase Phase, EPhaseState State, float Progress = 0.0f, const FText& StatusMessage = FText::GetEmpty());

	/**
	 * Get state for a specific phase.
	 *
	 * @param Phase The phase to query
	 * @return Pointer to phase state, or nullptr if not found
	 */
	const FLoadPhaseState* GetPhaseState(ELoadPhase Phase) const;

	/**
	 * Get mutable state for a specific phase.
	 *
	 * @param Phase The phase to query
	 * @return Pointer to phase state, or nullptr if not found
	 */
	FLoadPhaseState* GetPhaseStateMutable(ELoadPhase Phase);

	/**
	 * Calculate overall progress across all phases.
	 *
	 * @return Normalized progress (0.0 to 1.0)
	 */
	float CalculateOverallProgress() const;

	/**
	 * Get all phase states.
	 *
	 * @return Reference to phase states array
	 */
	const TArray<FLoadPhaseState>& GetAllPhaseStates() const { return PhaseStates; }

	/**
	 * Start timing for a phase.
	 *
	 * @param Phase The phase starting execution
	 */
	void StartPhaseTiming(ELoadPhase Phase);

	/**
	 * Stop timing for a phase.
	 *
	 * @param Phase The phase ending execution
	 */
	void StopPhaseTiming(ELoadPhase Phase);

	/**
	 * Get duration for a specific phase.
	 *
	 * @param Phase The phase to query
	 * @return Duration in seconds, or 0 if not available
	 */
	double GetPhaseDuration(ELoadPhase Phase) const;

	/**
	 * Get total elapsed time since tracking started.
	 *
	 * @return Elapsed time in seconds
	 */
	double GetTotalElapsedTime() const;

	/**
	 * Fill telemetry structure with collected data.
	 *
	 * @param OutTelemetry Telemetry structure to populate
	 */
	void FillTelemetry(FProjectLoadTelemetry& OutTelemetry) const;

	/**
	 * Get number of phases being tracked.
	 */
	int32 GetPhaseCount() const { return PhaseStates.Num(); }

	/**
	 * Check if all phases are complete (success or skip).
	 */
	bool AreAllPhasesComplete() const;

	/**
	 * Check if any phase has failed.
	 */
	bool HasAnyPhaseFailed() const;

	/**
	 * Get the current active phase (first InProgress phase).
	 *
	 * @return Current phase, or ELoadPhase::None if none active
	 */
	ELoadPhase GetCurrentActivePhase() const;

	/**
	 * Generate debug string with all phase states.
	 */
	FString ToDebugString() const;

private:
	/** Phase state tracking */
	TArray<FLoadPhaseState> PhaseStates;

	/** Time when tracking started */
	double TrackingStartTime = 0.0;

	/** Cached overall progress */
	mutable float CachedOverallProgress = 0.0f;

	/** Flag indicating cache needs recalculation */
	mutable bool bProgressCacheDirty = true;
};
