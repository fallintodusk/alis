// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

struct FProjectWorldAuthoredOverlaySet;
struct FProjectWorldCanonicalBundle;
struct FProjectWorldCanonicalCell;
struct FProjectWorldCanonicalPolygon;
struct FProjectWorldRealizationLayer;
struct FProjectWorldRealizationProfile;

enum class EProjectWorldVegetationExclusion : uint8
{
	None,
	Road,
	Water,
	AuthoredMask
};

struct FProjectWorldVegetationRoadSegment
{
	FVector2D Start = FVector2D::ZeroVector;
	FVector2D End = FVector2D::ZeroVector;
	double HalfWidthMeters = 0.0;
};

struct FProjectWorldVegetationWaterSegment
{
	FVector2D Start = FVector2D::ZeroVector;
	FVector2D End = FVector2D::ZeroVector;
	double ClearanceMeters = 0.0;
};

struct FProjectWorldVegetationExclusionContext
{
	TArray<FProjectWorldVegetationRoadSegment> RoadSegments;
	TArray<const FProjectWorldCanonicalPolygon*> WaterPolygons;
	TArray<FProjectWorldVegetationWaterSegment> WaterSegments;
	TArray<FVector4d> AuthoredMaskBounds;
	FString InputHash;

	EProjectWorldVegetationExclusion Classify(const FVector2D& Point) const;
};

namespace ProjectWorldVegetationExclusions
{
	bool Build(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldCanonicalCell& Cell,
		const FProjectWorldRealizationProfile& Profile,
		const FProjectWorldRealizationLayer& VegetationLayer,
		const FProjectWorldAuthoredOverlaySet& AuthoredOverlaySet,
		FProjectWorldVegetationExclusionContext& OutContext,
		FString& OutError);
}
