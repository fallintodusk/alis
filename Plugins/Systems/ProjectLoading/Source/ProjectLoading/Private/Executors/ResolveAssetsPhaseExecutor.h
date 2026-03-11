// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../ProjectLoadPhaseExecutor.h"

/**
 * Phase 1: Resolve Assets
 * Translates PrimaryAssetIds from manifests to soft object references.
 * Validates that map and experience assets exist in Asset Manager.
 */
class FResolveAssetsPhaseExecutor : public FProjectLoadPhaseExecutor
{
public:
	virtual FProjectPhaseResult Execute(FProjectPhaseContext& Context) override;
	virtual ELoadPhase GetPhase() const override { return ELoadPhase::ResolveAssets; }
	virtual FText GetPhaseName() const override;

	// AssetManager APIs require game thread
	virtual bool RequiresGameThread() const override { return true; }
};
