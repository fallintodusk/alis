// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldRuntimePartitionRealization.h"

#include "ProjectWorldRealizationService.h"
#include "ProjectWorldRuntimePartitionPolicy.h"
#include "ProjectWorldRuntimeProfile.h"

namespace ProjectWorldRuntimePartitionRealization
{
	bool ApplyAndCapture(
		UWorld* World,
		const FProjectWorldRuntimeProfile& Profile,
		FProjectWorldRealizationResult& OutResult,
		bool& bOutChanged,
		FString& OutError)
	{
		FProjectWorldRuntimePartitionSettings Expected;
		Expected.PartitionCount = 1;
		Expected.CellSizeCentimeters = Profile.RuntimeCellSizeMeters * 100;
		Expected.LoadingRangeCentimeters = Profile.RuntimeLoadingRangeMeters * 100;
		Expected.bIs2D = true;
		Expected.bBlockOnSlowStreaming = Profile.bBlockOnSlowStreaming;
		if (!ProjectWorldRuntimePartitionPolicy::Apply(
			World, Expected, bOutChanged, OutError))
		{
			return false;
		}

		FProjectWorldRuntimePartitionSettings Actual;
		if (!ProjectWorldRuntimePartitionPolicy::Read(World, Actual, OutError))
		{
			return false;
		}
		OutResult.RuntimePartitionCount = Actual.PartitionCount;
		OutResult.RuntimeCellSizeMeters = Actual.CellSizeCentimeters / 100;
		OutResult.RuntimeLoadingRangeMeters = Actual.LoadingRangeCentimeters / 100;
		OutResult.bRuntimePartitionIs2D = Actual.bIs2D;
		OutResult.bRuntimeBlockOnSlowStreaming = Actual.bBlockOnSlowStreaming;
		return true;
	}
}
