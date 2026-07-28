// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../ProjectLoadPhaseExecutor.h"

/**
 * Phase 2: Mount Content
 * Reserved owner for runtime mounting of already installed content.
 * Explicit requests fail closed until the mounting implementation exists.
 */
class FMountContentPhaseExecutor : public FProjectLoadPhaseExecutor
{
public:
	virtual FProjectPhaseResult Execute(FProjectPhaseContext& Context) override;
	virtual ELoadPhase GetPhase() const override { return ELoadPhase::MountContent; }
	virtual FText GetPhaseName() const override;
	virtual bool ShouldSkip(const FLoadRequest& Request) const override;
	virtual bool SupportsRetry() const override { return false; }
};
