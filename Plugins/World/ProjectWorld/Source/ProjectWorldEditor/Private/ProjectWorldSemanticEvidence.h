// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

struct FProjectWorldRealizationResult;
class UWorld;

namespace ProjectWorldSemanticEvidence
{
	bool Capture(
		UWorld* World,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError);
}
