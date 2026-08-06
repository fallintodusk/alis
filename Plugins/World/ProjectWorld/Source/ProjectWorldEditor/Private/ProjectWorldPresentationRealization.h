// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

struct FProjectWorldCanonicalBundle;
struct FProjectWorldPresentationProfile;
struct FProjectWorldPresentationResources;
struct FProjectWorldRealizationResult;
class UWorld;

namespace ProjectWorldPresentationRealization
{
	bool Apply(
		UWorld* World,
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldPresentationProfile& Profile,
		const FProjectWorldPresentationResources& Resources,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError);
}
