// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "MeshDescription.h"
#include "Curve/GeneralPolygon2.h"

struct FProjectWorldCanonicalBundle;
struct FProjectWorldCanonicalCell;
struct FProjectWorldCanonicalFeature;

struct FProjectWorldWaterMeshBuildResult
{
	FMeshDescription MeshDescription;
	FVector ActorOrigin = FVector::ZeroVector;
	FString SemanticDigest;
	int32 TriangleCount = 0;
	double CanonicalAreaSquareMeters = 0.0;
};

struct FProjectWorldPreparedWaterSurface
{
	TArray<FVector2D> TriangleVertices;
	TArray<UE::Geometry::FGeneralPolygon2d> Footprints;
};

namespace ProjectWorldWaterMeshBuilder
{
	void AppendTriangle(FMeshDescription& Description, const FVector Positions[3]);
	double PresentationOffset(const FProjectWorldCanonicalFeature& Feature, double LayerOffsetMeters);
	bool ValidateStoredLevel(const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldCanonicalFeature& Feature, const FVector& ActorOrigin, FString& OutError);

	bool PreparePolygons(const TArray<UE::Geometry::FGeneralPolygon2d>& Polygons,
		FProjectWorldPreparedWaterSurface& OutSurface, FString& OutError);
	double EvaluateSurfaceZ(
		const FProjectWorldCanonicalFeature& Feature,
		const FVector2D& CanonicalPoint,
		double HeightQuantizationMeters);

	// A conservative certificate for every point of a convex region, not samples.
	bool ConstantSurfaceZ(
		const FProjectWorldCanonicalFeature& Feature,
		TConstArrayView<FVector2D> CanonicalRegion,
		double HeightQuantizationMeters,
		double& OutHeight);

	bool PrepareSurface(
		const FProjectWorldCanonicalFeature& Feature,
		const FVector4d& TargetBounds,
		FProjectWorldPreparedWaterSurface& OutSurface,
		FString& OutError);

	bool BuildCellSurface(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldCanonicalCell& Cell,
		const FProjectWorldCanonicalFeature& Feature,
		const FProjectWorldPreparedWaterSurface& PreparedSurface,
		double SurfaceOffsetMeters,
		FProjectWorldWaterMeshBuildResult& OutResult,
		FString& OutError,
		const FVector* CommonOrigin = nullptr);

	bool BuildCellSurface(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldCanonicalCell& Cell,
		const FProjectWorldCanonicalFeature& Feature,
		double SurfaceOffsetMeters,
		FProjectWorldWaterMeshBuildResult& OutResult,
		FString& OutError);
}
