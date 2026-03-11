// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ProjectLoadingSettings.generated.h"

/**
 * Configuration settings for ProjectLoading subsystem.
 * Allows phase-level control for integration with immutable bootloader.
 */
UCLASS(Config=ProjectLoading, DefaultConfig, meta=(DisplayName="Project Loading"))
class PROJECTLOADING_API UProjectLoadingSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UProjectLoadingSettings();

	// UDeveloperSettings interface
	virtual FName GetCategoryName() const override { return TEXT("Project"); }

	/**
	 * When true, Phase 2 (MountContent) only mounts ContentPacks/DLC.
	 * When false, Phase 2 also mounts feature plugins (legacy behavior).
	 *
	 * NOTE: Currently Phase 2 only mounts ContentPacks regardless of this setting.
	 * This flag is reserved for future use if feature plugin mounting is added to Phase 2.
	 */
	UPROPERTY(Config, EditAnywhere, Category="Phase Configuration")
	bool bMountContentPacksOnly = true;

	/**
	 * When true, Phase 4 (ActivateFeatures) skips feature activation and delegates to Orchestrator.
	 * When false, Phase 4 computes and activates features locally (legacy behavior).
	 *
	 * This enables the immutable bootloader path:
	 *   BootROM -> Orchestrator -> [Orchestrator activates features] -> ProjectLoading (phases 1,2,3,5,6)
	 *
	 * When false (legacy path):
	 *   ProjectLoading -> [Phase 4 activates features based on Request.FeaturesToActivate]
	 */
	UPROPERTY(Config, EditAnywhere, Category="Phase Configuration")
	bool bDelegateFeatureActivationToOrchestrator = false;
};
