// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

struct FProjectWorldCanonicalBundle;
struct FProjectWorldCanonicalCell;
struct FProjectWorldCanonicalFeature;

struct FProjectWorldTerrainWaterTriangle
{
	FVector2D A;
	FVector2D B;
	FVector2D C;
	FVector4d Bounds;
};

struct FProjectWorldTerrainWaterPolygon
{
	int32 PolygonIndex = INDEX_NONE;
	FVector4d Bounds;
};

struct FProjectWorldTerrainWaterSurface
{
	const FProjectWorldCanonicalFeature* Feature = nullptr;
	TArray<FProjectWorldTerrainWaterPolygon> Polygons;
	TArray<FProjectWorldTerrainWaterTriangle> Triangles;
};

struct FProjectWorldTerrainWaterConformanceContext
{
	TMap<FString, FProjectWorldTerrainWaterSurface> Surfaces;
};

struct FProjectWorldTerrainWaterConformanceStats
{
	int32 WaterFootprintSampleCount = 0;
	int32 ConditionedSampleCount = 0;
	double MaximumCorrectionMeters = 0.0;
};

namespace ProjectWorldTerrainWaterConformance
{
	bool Prepare(
		const FProjectWorldCanonicalBundle& Bundle,
		FProjectWorldTerrainWaterConformanceContext& OutContext,
		FString& OutError);

	bool BuildCellHeights(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldTerrainWaterConformanceContext& Context,
		const FProjectWorldCanonicalCell& Cell,
		TArray<double>& OutHeightsMeters,
		FProjectWorldTerrainWaterConformanceStats& OutStats,
		FString& OutError);

	bool BuildCellHeights(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldCanonicalCell& Cell,
		TArray<double>& OutHeightsMeters,
		FProjectWorldTerrainWaterConformanceStats& OutStats,
		FString& OutError);
}
