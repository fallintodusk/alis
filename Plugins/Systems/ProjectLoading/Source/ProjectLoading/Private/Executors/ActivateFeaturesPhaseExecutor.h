// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "../ProjectLoadPhaseExecutor.h"

/**
 * Phase 4: Activate Features
 * Validates that features requested by the load are ready through the
 * Orchestrator registry. ProjectLoading does not own module activation.
 */
class FActivateFeaturesPhaseExecutor : public FProjectLoadPhaseExecutor
{
public:
	virtual FProjectPhaseResult Execute(FProjectPhaseContext& Context) override;
	virtual ELoadPhase GetPhase() const override { return ELoadPhase::ActivateFeatures; }
	virtual FText GetPhaseName() const override;
	virtual bool ShouldSkip(const FLoadRequest& Request) const override;
};
