// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

struct FProjectWorldCanonicalBundle;
struct FProjectWorldLayerDirtyInput;
struct FProjectWorldRealizationProfile;
struct FProjectWorldRealizationResult;
class UWorld;

namespace ProjectWorldLayerInventory
{
	bool Build(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldRealizationProfile& Profile,
		bool bFirstApply,
		const FProjectWorldLayerDirtyInput* DirtyInput,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError);

	bool CaptureArtifacts(
		UWorld* World,
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldRealizationProfile& Profile,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError);
}
