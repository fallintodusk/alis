// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

struct FProjectWorldRealizationResult;
struct FProjectWorldRuntimeProfile;
class UWorld;

namespace ProjectWorldTerritoryRuntimeAcceptance
{
	bool CaptureAndCheck(
		UWorld* World,
		const FProjectWorldRuntimeProfile& Profile,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError);
}
