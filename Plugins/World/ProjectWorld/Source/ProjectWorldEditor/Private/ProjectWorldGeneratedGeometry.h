// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

struct FProjectWorldCanonicalBundle;
struct FProjectWorldCanonicalCell;
struct FProjectWorldRealizationResult;
class UWorld;
class UMaterialInterface;

struct FProjectWorldGeometryPresentation
{
	UMaterialInterface* TerrainMaterial = nullptr;
	UMaterialInterface* RoadMaterial = nullptr;
	UMaterialInterface* BuildingMaterial = nullptr;
	FLinearColor TerrainColor = FLinearColor(0.18f, 0.32f, 0.12f);
	FLinearColor RoadColor = FLinearColor(0.08f, 0.08f, 0.08f);
	FLinearColor BuildingColor = FLinearColor(0.42f, 0.38f, 0.32f);
};

namespace ProjectWorldGeneratedGeometry
{
	extern const FName GeneratedTag;
	FGuid StableGuid(const FString& Value);

	bool RemoveOwnedActors(
		UWorld* World,
		bool bPreserveLandscape,
		FProjectWorldRealizationResult& OutResult);

	bool RemoveStaleOwnedActorsForApply(
		UWorld* World,
		const FProjectWorldCanonicalBundle& Bundle,
		const FString& RuntimeProfileId,
		bool bPreserveLandscape,
		FProjectWorldRealizationResult& OutResult);

	bool CreateOwnedActors(
		UWorld* World,
		const FProjectWorldCanonicalBundle& Bundle,
		bool bIncludeTerrain,
		int32 MaxRoadFeatures,
		int32 MaxBuildingFeatures,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError,
		const FProjectWorldGeometryPresentation* Presentation = nullptr,
		const FString& CollisionRoadFeatureId = FString());

	double SampleTerrain(
		const FProjectWorldCanonicalCell& Cell,
		double X,
		double Y);

	double MeasureCoordinateRoundTrip(
		UWorld* World,
		const FProjectWorldCanonicalBundle& Bundle,
		bool bPersistActor,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError);
}
