// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once

#include "CoreMinimal.h"
#include "ProjectWorldCivilGrade.h"

struct FProjectWorldCanonicalCell;

struct FProjectWorldSurfaceTriangle
{
	FVector Points[3];
};

// Plain canonical surface geometry; independent of any terrain renderer.
namespace ProjectWorldTerrainSurface
{
    constexpr int32 MaximumFieldTriangles=65536;
    enum class EFieldSampleResult {Sampled,Outside,Invalid};
    PROJECTWORLDEDITOR_API bool ValidateField(const TArray<FProjectWorldSurfaceTriangle>& Triangles,
        int32 MaximumTriangles,FString& Error);
    PROJECTWORLDEDITOR_API EFieldSampleResult TrySampleField(const TArray<FProjectWorldSurfaceTriangle>& Triangles,
        const FVector2D& Point,int32 MaximumTriangles,double& Height,FString& Error);
	PROJECTWORLDEDITOR_API bool Triangles(const FProjectWorldCanonicalCell& Cell,
		const FBox2D& Bounds, TArray<FProjectWorldSurfaceTriangle>& Out, FString& Error,
		EProjectWorldTerrainQuery Query = EProjectWorldTerrainQuery::FinalSurface);
	PROJECTWORLDEDITOR_API bool IntersectXY(const FProjectWorldSurfaceTriangle& A,
		const FProjectWorldSurfaceTriangle& B, TArray<FVector2D>& Out, FString& Error);
	PROJECTWORLDEDITOR_API bool Height(const FProjectWorldSurfaceTriangle& Triangle,
		const FVector2D& Point, double& Out, FString& Error);
	PROJECTWORLDEDITOR_API double Area(const TArray<FVector2D>& Polygon);
}
