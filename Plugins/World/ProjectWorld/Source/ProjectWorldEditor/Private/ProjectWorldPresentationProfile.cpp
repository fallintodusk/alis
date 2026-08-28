// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldPresentationProfile.h"

#include "ProjectWorldPresentationMaterialBinding.h"
#include "ProjectWorldSchemaReference.h"

#include "Dom/JsonObject.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Utilities/ProjectSha256.h"

namespace ProjectWorldPresentationProfile
{
	namespace
	{
		const TCHAR* ExpectedSchemaFilename =
			TEXT("project_world_presentation_profile.schema.json");
		bool IsIdentifierToken(const FString& Value)
		{
			for (const TCHAR Character : Value)
			{
				const bool bLowerAscii = Character >= TEXT('a') && Character <= TEXT('z');
				const bool bDigitAscii = Character >= TEXT('0') && Character <= TEXT('9');
				if (!bLowerAscii && !bDigitAscii && Character != TEXT('_'))
				{
					return false;
				}
			}
			return !Value.IsEmpty();
		}

		bool HasOnlyFields(
			const TSharedPtr<FJsonObject>& Object,
			std::initializer_list<const TCHAR*> Allowed,
			FString& OutError)
		{
			TSet<FString> AllowedFields;
			for (const TCHAR* Field : Allowed)
			{
				AllowedFields.Add(Field);
			}
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Object->Values)
			{
				if (!AllowedFields.Contains(Field.Key))
				{
					OutError = FString::Printf(TEXT("Unknown field: %s"), *Field.Key);
					return false;
				}
			}
			return true;
		}

		bool RequireString(
			const TSharedPtr<FJsonObject>& Object,
			const TCHAR* Name,
			FString& OutValue,
			FString& OutError)
		{
			if (!Object->TryGetStringField(Name, OutValue) || OutValue.IsEmpty())
			{
				OutError = FString::Printf(TEXT("Missing or empty string: %s"), Name);
				return false;
			}
			return true;
		}

		bool RequireNumber(
			const TSharedPtr<FJsonObject>& Object,
			const TCHAR* Name,
			double Minimum,
			double Maximum,
			double& OutValue,
			FString& OutError)
		{
			if (!Object->TryGetNumberField(Name, OutValue) || !FMath::IsFinite(OutValue) ||
				OutValue < Minimum || OutValue > Maximum)
			{
				OutError = FString::Printf(TEXT("Number is missing or outside its contract: %s"), Name);
				return false;
			}
			return true;
		}

		bool RequireVector(
			const TSharedPtr<FJsonObject>& Object,
			const TCHAR* Name,
			int32 Count,
			double Minimum,
			double Maximum,
			TArray<double>& OutValues,
			FString& OutError)
		{
			const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
			if (!Object->TryGetArrayField(Name, Values) || Values == nullptr || Values->Num() != Count)
			{
				OutError = FString::Printf(TEXT("Array has the wrong shape: %s"), Name);
				return false;
			}
			for (const TSharedPtr<FJsonValue>& Value : *Values)
			{
				double Number = 0.0;
				if (!Value->TryGetNumber(Number) || !FMath::IsFinite(Number) ||
					Number < Minimum || Number > Maximum)
				{
					OutError = FString::Printf(TEXT("Array value is outside its contract: %s"), Name);
					return false;
				}
				OutValues.Add(Number);
			}
			return true;
		}

