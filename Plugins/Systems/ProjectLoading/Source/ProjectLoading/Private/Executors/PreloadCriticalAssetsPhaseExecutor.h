// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../ProjectLoadPhaseExecutor.h"

struct FStreamableHandle;

/**
 * Phase 3: Preload Critical Assets
 * Streams in essential assets before map travel.
 * Uses FStreamableManager for async asset loading.
 */
class FPreloadCriticalAssetsPhaseExecutor : public FProjectLoadPhaseExecutor
{
public:
	virtual FProjectPhaseResult Execute(FProjectPhaseContext& Context) override;
	virtual ELoadPhase GetPhase() const override { return ELoadPhase::PreloadCriticalAssets; }
	virtual FText GetPhaseName() const override;

	// AssetManager and StreamableManager APIs require game thread
	virtual bool RequiresGameThread() const override { return true; }
};
