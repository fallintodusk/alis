// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "CoreMinimal.h"
#include "ProjectWorldTerrainSurface.h"
struct FProjectWorldCanonicalBundle;
struct FProjectWorldCanonicalCell;
struct FMeshDescription;

namespace ProjectWorldConstructedMesh
{
	struct FSlopeMetrics
	{
		double MaximumGrade=0,MaximumCrossSlope=0;
		int32 FaceCount=0,FirstFailingFace=INDEX_NONE;
	};
	PROJECTWORLDEDITOR_API bool ValidateSlopes(const TArray<FProjectWorldSurfaceTriangle>& Faces,
		const FVector2D& AlongAxis,double MaximumGrade,double MaximumCrossSlope,FSlopeMetrics& Metrics,FString& Error);
	// Native stored local centimetres. Tests relationships, not source-footprint coverage.
	PROJECTWORLDEDITOR_API bool ValidateStored(const TArray<FProjectWorldSurfaceTriangle>& Body,
		double MaximumJoinStepCentimeters, FString& Error);
	// Complete physical source surface at canonical height; native storage cannot add presentation lift.
	// Optional source ordinals follow final Out.Triangles().GetElementIDs() iteration order; failure clears all outputs.
	// Optional displacement bounds all source-footprint/clipped-node XY conversions in native local cm.
	// It is NaN on failure and measures storage conversion, not geographic accuracy or frame-transform error.
	PROJECTWORLDEDITOR_API bool Build(const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldCanonicalCell& Cell, const TArray<FProjectWorldSurfaceTriangle>& Field,
		double MaximumJoinStepMeters,
		FMeshDescription& Out, FVector& Origin, FString& Error,TArray<int32>* OutSourceTriangleIndices=nullptr,
		double* OutMaximumXYDisplacementCentimeters=nullptr);
}
