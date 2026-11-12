// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "CoreMinimal.h"
#include "ProjectWorldTerrainSurface.h"
struct FProjectWorldCanonicalBundle;
struct FProjectWorldCanonicalCell;
struct FProjectWorldRealizationLayer;

struct FProjectWorldRoadNominalFootprint
{
	// Canonical XY meters at Z=0, before height projection, clipping and native float storage.
	TArray<FProjectWorldSurfaceTriangle> Triangles;
	TSet<FString> LocallyEligibleFeatureIds;
	FString GeometryHash;
};

namespace ProjectWorldRoadNominalFootprint
{
	// Nominal formation shares the renderer's tangent/side/subdivision owner.
	// This does not replace qualification of the final stored emission footprint.
	PROJECTWORLDROADEDITOR_API bool Read(const FProjectWorldCanonicalBundle& Bundle,const FProjectWorldCanonicalCell& Cell,
		const FProjectWorldRealizationLayer& Layer,const TSet<FString>& FeatureIds,FProjectWorldRoadNominalFootprint& Out,FString& Error);
}
