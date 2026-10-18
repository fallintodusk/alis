// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "CoreMinimal.h"
#include "ProjectWorldTerrainSurface.h"
struct FMeshDescription;
struct FProjectWorldCanonicalBundle;
struct FProjectWorldCanonicalCell;
struct FProjectWorldCanonicalRoadElevation;
struct FProjectWorldCanonicalCivilGrade;
namespace ProjectWorldRoadSurface
{
	struct FRibbonTriangle
	{
		FProjectWorldSurfaceTriangle Surface;
		bool bCanonicalHeight = false;
		TArray<TPair<FVector2D, FVector2D>> Abutments;
		const FProjectWorldCanonicalRoadElevation* Elevation = nullptr;
		const FProjectWorldCanonicalCivilGrade* Grade = nullptr;
		int32 StorageGroup = INDEX_NONE;
		FProjectWorldSurfaceTriangle StorageDomain;
	};
	struct FContext
	{
		TMap<FIntPoint, const FProjectWorldCanonicalCell*> Cells;
		FBox2D Domain = FBox2D(ForceInit);
	};
	bool Index(const FProjectWorldCanonicalBundle& Bundle, FContext& Context, FString& Error);
	bool Conform(const FProjectWorldCanonicalBundle& Bundle, const FProjectWorldCanonicalCell& Cell,
		const FContext& Context, const TArray<FRibbonTriangle>& Ribbon,
		const FVector& Origin, double Lift, int32 Halo, FMeshDescription* Description,
		TSet<FString>& ConsumedTerrain, FString& Error);
}
