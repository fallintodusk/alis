// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "CoreMinimal.h"
#include "ProjectWorldTerrainSurface.h"
struct FProjectWorldCanonicalBundle;
struct FProjectWorldCanonicalFeature;

// Typed geometry consumption only; source qualification and footprint admission stay upstream.
namespace ProjectWorldConstructedSurface
{
	enum class ESampleResult { Sampled, Outside, Invalid };
	PROJECTWORLDEDITOR_API bool Read(const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldCanonicalFeature& Feature, TArray<FProjectWorldSurfaceTriangle>& Triangles,
		FString& Identity, FString& Error);
	PROJECTWORLDEDITOR_API bool ValidateField(const TArray<FProjectWorldSurfaceTriangle>& Triangles, FString& Error);
	// Sample a validated complete field; outside or contradictory membership remains unknown.
	PROJECTWORLDEDITOR_API bool Sample(const TArray<FProjectWorldSurfaceTriangle>& Triangles,
		const FVector2D& Point,double& Height,FString& Error);
	PROJECTWORLDEDITOR_API ESampleResult TrySample(const TArray<FProjectWorldSurfaceTriangle>& Triangles,
		const FVector2D& Point,double& Height,FString& Error);
}
