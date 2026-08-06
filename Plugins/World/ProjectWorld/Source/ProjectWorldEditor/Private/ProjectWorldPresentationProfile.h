// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;

struct FProjectWorldCaptureViewpoint
{
	FString Name;
	FVector2D LocationNormalized = FVector2D::ZeroVector;
	double HeightMeters = 0.0;
	FVector2D LookAtNormalized = FVector2D::ZeroVector;
	double LookAtHeightMeters = 0.0;
	float FieldOfViewDegrees = 60.0f;
};

struct FProjectWorldPresentationProfile
{
	FString ProfileId;
	FString ProfileHash;
	FString TerrainMaterialPath;
	FString RoadMaterialPath;
	FString BuildingMaterialPath;
	FString CloudMaterialPath;
	FLinearColor TerrainPrimaryColor = FLinearColor::Black;
	FLinearColor TerrainSecondaryColor = FLinearColor::Black;
	FLinearColor TerrainLineColor = FLinearColor::Black;
	float TerrainTileScale = 1.0f;
	FRotator SunRotation = FRotator::ZeroRotator;
	float SunIntensityLux = 0.0f;
	float SkyLightIntensity = 0.0f;
	float FogDensity = 0.0f;
	float FogHeightFalloff = 0.0f;
	float FixedExposureEv100 = 0.0f;
	TArray<FProjectWorldCaptureViewpoint> CaptureViewpoints;
};

struct FProjectWorldPresentationResources
{
	UMaterialInterface* TerrainMaterial = nullptr;
	UMaterialInterface* RoadMaterial = nullptr;
	UMaterialInterface* BuildingMaterial = nullptr;
	UMaterialInterface* CloudMaterial = nullptr;
};

namespace ProjectWorldPresentationProfile
{
	bool Load(
		const FString& Path,
		FProjectWorldPresentationProfile& OutProfile,
		FString& OutErrorCode,
		FString& OutError);

	bool ResolveResources(
		const FProjectWorldPresentationProfile& Profile,
		FProjectWorldPresentationResources& OutResources,
		FString& OutError);
}
