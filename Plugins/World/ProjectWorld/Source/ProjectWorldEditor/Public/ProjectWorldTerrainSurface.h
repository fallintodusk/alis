// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once

#include "CoreMinimal.h"

struct FProjectWorldCanonicalCell;

struct FProjectWorldSurfaceTriangle
{
	FVector Points[3];
};

// Plain canonical surface geometry; independent of any terrain renderer.
namespace ProjectWorldTerrainSurface
{
	PROJECTWORLDEDITOR_API bool Triangles(const FProjectWorldCanonicalCell& Cell,
		const FBox2D& Bounds, TArray<FProjectWorldSurfaceTriangle>& Out, FString& Error);
	PROJECTWORLDEDITOR_API bool IntersectXY(const FProjectWorldSurfaceTriangle& A,
		const FProjectWorldSurfaceTriangle& B, TArray<FVector2D>& Out, FString& Error);
	PROJECTWORLDEDITOR_API bool Height(const FProjectWorldSurfaceTriangle& Triangle,
		const FVector2D& Point, double& Out, FString& Error);
	PROJECTWORLDEDITOR_API double Area(const TArray<FVector2D>& Polygon);
}
