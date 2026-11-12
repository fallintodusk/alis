// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "CoreMinimal.h"
#include "Curve/GeneralPolygon2.h"
#include "ProjectWorldTerrainSurface.h"

namespace ProjectWorldConstructedMeshCoverage
{
	enum class ESourceClip { Empty, Area, Invalid };
	// Native convex clipping output; numerical area disagreement is unknown, not empty.
	ESourceClip ClassifySourceClip(const UE::Geometry::FPolygon2d& Ring,FString& Error);
	struct FMetrics
	{
		double OriginalArea=0,StoredArea=0,MissingArea=0,ExteriorArea=0,ConversionBound=0,ArithmeticBound=0;
	};
	bool Validate(const TArray<UE::Geometry::FGeneralPolygon2d>& Original,
		const TArray<FProjectWorldSurfaceTriangle>& Stored, double RadiusCentimeters,FMetrics& Metrics,FString& Error);
}
