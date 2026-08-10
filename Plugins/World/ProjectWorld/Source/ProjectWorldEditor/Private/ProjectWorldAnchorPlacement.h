// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

struct FProjectWorldAnchorRecord;
struct FProjectWorldAnchorResolution;
struct FProjectWorldAuthoredOverlaySet;
struct FProjectWorldCanonicalBundle;

namespace ProjectWorldAnchorPlacement
{
	bool ResolveHeight(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldAuthoredOverlaySet& Set,
		const FProjectWorldAnchorRecord& Anchor,
		const FVector2D& CanonicalPoint,
		double HorizontalDriftMeters,
		FProjectWorldAnchorResolution& OutResolution,
		double& OutHeightMeters,
		FString& OutError);
}
