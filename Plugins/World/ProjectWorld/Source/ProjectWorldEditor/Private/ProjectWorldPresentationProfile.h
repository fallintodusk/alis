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
	FString RoadMaterialPath;
	FString BuildingMaterialPath;
	FString CloudMaterialPath;
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
	FString TerrainMaterialObjectPath;
	FString TerrainMaterialPackageSha256;
	FString TerrainMaterialSemanticIdentity;
	FString TerrainMaterialManifestSha256;
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
