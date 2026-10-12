// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "CoreMinimal.h"
#include "ProjectWorldTerrainSurface.h"
struct FMeshDescription;
struct FProjectWorldCanonicalBundle;
struct FProjectWorldCanonicalCell;
namespace ProjectWorldRoadSurface
{
	struct FContext
	{
		TMap<FIntPoint, const FProjectWorldCanonicalCell*> Cells;
		FBox2D Domain = FBox2D(ForceInit);
	};
	bool Index(const FProjectWorldCanonicalBundle& Bundle, FContext& Context, FString& Error);
	bool Conform(const FProjectWorldCanonicalBundle& Bundle, const FProjectWorldCanonicalCell& Cell,
		const FContext& Context, const TArray<FProjectWorldSurfaceTriangle>& Ribbon,
		const FVector& Origin, double Lift, int32 Halo, FMeshDescription* Description,
		TSet<FString>& ConsumedTerrain, FString& Error);
}
