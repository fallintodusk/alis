// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../ProjectLoadPhaseExecutor.h"

/**
 * Phase 6: Warmup
 * Optional warmup phase for shader compilation and level streaming.
 * Non-critical - failure allows pipeline to continue with degraded performance.
 *
 * IMPORTANT: Do NOT override ShouldSkip() here!
 * This phase MUST always run (even if warmup is disabled) because it releases
 * preload handles from Phase 3. If ShouldSkip() returns true, Execute() is never
 * called and handles leak forever.
 */
class FWarmupPhaseExecutor : public FProjectLoadPhaseExecutor
{
public:
	virtual FProjectPhaseResult Execute(FProjectPhaseContext& Context) override;
	virtual ELoadPhase GetPhase() const override { return ELoadPhase::Warmup; }
	virtual FText GetPhaseName() const override;

	// NOTE: ShouldSkip() intentionally NOT overridden - we must always run to release preload handles
	// The "skip warmup" logic is handled inside Execute() after handle cleanup

	// Warmup is non-critical - failure allows pipeline to continue with degraded performance
	virtual bool IsCritical() const override { return false; }

	// World and streaming level APIs require game thread
	virtual bool RequiresGameThread() const override { return true; }
};
