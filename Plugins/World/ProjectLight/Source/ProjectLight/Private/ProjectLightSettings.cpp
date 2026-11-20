#include "ProjectLightSettings.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/PostProcessVolume.h"
#include "HAL/IConsoleManager.h"
#include "SunPosition.h"

void ProjectLight::ConfigureNightFill(UDirectionalLightComponent& Light)
{
	Light.SetMobility(EComponentMobility::Movable);
	Light.SetAtmosphereSunLightIndex(1);
	Light.SetAtmosphereSunLight(true);
	Light.SetCastShadows(false);
	Light.SetLightColor(FLinearColor(0.7f, 0.8f, 1.0f));
	Light.SetIntensity(0);
}

bool ProjectLight::IsNightFillConfigured(const UDirectionalLightComponent& Light)
{
	return Light.Mobility == EComponentMobility::Movable && Light.GetAtmosphereSunLightIndex() == 1 &&
		Light.IsUsedAsAtmosphereSunLight() && !Light.CastShadows &&
		Light.GetLightColor().Equals(FLinearColor(0.7f, 0.8f, 1.0f), 0.01f);
}

bool FProjectLightSettings::IsValid() const
{
	const float Values[] = { SunIntensityLux, SkyLightIntensity, FogDensity, FogHeightFalloff,
		FixedExposureEv100, MinEv100, MaxEv100, HistogramMinEv100, HistogramMaxEv100,
		ExposureSpeedUp, ExposureSpeedDown, LiveUpdateSeconds, DemoUpdateSeconds, NightFillLux, NightMinEv100 };
	for (float Value : Values) if (!FMath::IsFinite(Value)) return false;
	return !SunRotation.ContainsNaN() && SunIntensityLux >= 0 && SkyLightIntensity >= 0 &&
		FogDensity >= 0 && FogHeightFalloff > 0 && LiveUpdateSeconds >= 0.02f &&
		DemoUpdateSeconds >= 0.02f && NightFillLux >= 0 && NightMinEv100 <= MinEv100 && (!bHistogram || (MinEv100 < MaxEv100 &&
		HistogramMinEv100 <= MinEv100 && HistogramMaxEv100 >= MaxEv100 &&
		ExposureSpeedUp > 0 && ExposureSpeedDown > 0));
}

FString FProjectLightSettings::IntegrityRecord() const
{
	return FString::Printf(TEXT("%.17g,%.17g,%.17g|%.9g|%.9g|%.9g|%.9g|%.9g|%d|%.9g|%.9g|%.9g|%.9g|%.9g|%.9g|%.9g|%.9g"),
		SunRotation.Pitch, SunRotation.Yaw, SunRotation.Roll, SunIntensityLux, SkyLightIntensity, FogDensity, FogHeightFalloff,
		FixedExposureEv100, bHistogram, MinEv100, MaxEv100, HistogramMinEv100,
		HistogramMaxEv100, ExposureSpeedUp, ExposureSpeedDown, LiveUpdateSeconds, DemoUpdateSeconds) +
		FString::Printf(TEXT("|%.9g|%.9g"), NightFillLux, NightMinEv100);
}

bool FProjectLightBindings::IsValidForWorld(const UWorld& World) const
{
	return IsValid(Sun) && IsValid(SkyLight) && IsValid(Atmosphere) && IsValid(Fog) &&
		IsValid(Cloud) && IsValid(PostProcess) && IsValid(CloudMaterial) &&
		(!NightFill || (IsValid(NightFill) && NightFill->GetWorld() == &World)) &&
		Sun->GetWorld() == &World && SkyLight->GetWorld() == &World && Atmosphere->GetWorld() == &World &&
		Fog->GetWorld() == &World && Cloud->GetWorld() == &World && PostProcess->GetWorld() == &World;
}

void ProjectLight::ConfigureFixedExposure(FPostProcessSettings& Settings, float Ev100)
{
	Settings.bOverride_AutoExposureMethod = true;
	Settings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
	Settings.bOverride_AutoExposureMinBrightness = false;
	Settings.bOverride_AutoExposureMaxBrightness = false;
	Settings.bOverride_HistogramLogMin = false;
	Settings.bOverride_HistogramLogMax = false;
	Settings.bOverride_AutoExposureSpeedUp = false;
	Settings.bOverride_AutoExposureSpeedDown = false;
	Settings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	Settings.AutoExposureApplyPhysicalCameraExposure = true;
	Settings.bOverride_CameraISO = true;
	Settings.CameraISO = 100.0f;
	Settings.bOverride_CameraShutterSpeed = true;
	Settings.CameraShutterSpeed = FMath::Pow(2.0f, Ev100) / 16.0f;
	Settings.bOverride_DepthOfFieldFstop = true;
	Settings.DepthOfFieldFstop = 4.0f;
	Settings.bOverride_AutoExposureBias = true;
	Settings.AutoExposureBias = 0.0f;
}

