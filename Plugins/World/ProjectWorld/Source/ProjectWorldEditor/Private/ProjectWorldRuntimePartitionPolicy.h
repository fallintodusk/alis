// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

class UWorld;

struct FProjectWorldRuntimePartitionSettings
{
	int32 PartitionCount = 0;
	int32 CellSizeCentimeters = 0;
	int32 LoadingRangeCentimeters = 0;
	bool bIs2D = false;
	bool bBlockOnSlowStreaming = false;
};

namespace ProjectWorldRuntimePartitionPolicy
{
	bool Apply(
		UWorld* World,
		const FProjectWorldRuntimePartitionSettings& Settings,
		bool& bOutChanged,
		FString& OutError);
	bool Read(
		UWorld* World,
		FProjectWorldRuntimePartitionSettings& OutSettings,
		FString& OutError);
}