		bool IsEngineMaterialPath(const FString& Path)
		{
			return Path.StartsWith(TEXT("/Engine/")) && Path.Contains(TEXT("."));
		}
	}

	bool Load(
		const FString& Path,
		FProjectWorldPresentationProfile& OutProfile,
		FString& OutErrorCode,
		FString& OutError)
	{
		OutProfile = FProjectWorldPresentationProfile();
		OutErrorCode.Reset();
		OutError.Reset();
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			OutErrorCode = TEXT("presentation-profile-read");
			OutError = Path;
			return false;
		}

		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid() ||
			!HasOnlyFields(Root, {
				TEXT("$schema"), TEXT("schema_version"), TEXT("profile_id"),
				TEXT("materials"),
				TEXT("outdoor_environment"), TEXT("capture_viewpoints")}, OutError))
		{
			OutErrorCode = TEXT("presentation-profile-json");
			return false;
		}

		FString Schema;
		double SchemaVersion = 0.0;
		if (!RequireString(Root, TEXT("$schema"), Schema, OutError) ||
			!ProjectWorldSchemaReference::ResolvesToCanonical(
				Path,
				Schema,
				ExpectedSchemaFilename,
				OutError) ||
			!RequireNumber(Root, TEXT("schema_version"), 1.0, 1.0, SchemaVersion, OutError) ||
			!RequireString(Root, TEXT("profile_id"), OutProfile.ProfileId, OutError))
		{
			OutErrorCode = TEXT("presentation-profile-contract");
			return false;
		}
		if (!IsIdentifierToken(OutProfile.ProfileId))
		{
			OutErrorCode = TEXT("presentation-profile-contract");
			OutError = TEXT("profile_id must contain only lowercase ASCII letters, digits, and underscores.");
			return false;
		}

		const TSharedPtr<FJsonObject>* Materials = nullptr;
		if (!Root->TryGetObjectField(TEXT("materials"), Materials) || Materials == nullptr ||
			!HasOnlyFields(*Materials, {TEXT("road"), TEXT("building"), TEXT("cloud")}, OutError) ||
			!RequireString(*Materials, TEXT("road"), OutProfile.RoadMaterialPath, OutError) ||
			!RequireString(*Materials, TEXT("building"), OutProfile.BuildingMaterialPath, OutError) ||
			!RequireString(*Materials, TEXT("cloud"), OutProfile.CloudMaterialPath, OutError) ||
			!IsEngineMaterialPath(OutProfile.RoadMaterialPath) ||
			!IsEngineMaterialPath(OutProfile.BuildingMaterialPath) ||
			!IsEngineMaterialPath(OutProfile.CloudMaterialPath))
		{
			OutErrorCode = TEXT("presentation-profile-material");
			return false;
		}

		const TSharedPtr<FJsonObject>* Environment = nullptr;
		TArray<double> SunRotation;
		double SunIntensity = 0.0;
		double SkyLightIntensity = 0.0;
		double FogDensity = 0.0;
		double FogHeightFalloff = 0.0;
		double Exposure = 0.0;
		if (!Root->TryGetObjectField(TEXT("outdoor_environment"), Environment) || Environment == nullptr ||
			!HasOnlyFields(*Environment, {
				TEXT("sun_rotation"), TEXT("sun_intensity_lux"), TEXT("sky_light_intensity"),
				TEXT("fog_density"), TEXT("fog_height_falloff"), TEXT("fixed_exposure_ev100")}, OutError) ||
			!RequireVector(*Environment, TEXT("sun_rotation"), 3, -360.0, 360.0, SunRotation, OutError) ||
			!RequireNumber(*Environment, TEXT("sun_intensity_lux"), 1000.0, 120000.0, SunIntensity, OutError) ||
			!RequireNumber(*Environment, TEXT("sky_light_intensity"), 0.0, 10.0, SkyLightIntensity, OutError) ||
			!RequireNumber(*Environment, TEXT("fog_density"), 0.0, 0.05, FogDensity, OutError) ||
			!RequireNumber(*Environment, TEXT("fog_height_falloff"), 0.001, 2.0, FogHeightFalloff, OutError) ||
			!RequireNumber(*Environment, TEXT("fixed_exposure_ev100"), -10.0, 20.0, Exposure, OutError))
		{
			OutErrorCode = TEXT("presentation-profile-environment");
			return false;
		}
		OutProfile.SunRotation = FRotator(SunRotation[0], SunRotation[1], SunRotation[2]);
		OutProfile.SunIntensityLux = SunIntensity;
		OutProfile.SkyLightIntensity = SkyLightIntensity;
		OutProfile.FogDensity = FogDensity;
		OutProfile.FogHeightFalloff = FogHeightFalloff;
		OutProfile.FixedExposureEv100 = Exposure;

		const TArray<TSharedPtr<FJsonValue>>* Viewpoints = nullptr;
		TSet<FString> ViewpointNames;
		if (!Root->TryGetArrayField(TEXT("capture_viewpoints"), Viewpoints) ||
			Viewpoints == nullptr || Viewpoints->Num() < 3)
		{
			OutErrorCode = TEXT("presentation-profile-viewpoints");
			OutError = TEXT("At least three capture viewpoints are required.");
			return false;
		}
		for (const TSharedPtr<FJsonValue>& Value : *Viewpoints)
		{
			const TSharedPtr<FJsonObject>* Viewpoint = nullptr;
			FProjectWorldCaptureViewpoint Parsed;
			TArray<double> Location;
			TArray<double> LookAt;
			double Height = 0.0;
			double LookAtHeight = 0.0;
			double FieldOfView = 0.0;
			if (!Value->TryGetObject(Viewpoint) || Viewpoint == nullptr ||
				!HasOnlyFields(*Viewpoint, {
					TEXT("name"), TEXT("location_normalized"), TEXT("height_m"),
					TEXT("look_at_normalized"), TEXT("look_at_height_m"), TEXT("fov_degrees")}, OutError) ||
				!RequireString(*Viewpoint, TEXT("name"), Parsed.Name, OutError) ||
				!IsIdentifierToken(Parsed.Name) ||
				ViewpointNames.Contains(Parsed.Name) ||
				!RequireVector(*Viewpoint, TEXT("location_normalized"), 2, -1.0, 2.0, Location, OutError) ||
				!RequireNumber(*Viewpoint, TEXT("height_m"), 1.0, 5000.0, Height, OutError) ||
				!RequireVector(*Viewpoint, TEXT("look_at_normalized"), 2, -1.0, 2.0, LookAt, OutError) ||
				!RequireNumber(*Viewpoint, TEXT("look_at_height_m"), -1000.0, 5000.0, LookAtHeight, OutError) ||
				!RequireNumber(*Viewpoint, TEXT("fov_degrees"), 20.0, 120.0, FieldOfView, OutError))
			{
				OutErrorCode = TEXT("presentation-profile-viewpoints");
				return false;
			}
			ViewpointNames.Add(Parsed.Name);
			Parsed.LocationNormalized = FVector2D(Location[0], Location[1]);
			Parsed.HeightMeters = Height;
			Parsed.LookAtNormalized = FVector2D(LookAt[0], LookAt[1]);
			Parsed.LookAtHeightMeters = LookAtHeight;
			Parsed.FieldOfViewDegrees = FieldOfView;
			OutProfile.CaptureViewpoints.Add(MoveTemp(Parsed));
		}

		if (!FProjectSha256::HashFile(Path, OutProfile.ProfileHash))
		{
			OutErrorCode = TEXT("presentation-profile-hash");
			OutError = Path;
			return false;
		}
		return true;
	}

	bool ResolveResources(
		const FProjectWorldPresentationProfile& Profile,
		FProjectWorldPresentationResources& OutResources,
		FString& OutError)
	{
		if (!ProjectWorldPresentationMaterialBinding::ResolveTerrain(OutResources, OutError))
		{
			return false;
		}
		OutResources.RoadMaterial = LoadObject<UMaterialInterface>(nullptr, *Profile.RoadMaterialPath);
		OutResources.BuildingMaterial = LoadObject<UMaterialInterface>(nullptr, *Profile.BuildingMaterialPath);
		OutResources.CloudMaterial = LoadObject<UMaterialInterface>(nullptr, *Profile.CloudMaterialPath);
		if (OutResources.RoadMaterial == nullptr || OutResources.BuildingMaterial == nullptr ||
			OutResources.CloudMaterial == nullptr)
		{
			OutError = TEXT("One or more approved presentation materials could not be loaded.");
			return false;
		}
		return true;
	}
}
