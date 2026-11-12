// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "CoreMinimal.h"
#include "ProjectWorldRoadSurface.h"
struct FProjectWorldCanonicalFeature;
struct FProjectWorldCanonicalBundle;
namespace ProjectWorldRoadElevation
{
	bool Validate(const FProjectWorldCanonicalBundle& Bundle, const FProjectWorldCanonicalFeature& Feature,
		FString& ConsumedIdentity, FString& Error);
	bool Station(const FProjectWorldCanonicalFeature& Feature, const FVector2D& XY, double Tolerance,
		double& Out, FString& Error);
	bool Append(const FProjectWorldCanonicalBundle& Bundle, const FProjectWorldCanonicalFeature& Feature, const FVector2D& A, const FVector2D& B,
		const FVector2D& SideA, const FVector2D& SideB, double Tolerance, double Offset,
		TArray<ProjectWorldRoadSurface::FRibbonTriangle>& Out, FString& Error);
}