bool ProjectLight::ApplyStatic(const FProjectLightSettings& Settings, const FProjectLightBindings& Bindings)
{
	if (!Settings.IsValid() || !Bindings.Sun || !Bindings.Sun->GetWorld() ||
		!Bindings.IsValidForWorld(*Bindings.Sun->GetWorld())) return false;
	Bindings.Sun->SetMobility(EComponentMobility::Movable);
	Bindings.Sun->SetWorldRotation(Settings.SunRotation);
	Bindings.Sun->SetIntensity(Settings.SunIntensityLux);
	Bindings.Sun->SetAtmosphereSunLight(true);
	if (Bindings.NightFill) ConfigureNightFill(*Bindings.NightFill);
	Bindings.SkyLight->SetMobility(EComponentMobility::Movable);
	Bindings.SkyLight->SetIntensity(Settings.SkyLightIntensity);
	Bindings.SkyLight->SetRealTimeCapture(true);
	Bindings.Fog->SetFogDensity(Settings.FogDensity);
	Bindings.Fog->SetFogHeightFalloff(Settings.FogHeightFalloff);
	Bindings.Fog->SetVolumetricFog(true);
	Bindings.Cloud->SetMaterial(Bindings.CloudMaterial);
	Bindings.PostProcess->BlendWeight = 1.0f;
	auto& Exposure = Bindings.PostProcess->Settings;
	ConfigureFixedExposure(Exposure, Settings.FixedExposureEv100);
	if (Settings.bHistogram)
	{
		const auto* Extended = IConsoleManager::Get().FindTConsoleVariableDataInt(
			TEXT("r.DefaultFeature.AutoExposure.ExtendDefaultLuminanceRange"));
		check(Extended);
		const bool bExtended = Extended->GetValueOnGameThread() != 0;
		Exposure.AutoExposureMethod = EAutoExposureMethod::AEM_Histogram;
		Exposure.bOverride_AutoExposureMinBrightness = true;
		Exposure.bOverride_AutoExposureMaxBrightness = true;
		Exposure.AutoExposureMinBrightness = bExtended ? Settings.MinEv100 : FMath::Pow(2.0f, Settings.MinEv100);
		Exposure.AutoExposureMaxBrightness = bExtended ? Settings.MaxEv100 : FMath::Pow(2.0f, Settings.MaxEv100);
		Exposure.bOverride_HistogramLogMin = true;
		Exposure.bOverride_HistogramLogMax = true;
		Exposure.HistogramLogMin = Settings.HistogramMinEv100;
		Exposure.HistogramLogMax = Settings.HistogramMaxEv100;
		Exposure.bOverride_AutoExposureSpeedUp = true;
		Exposure.bOverride_AutoExposureSpeedDown = true;
		Exposure.AutoExposureSpeedUp = Settings.ExposureSpeedUp;
		Exposure.AutoExposureSpeedDown = Settings.ExposureSpeedDown;
	}
	return true;
}

bool ProjectLight::SunDirection(FDateTime Utc, double Latitude, double Longitude,
	const FVector& East, const FVector& North, const FVector& Up, FVector& OutToSun, double& OutElevation)
{
	if (!FMath::IsFinite(Latitude) || !FMath::IsFinite(Longitude) || FMath::Abs(Latitude) > 90 ||
		FMath::Abs(Longitude) > 180 || East.ContainsNaN() || North.ContainsNaN() || Up.ContainsNaN() ||
		!East.IsNormalized() || !North.IsNormalized() || !Up.IsNormalized() ||
		FMath::Abs(East.Dot(North)) > 0.001 || FMath::Abs(East.Dot(Up)) > 0.001 ||
		FMath::Abs(North.Dot(Up)) > 0.001 || Utc.GetTicks() < 0 || Utc > FDateTime::MaxValue()) return false;
	FSunPositionData Sun;
	USunPositionFunctionLibrary::GetSunPosition(Latitude, Longitude, 0.0f, false,
		Utc.GetYear(), Utc.GetMonth(), Utc.GetDay(), Utc.GetHour(), Utc.GetMinute(), Utc.GetSecond(), Sun);
	// Native SunPosition includes a 180-degree engine-rotation offset in elevation.
	const double Elevation = Sun.CorrectedElevation - 180.0;
	if (!FMath::IsFinite(Elevation) || !FMath::IsFinite(Sun.Azimuth) || FMath::Abs(Elevation) > 90.1) return false;
	const double El = FMath::DegreesToRadians(Elevation);
	const double Az = FMath::DegreesToRadians(static_cast<double>(Sun.Azimuth));
	OutToSun = (North * FMath::Cos(Az) + East * FMath::Sin(Az)) * FMath::Cos(El) + Up * FMath::Sin(El);
	OutElevation = Elevation;
	return true;
}
