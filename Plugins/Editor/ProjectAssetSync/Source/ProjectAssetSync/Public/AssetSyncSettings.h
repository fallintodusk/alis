// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AssetSyncSettings.generated.h"

/**
 * Project settings for the Asset Sync plugin.
 *
 * Accessible via: Editor → Project Settings → Plugins → Asset Sync
 */
UCLASS(Config=Editor, DefaultConfig, meta=(DisplayName="Asset Sync"))
class PROJECTASSETSYNC_API UAssetSyncSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UAssetSyncSettings();

	/**
	 * Automatically fix redirectors after asset renames.
	 *
	 * When enabled, the plugin automatically:
	 * - Finds redirectors created by asset renames
	 * - Updates all references to point directly to the new asset
	 * - Deletes the redirector to keep content browser clean
	 *
	 * Safe for workflows with:
	 * - World Partition (isolated actors)
	 * - Binary merge strategy ("take incoming")
	 * - Frequent syncs (no long-lived feature branches)
	 *
	 * Disable if your team uses long-lived feature branches where redirectors
	 * protect references across divergent branches.
	 */
	UPROPERTY(Config, EditAnywhere, Category="Redirector Auto-Fix",
		meta=(DisplayName="Auto-Fix Redirectors On Rename"))
	bool bAutoFixRedirectorsOnRename;

	//~ Begin UDeveloperSettings Interface
	virtual FName GetCategoryName() const override;
	//~ End UDeveloperSettings Interface
};
