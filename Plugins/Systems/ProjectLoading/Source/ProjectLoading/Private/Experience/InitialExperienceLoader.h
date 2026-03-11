// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Types/ProjectLoadRequest.h"

// Forward declarations
class UProjectExperienceRegistry;
class UProjectExperienceDescriptorBase;
class UGameInstance;

/**
 * FInitialExperienceLoader
 *
 * Single Responsibility: Load initial/entry experience from configuration.
 *
 * Features:
 * - Resolve experience name from game config
 * - Fetch descriptor from registry
 * - Build FLoadRequest from descriptor
 * - Detect redundant loads (already on target map)
 * - Detailed logging for observability
 *
 * Configuration:
 * - Reads from [/Script/Alis.AlisGI] EntryPointExperience
 * - Defaults to "MainMenuWorld"
 */
class FInitialExperienceLoader
{
public:
	/**
	 * Construct loader with registry reference.
	 *
	 * @param InRegistry Experience registry for descriptor lookup
	 */
	explicit FInitialExperienceLoader(UProjectExperienceRegistry* InRegistry);

	/**
	 * Resolve initial experience name from configuration.
	 *
	 * @return Experience name (e.g., "MainMenuWorld")
	 */
	FName ResolveInitialExperienceName() const;

	/**
	 * Build a load request from an experience name.
	 *
	 * @param ExperienceName Name of the experience to load
	 * @param OutRequest Request structure to populate
	 * @param OutError Error message if build fails
	 * @return True if request was built successfully
	 */
	bool BuildLoadRequest(FName ExperienceName, FLoadRequest& OutRequest, FText& OutError);

	/**
	 * Check if we're already on the target map.
	 * Prevents redundant load when engine already loaded default map.
	 *
	 * @param Request The load request to check
	 * @param GameInstance Game instance for world access
	 * @return True if already on target map
	 */
	bool IsAlreadyOnTargetMap(const FLoadRequest& Request, UGameInstance* GameInstance) const;

	/**
	 * Ensure asset scans are registered for the descriptor.
	 * Required for runtime discovery of experience assets.
	 *
	 * @param Descriptor The experience descriptor
	 */
	void EnsureAssetScans(const UProjectExperienceDescriptorBase& Descriptor);

	/**
	 * Get detailed status string for observability.
	 */
	FString GetStatusString() const;

private:
	/**
	 * Log detailed load request info for observability.
	 */
	void LogLoadRequestDetails(const FLoadRequest& Request) const;

	/**
	 * Dynamically discover map dependencies using AssetRegistry.
	 * Finds all StaticMesh, Texture, Material, SkeletalMesh assets referenced by the map.
	 *
	 * @param MapSoftPath Path to the map asset
	 * @param OutDependencies Discovered dependencies to preload
	 * @param MaxDepth Maximum recursion depth (default 3)
	 */
	void DiscoverMapDependencies(const FSoftObjectPath& MapSoftPath, TArray<FSoftObjectPath>& OutDependencies, int32 MaxDepth = 3) const;

	/** Registry for descriptor lookup (not owned) */
	UProjectExperienceRegistry* Registry = nullptr;

	/** Last resolved experience name (for observability) */
	mutable FName LastResolvedExperience;

	/** Last error message */
	mutable FString LastError;
};
