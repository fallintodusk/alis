// Copyright ALIS. All Rights Reserved.

#include "AssetSyncSettings.h"

UAssetSyncSettings::UAssetSyncSettings()
	: bAutoFixRedirectorsOnRename(true) // Default ON - safe for World Partition + binary merge workflow
{
}

FName UAssetSyncSettings::GetCategoryName() const
{
	return FName(TEXT("Plugins"));
}
