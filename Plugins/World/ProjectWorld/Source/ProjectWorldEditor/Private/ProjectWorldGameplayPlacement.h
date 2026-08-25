// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

struct FProjectWorldCanonicalBundle;
struct FProjectWorldCanonicalCell;
struct FProjectWorldLayerInventory;
struct FProjectWorldRealizationLayer;
struct FProjectWorldRealizationProfile;
struct FProjectWorldRealizationResult;
class UWorld;

struct FProjectWorldGameplayPlacement
{
	FString ObjectId;
	FPrimaryAssetId DefinitionId;
	int32 CellX = 0;
	int32 CellY = 0;
	double EastingMeters = 0.0;
	double NorthingMeters = 0.0;
	double SurfaceOffsetMeters = 0.0;
	double YawDegrees = 0.0;
};

struct FProjectWorldGameplayPlacementSet
{
	FString PlacementSetId;
	FString WorldDataPluginName;
	FString CanonicalProfileId;
	FString SourcePath;
	TArray<FProjectWorldGameplayPlacement> Placements;
};

namespace ProjectWorldGameplayPlacement
{
	bool Load(
		const FProjectWorldRealizationProfile& Profile,
		const FProjectWorldRealizationLayer& Layer,
		FProjectWorldGameplayPlacementSet& OutSet,
		FString& OutError);

	bool BuildInput(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldRealizationLayer& Layer,
		const FProjectWorldGameplayPlacement& Placement,
		FString& OutCellId,
		FTransform& OutTransform,
		FString& OutHash,
		FString& OutError);

	bool Apply(
		UWorld* World,
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldRealizationProfile& Profile,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError);

	bool Capture(
		UWorld* World,
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldRealizationProfile& Profile,
		FProjectWorldLayerInventory& Inventory,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError);
}
