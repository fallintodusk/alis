// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../ProjectLoadPhaseExecutor.h"

/**
 * Phase 2: Mount Content
 * Mounts IOStore containers or PAK files for additional content.
 * Integrates with ProjectContentPackSubsystem for download and mounting.
 */
class FMountContentPhaseExecutor : public FProjectLoadPhaseExecutor
{
public:
	virtual FProjectPhaseResult Execute(FProjectPhaseContext& Context) override;
	virtual ELoadPhase GetPhase() const override { return ELoadPhase::MountContent; }
	virtual FText GetPhaseName() const override;
	virtual bool ShouldSkip(const FLoadRequest& Request) const override;
};
