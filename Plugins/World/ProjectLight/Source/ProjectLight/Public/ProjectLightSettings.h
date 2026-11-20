#pragma once

#include "CoreMinimal.h"
#include "Engine/Scene.h"
#include "ProjectLightSettings.generated.h"

class UDirectionalLightComponent;
class USkyLightComponent;
class USkyAtmosphereComponent;
class UExponentialHeightFogComponent;
class UVolumetricCloudComponent;
class APostProcessVolume;
class UMaterialInterface;

USTRUCT()
struct PROJECTLIGHT_API FProjectLightSettings
{
	GENERATED_BODY()
	UPROPERTY() FRotator SunRotation = FRotator::ZeroRotator;
	UPROPERTY() float SunIntensityLux = 50000.0f;
	UPROPERTY() float SkyLightIntensity = 1.0f;
	UPROPERTY() float FogDensity = 0.002f;
	UPROPERTY() float FogHeightFalloff = 0.2f;
	UPROPERTY() float FixedExposureEv100 = 12.0f;
	UPROPERTY() bool bHistogram = false;
	UPROPERTY() float MinEv100 = 6.0f;
	UPROPERTY() float MaxEv100 = 14.0f;
	UPROPERTY() float HistogramMinEv100 = 0.0f;
	UPROPERTY() float HistogramMaxEv100 = 16.0f;
	UPROPERTY() float ExposureSpeedUp = 3.0f;
	UPROPERTY() float ExposureSpeedDown = 1.0f;
	UPROPERTY() float LiveUpdateSeconds = 1.0f;
	UPROPERTY() float DemoUpdateSeconds = 0.1f;
	UPROPERTY() float NightFillLux = 0.2f;
	UPROPERTY() float NightMinEv100 = -4.5f;

	bool IsValid() const;
	FString IntegrityRecord() const;
};

USTRUCT()
struct PROJECTLIGHT_API FProjectLightBindings
{
	GENERATED_BODY()
	UPROPERTY() TObjectPtr<UDirectionalLightComponent> Sun;
	UPROPERTY() TObjectPtr<UDirectionalLightComponent> NightFill;
	UPROPERTY() TObjectPtr<USkyLightComponent> SkyLight;
	UPROPERTY() TObjectPtr<USkyAtmosphereComponent> Atmosphere;
	UPROPERTY() TObjectPtr<UExponentialHeightFogComponent> Fog;
	UPROPERTY() TObjectPtr<UVolumetricCloudComponent> Cloud;
	UPROPERTY() TObjectPtr<APostProcessVolume> PostProcess;
	UPROPERTY() TObjectPtr<UMaterialInterface> CloudMaterial;

	bool IsValidForWorld(const UWorld& World) const;
};

namespace ProjectLight
{
	PROJECTLIGHT_API void ConfigureNightFill(UDirectionalLightComponent& Light);
	PROJECTLIGHT_API bool IsNightFillConfigured(const UDirectionalLightComponent& Light);
	PROJECTLIGHT_API void ConfigureFixedExposure(FPostProcessSettings& Settings, float Ev100);
	PROJECTLIGHT_API bool ApplyStatic(const FProjectLightSettings& Settings, const FProjectLightBindings& Bindings);
	PROJECTLIGHT_API bool SunDirection(FDateTime Utc, double Latitude, double Longitude,
		const FVector& East, const FVector& North, const FVector& Up, FVector& OutToSun, double& OutElevation);
}
