// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Experience/ProjectExperienceDescriptorBase.h"

/**
 * Append-only registry for global (non-experience-specific) asset scan specs.
 *
 * Plugins register their primary asset scan specs here during StartupModule().
 * ProjectLoading applies them via ScanPathsForPrimaryAssets() before
 * experience-specific scans run.
 *
 * Plain C++ singleton (not UObject) because it is populated before UObject
 * system initialization.
 *
 * Scanning registers types with AssetManager so GetPrimaryAssetPath() returns
 * valid paths. It does NOT load assets into memory -- that requires
 * LoadPrimaryAsset() or LoadPrimaryAssets().
 *
 * Thread safety: game-thread and startup-only. Not thread-safe by design.
 * Registration happens on game thread during module startup, consumption
 * happens on game thread during subsystem init.
 *
 * Scope: fixes cooked runtime registration only. Editor uses config merge
 * (EnsureAssetScans returns early when GIsEditor). This is intentional.
 */
class PROJECTCORE_API FGlobalAssetScanRegistry
{
public:
	static FGlobalAssetScanRegistry& Get();

	/**
	 * Register a global scan spec. Called from plugin StartupModule().
	 * Deduplicates by full effective spec (type, directories, base class,
	 * blueprint flag, editor-only flag, sync-scan flag).
	 * Append-only: registered specs are never removed or reordered.
	 */
	void RegisterScanSpec(const FExperienceAssetScanSpec& Spec);

	/** All registered global scan specs (append-only array). */
	const TArray<FExperienceAssetScanSpec>& GetAllSpecs() const;

	/** Current spec count. Consumer tracks applied count to apply only new specs. */
	int32 GetSpecCount() const;

private:
	FGlobalAssetScanRegistry() = default;

	bool IsDuplicate(const FExperienceAssetScanSpec& Spec) const;

	TArray<FExperienceAssetScanSpec> Specs;
};
