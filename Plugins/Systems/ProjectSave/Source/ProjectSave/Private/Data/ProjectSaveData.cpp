// Copyright ALIS. All Rights Reserved.

#include "Data/ProjectSaveData.h"

FProjectWorldProgress* UProjectSaveGame::FindWorldProgress(FName WorldId)
{
	for (FProjectWorldProgress& Progress : WorldProgressData)
	{
		if (Progress.WorldId == WorldId)
		{
			return &Progress;
		}
	}
	return nullptr;
}

FProjectWorldProgress& UProjectSaveGame::FindOrCreateWorldProgress(FName WorldId)
{
	FProjectWorldProgress* Existing = FindWorldProgress(WorldId);
	if (Existing)
	{
		return *Existing;
	}

	// Create new entry
	FProjectWorldProgress& NewProgress = WorldProgressData.AddDefaulted_GetRef();
	NewProgress.WorldId = WorldId;
	return NewProgress;
}
