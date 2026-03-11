// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../ProjectLoadPhaseExecutor.h"

/**
 * Phase 4: Activate Features
 * Activates feature plugins required by the load request.
 * Integrates with ProjectFeatureActivationSubsystem.
 */
class FActivateFeaturesPhaseExecutor : public FProjectLoadPhaseExecutor
{
public:
	virtual FProjectPhaseResult Execute(FProjectPhaseContext& Context) override;
	virtual ELoadPhase GetPhase() const override { return ELoadPhase::ActivateFeatures; }
	virtual FText GetPhaseName() const override;
	virtual bool ShouldSkip(const FLoadRequest& Request) const override;
};
