// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

class UProjectExperienceRegistry;

/**
 * Discovers configured UProjectExperienceDefinition assets and registers a generic
 * descriptor for each.
 *
 * Lifecycle: this deliberately does NOT run from StartupModule(). Module startup happens
 * before engine init, where UAssetManager::IsInitialized() is always false (see
 * FProjectLoadingModule::StartupModule and docs/asset_manager_registration.md). It runs
 * on first use instead - when a descriptor lookup misses - which is after the AssetManager
 * is live and still before the descriptor's own asset scans are needed.
 */
namespace ProjectExperienceDefinitions
{
	/**
	 * Ensure configured definitions have been discovered and registered exactly once.
	 *
	 * @param Registry Registry receiving the generic descriptors.
	 * @return Number of definitions registered by this call (0 if already done or none found).
	 */
	PROJECTLOADING_API int32 EnsureRegistered(UProjectExperienceRegistry& Registry);

	/** Reset the once-only guard. Test-only seam so cases stay independent. */
	PROJECTLOADING_API void ResetForTests();
}
