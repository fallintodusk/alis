// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../ProjectLoadPhaseExecutor.h"

/**
 * Phase 5: Travel
 * Performs map travel (seamless or hard) to the target world.
 * Travel is atomic and does not support retry.
 */
class FTravelPhaseExecutor : public FProjectLoadPhaseExecutor
{
public:
	virtual FProjectPhaseResult Execute(FProjectPhaseContext& Context) override;
	virtual ELoadPhase GetPhase() const override { return ELoadPhase::Travel; }
	virtual FText GetPhaseName() const override;
	virtual bool SupportsRetry() const override { return false; } // Travel is atomic - don't retry

	// AssetManager and World travel APIs require game thread
	virtual bool RequiresGameThread() const override { return true; }

private:
	/** Resolve map path from request */
	bool ResolveMapPath(const FProjectPhaseContext& Context, FString& OutMapPath, FText& OutError);
};
