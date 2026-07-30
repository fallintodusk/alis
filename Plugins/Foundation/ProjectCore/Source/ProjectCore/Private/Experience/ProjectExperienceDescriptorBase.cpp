// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Experience/ProjectExperienceDescriptorBase.h"

#include "Types/ProjectLoadRequest.h"

void UProjectExperienceDescriptorBase::BuildLoadRequest(FLoadRequest& OutRequest) const
{
	OutRequest.ExperienceName = ExperienceName;

	if (!LoadAssets.Map.ToSoftObjectPath().IsNull())
	{
		OutRequest.MapSoftPath = LoadAssets.Map.ToSoftObjectPath();
	}
}
