// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "MeshDescription.h"

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
};

namespace ProjectWorldWaterMeshBuilder
{
	double EvaluateSurfaceZ(
		const FProjectWorldCanonicalFeature& Feature,
		const FVector2D& CanonicalPoint,
		double HeightQuantizationMeters);

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
		FString& OutError);

	bool BuildCellSurface(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldCanonicalCell& Cell,
		const FProjectWorldCanonicalFeature& Feature,
		double SurfaceOffsetMeters,
		FProjectWorldWaterMeshBuildResult& OutResult,
		FString& OutError);
}
