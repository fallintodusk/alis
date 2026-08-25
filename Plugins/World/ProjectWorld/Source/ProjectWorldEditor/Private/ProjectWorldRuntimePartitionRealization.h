// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

struct FProjectWorldRealizationResult;
struct FProjectWorldRuntimeProfile;
class UWorld;

namespace ProjectWorldRuntimePartitionRealization
{
	bool ApplyAndCapture(
		UWorld* World,
		const FProjectWorldRuntimeProfile& Profile,
		FProjectWorldRealizationResult& OutResult,
		bool& bOutChanged,
		FString& OutError);
}
