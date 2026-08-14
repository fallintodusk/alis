// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

struct FProjectWorldAuthoredOverlaySet;
struct FProjectWorldCanonicalBundle;
struct FProjectWorldRealizationResult;
class UWorld;

namespace ProjectWorldAuthoredOverlayRealization
{
	bool Resolve(
		const FProjectWorldCanonicalBundle& Bundle,
		const FString& ProfilePath,
		FProjectWorldAuthoredOverlaySet& OutSet,
		FProjectWorldRealizationResult& OutResult,
		FString& OutErrorCode,
		FString& OutError);

	bool Apply(
		UWorld* World,
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldAuthoredOverlaySet& Set,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError);
}
