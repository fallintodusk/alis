// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "CoreMinimal.h"
#include "ProjectWorldStoredSurface.h"
struct FProjectWorldCanonicalBundle;
struct FProjectWorldCanonicalCell;
struct FProjectWorldRealizationLayer;

struct FProjectWorldRoadEmission
{
	FProjectWorldStoredSurface Surface;
	FString GeometryHash;
	TSet<FString> LocallyEligibleFeatureIds;
	double PresentationLiftMeters = 0;
};

namespace ProjectWorldRoadEmission
{
	// Read the exact prospective stored geometry without creating assets or touching a world.
	// A global selection may have no local contribution. Compare LocallyEligibleFeatureIds
	// with the declared local closure; class/width filters and cell membership still apply.
	PROJECTWORLDROADEDITOR_API bool Read(const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldCanonicalCell& Cell, const FProjectWorldRealizationLayer& Layer,
		const TSet<FString>& FeatureIds, FProjectWorldRoadEmission& Out, FString& Error);
}
