// Copyright ALIS. All Rights Reserved.

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
