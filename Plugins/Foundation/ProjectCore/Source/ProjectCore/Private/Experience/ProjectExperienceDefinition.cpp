// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Experience/ProjectExperienceDefinition.h"

FPrimaryAssetId UProjectExperienceDefinition::GetPrimaryAssetId() const
{
	if (ExperienceName.IsNone())
	{
		return FPrimaryAssetId();
	}

	return FPrimaryAssetId(FPrimaryAssetType(TEXT("ExperienceDefinition")), ExperienceName);
}
