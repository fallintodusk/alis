// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

struct FProjectWorldCanonicalBundle;
struct FProjectWorldCanonicalCell;
struct FProjectWorldAuthoredOverlaySet;
struct FProjectWorldLayerDirtyInput;
struct FProjectWorldRealizationProfile;
struct FProjectWorldRealizationResult;
class UWorld;

namespace ProjectWorldLayerInventory
{
	bool HashTerrainWaterCellInput(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldCanonicalCell& Cell,
		FString& OutHash);

	bool Build(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldRealizationProfile& Profile,
		const FProjectWorldAuthoredOverlaySet& AuthoredOverlaySet,
		bool bFirstApply,
		const FProjectWorldLayerDirtyInput* DirtyInput,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError);

	bool CaptureArtifacts(
		UWorld* World,
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldRealizationProfile& Profile,
		const FProjectWorldAuthoredOverlaySet& AuthoredOverlaySet,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError);
}
