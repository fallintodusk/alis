// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "ProjectWorldRealizationService.h"

namespace ProjectWorldSavePolicy
{
	inline bool RequiresBroadWorldSave(
		const FProjectWorldRealizationResult& Result,
		const bool bExistingMap,
		const bool bPartitionHlodPolicyChanged)
	{
		const int32 ActorMutationCount = Result.CreatedActorCount +
			Result.UpdatedActorCount + Result.RemovedActorCount;
		const bool bGeneratedWorldChanged = bPartitionHlodPolicyChanged ||
			ActorMutationCount > 0 || Result.UpdatedLandscapeComponentCount > 0;
		const bool bOnlySelfSavedActorsChanged = bExistingMap &&
			Result.SelfSavedActorMutationCount > 0 &&
			Result.SelfSavedActorMutationCount == ActorMutationCount &&
			Result.UpdatedLandscapeComponentCount == 0 && !bPartitionHlodPolicyChanged;
		return bGeneratedWorldChanged && !bOnlySelfSavedActorsChanged;
	}
}
