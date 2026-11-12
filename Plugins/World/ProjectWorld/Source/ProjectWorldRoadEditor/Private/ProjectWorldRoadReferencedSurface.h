// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "ProjectWorldRoadSurface.h"
struct FProjectWorldCanonicalBundle;
struct FProjectWorldCanonicalFeature;
namespace ProjectWorldRoadReferencedSurface
{
	bool Read(const FProjectWorldCanonicalBundle& Bundle, const FProjectWorldCanonicalRoadElevation& Profile,
		TArray<FProjectWorldSurfaceTriangle>& Field, FString& Identity, FString& Error);
	bool Append(const FProjectWorldCanonicalBundle& Bundle, const FProjectWorldCanonicalFeature& Road,
		const FProjectWorldSurfaceTriangle (&Domain)[2], TArray<ProjectWorldRoadSurface::FRibbonTriangle>& Out, FString& Error);
}
