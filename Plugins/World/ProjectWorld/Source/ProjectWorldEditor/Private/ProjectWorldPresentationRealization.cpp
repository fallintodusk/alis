// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldPresentationRealization.h"

#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldGeneratedGeometry.h"
#include "ProjectWorldPresentationProfile.h"
#include "ProjectWorldRealizationService.h"

#include "ActorFactories/ActorFactoryBoxVolume.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"

namespace ProjectWorldPresentationRealization
{
	namespace
	{
		const FString RolePrefix(TEXT("ProjectWorld.PresentationRole="));
		const FString ProfilePrefix(TEXT("ProjectWorld.Presentation="));
		const FString ProfileHashPrefix(TEXT("ProjectWorld.PresentationHash="));
		const FString GridPrefix(TEXT("ProjectWorld.Grid="));

		FString RoleFromActor(const AActor& Actor)
		{
			for (const FName& Tag : Actor.Tags)
			{
				const FString Value = Tag.ToString();
				if (Value.StartsWith(RolePrefix))
				{
					return Value.RightChop(RolePrefix.Len());
				}
			}
			return FString();
		}

		bool HasSingleTagValue(const AActor& Actor, const FString& Prefix, const FString& Value)
		{
			const FName Expected(*(Prefix + Value));
			return Actor.Tags.Contains(Expected) &&
				!Actor.Tags.ContainsByPredicate([&Prefix, &Expected](const FName& Tag)
				{
					return Tag != Expected && Tag.ToString().StartsWith(Prefix);
				});
		}

		void SetTagValue(AActor& Actor, const FString& Prefix, const FString& Value)
		{
			Actor.Tags.RemoveAll([&Prefix](const FName& Tag)
			{
				return Tag.ToString().StartsWith(Prefix);
			});
			Actor.Tags.Add(FName(*(Prefix + Value)));
		}

		void ConfigureFixedExposure(FPostProcessSettings& Settings, float FixedExposureEv100)
		{
			constexpr float ManualAperture = 4.0f;
			constexpr float ManualIso = 100.0f;
			const float ManualShutterSpeed =
				FMath::Pow(2.0f, FixedExposureEv100) /
				FMath::Square(ManualAperture);
			Settings.bOverride_AutoExposureMethod = true;
			Settings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
			Settings.bOverride_AutoExposureMinBrightness = false;
			Settings.bOverride_AutoExposureMaxBrightness = false;
			Settings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
			Settings.AutoExposureApplyPhysicalCameraExposure = true;
			Settings.bOverride_CameraISO = true;
			Settings.CameraISO = ManualIso;
			Settings.bOverride_CameraShutterSpeed = true;
			Settings.CameraShutterSpeed = ManualShutterSpeed;
			Settings.bOverride_DepthOfFieldFstop = true;
			Settings.DepthOfFieldFstop = ManualAperture;
			Settings.bOverride_AutoExposureBias = true;
			Settings.AutoExposureBias = 0.0f;
		}

		bool EnsureEditorVolumeBrush(APostProcessVolume& Volume)
		{
			if (Volume.GetRootComponent() != nullptr &&
				Volume.GetRootComponent()->Bounds.SphereRadius > UE_SMALL_NUMBER)
			{
				return true;
			}

			UActorFactoryBoxVolume* Factory = NewObject<UActorFactoryBoxVolume>();
			Factory->PostSpawnActor(nullptr, &Volume);
			return Volume.GetRootComponent() != nullptr &&
				Volume.GetRootComponent()->Bounds.SphereRadius > UE_SMALL_NUMBER;
		}

		bool ConfigureAlwaysLoadedUnboundVolume(APostProcessVolume& Volume)
		{
			Volume.bUnbound = false;
			if (!Volume.CanChangeIsSpatiallyLoadedFlag())
			{
				return false;
			}
			Volume.SetIsSpatiallyLoaded(false);
			Volume.bUnbound = true;
			return !Volume.GetIsSpatiallyLoaded();
		}

		void SetIdentity(
			AActor& Actor,
			const FString& Role,
			const FProjectWorldCanonicalBundle& Bundle,
			const FProjectWorldPresentationProfile& Profile)
		{
			Actor.Tags.AddUnique(ProjectWorldGeneratedGeometry::GeneratedTag);
			SetTagValue(Actor, RolePrefix, Role);
			SetTagValue(Actor, ProfilePrefix, Profile.ProfileId);
			SetTagValue(Actor, ProfileHashPrefix, Profile.ProfileHash);
			SetTagValue(Actor, GridPrefix, Bundle.GridId);
			Actor.SetActorLabel(FString::Printf(TEXT("ProjectWorld_%s"), *Role), false);
			if (Actor.CanChangeIsSpatiallyLoadedFlag())
			{
				Actor.SetIsSpatiallyLoaded(false);
			}
			Actor.MarkPackageDirty();
		}

		template <typename TActor>
		TActor* ReuseOrSpawn(
			UWorld* World,
			const FString& Role,
			const TMap<FString, AActor*>& Existing,
			const FProjectWorldCanonicalBundle& Bundle,
			const FProjectWorldPresentationProfile& Profile,
			FProjectWorldRealizationResult& OutResult)
		{
			const FGuid ExpectedGuid = ProjectWorldGeneratedGeometry::StableGuid(
				Bundle.GridId + TEXT("|presentation|") + Role);
			if (AActor* const* ExistingActor = Existing.Find(Role))
			{
				TActor* TypedActor = CastChecked<TActor>(*ExistingActor);
				if (TypedActor->GetActorGuid() == ExpectedGuid)
				{
					++OutResult.UpdatedActorCount;
					return TypedActor;
				}
				if (!World->EditorDestroyActor(TypedActor, true))
				{
					return nullptr;
				}
				++OutResult.RemovedActorCount;
			}

			FActorSpawnParameters SpawnParameters;
			SpawnParameters.Name = FName(*FString::Printf(TEXT("ProjectWorld_%s"), *Role));
			SpawnParameters.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Required_ErrorAndReturnNull;
			SpawnParameters.OverrideActorGuid = ExpectedGuid;
			TActor* Actor = World->SpawnActor<TActor>(TActor::StaticClass(), FTransform::Identity, SpawnParameters);
			if (Actor != nullptr)
			{
				++OutResult.CreatedActorCount;
			}
			return Actor;
		}

		FVector CanonicalPoint(
			const FProjectWorldCanonicalBundle& Bundle,
			const FVector2D& Normalized,
			double HeightMeters)
		{
			FVector4d Bounds = Bundle.Cells[0].Bounds;
			for (const FProjectWorldCanonicalCell& Cell : Bundle.Cells)
			{
				Bounds.X = FMath::Min(Bounds.X, Cell.Bounds.X);
				Bounds.Y = FMath::Min(Bounds.Y, Cell.Bounds.Y);
				Bounds.Z = FMath::Max(Bounds.Z, Cell.Bounds.Z);
				Bounds.W = FMath::Max(Bounds.W, Cell.Bounds.W);
			}
			return FVector(
				FMath::Lerp(Bounds.X, Bounds.Z, Normalized.X),
				FMath::Lerp(Bounds.Y, Bounds.W, Normalized.Y),
				Bundle.HeightOriginMeters + HeightMeters);
		}

		bool PrepareExistingActors(
			UWorld* World,
			const TMap<FString, UClass*>& ExpectedClasses,
			TMap<FString, AActor*>& OutExisting,
			FProjectWorldRealizationResult& OutResult)
		{
			TArray<AActor*> InvalidActors;
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				if (!It->Tags.Contains(ProjectWorldGeneratedGeometry::GeneratedTag))
				{
					continue;
				}
				const FString Role = RoleFromActor(**It);
				if (Role.IsEmpty())
				{
					continue;
				}
				UClass* const* ExpectedClass = ExpectedClasses.Find(Role);
				if (ExpectedClass == nullptr || !It->IsA(*ExpectedClass) || OutExisting.Contains(Role))
				{
					InvalidActors.Add(*It);
					continue;
				}
				OutExisting.Add(Role, *It);
			}

			for (AActor* Actor : InvalidActors)
			{
				if (!World->EditorDestroyActor(Actor, true))
				{
					return false;
				}
				++OutResult.RemovedActorCount;
			}
			return true;
		}
	}

	bool Apply(
		UWorld* World,
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldPresentationProfile& Profile,
		const FProjectWorldPresentationResources& Resources,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError)
	{
		if (World == nullptr || Bundle.Cells.IsEmpty())
		{
			OutError = TEXT("Presentation realization requires a World and at least one canonical cell.");
			return false;
		}

		TMap<FString, UClass*> ExpectedClasses = {
			{TEXT("Sun"), ADirectionalLight::StaticClass()},
			{TEXT("SkyAtmosphere"), ASkyAtmosphere::StaticClass()},
			{TEXT("SkyLight"), ASkyLight::StaticClass()},
			{TEXT("HeightFog"), AExponentialHeightFog::StaticClass()},
			{TEXT("VolumetricCloud"), AVolumetricCloud::StaticClass()},
			{TEXT("PostProcess"), APostProcessVolume::StaticClass()}};
		for (const FProjectWorldCaptureViewpoint& Viewpoint : Profile.CaptureViewpoints)
		{
			ExpectedClasses.Add(TEXT("Capture_") + Viewpoint.Name, ACameraActor::StaticClass());
		}

		TMap<FString, AActor*> Existing;
		if (!PrepareExistingActors(World, ExpectedClasses, Existing, OutResult))
		{
			OutError = TEXT("Cannot remove a stale generated presentation actor.");
			return false;
		}
		bool bPresentationCurrent = Existing.Num() == ExpectedClasses.Num();
		for (const TPair<FString, UClass*>& Expected : ExpectedClasses)
		{
			AActor* const* Actor = Existing.Find(Expected.Key);
			const FGuid ExpectedGuid = ProjectWorldGeneratedGeometry::StableGuid(
				Bundle.GridId + TEXT("|presentation|") + Expected.Key);
			bPresentationCurrent &= Actor != nullptr && (*Actor)->GetActorGuid() == ExpectedGuid &&
				HasSingleTagValue(**Actor, RolePrefix, Expected.Key) &&
				HasSingleTagValue(**Actor, ProfilePrefix, Profile.ProfileId) &&
				HasSingleTagValue(**Actor, ProfileHashPrefix, Profile.ProfileHash) &&
				HasSingleTagValue(**Actor, GridPrefix, Bundle.GridId) &&
				!(*Actor)->GetIsSpatiallyLoaded();
		}
		if (bPresentationCurrent)
		{
			OutResult.PresentationActorCount = ExpectedClasses.Num();
			OutResult.CaptureViewpointCount = Profile.CaptureViewpoints.Num();
			OutResult.PreservedActorCount += ExpectedClasses.Num();
			return true;
		}

		ADirectionalLight* Sun = ReuseOrSpawn<ADirectionalLight>(World, TEXT("Sun"), Existing, Bundle, Profile, OutResult);
		ASkyAtmosphere* Atmosphere = ReuseOrSpawn<ASkyAtmosphere>(World, TEXT("SkyAtmosphere"), Existing, Bundle, Profile, OutResult);
		ASkyLight* SkyLight = ReuseOrSpawn<ASkyLight>(World, TEXT("SkyLight"), Existing, Bundle, Profile, OutResult);
		AExponentialHeightFog* Fog = ReuseOrSpawn<AExponentialHeightFog>(World, TEXT("HeightFog"), Existing, Bundle, Profile, OutResult);
		AVolumetricCloud* Cloud = ReuseOrSpawn<AVolumetricCloud>(World, TEXT("VolumetricCloud"), Existing, Bundle, Profile, OutResult);
		APostProcessVolume* PostProcess = ReuseOrSpawn<APostProcessVolume>(World, TEXT("PostProcess"), Existing, Bundle, Profile, OutResult);
		if (Sun == nullptr || Atmosphere == nullptr || SkyLight == nullptr || Fog == nullptr ||
			Cloud == nullptr || PostProcess == nullptr)
		{
			OutError = TEXT("Cannot create the complete generated outdoor environment.");
			return false;
		}
		if (!EnsureEditorVolumeBrush(*PostProcess))
		{
			OutError = TEXT("Generated Post Process Volume has no valid editor brush.");
			return false;
		}

		Sun->SetActorRotation(Profile.SunRotation);
		Sun->GetComponent()->SetMobility(EComponentMobility::Movable);
		Sun->GetComponent()->SetIntensity(Profile.SunIntensityLux);
		Sun->GetComponent()->SetAtmosphereSunLight(true);
		SkyLight->GetLightComponent()->SetMobility(EComponentMobility::Movable);
		SkyLight->GetLightComponent()->SetIntensity(Profile.SkyLightIntensity);
		SkyLight->GetLightComponent()->SetRealTimeCapture(true);
		Fog->GetComponent()->SetFogDensity(Profile.FogDensity);
		Fog->GetComponent()->SetFogHeightFalloff(Profile.FogHeightFalloff);
		Fog->GetComponent()->SetVolumetricFog(true);
		UVolumetricCloudComponent* CloudComponent =
			Cloud->FindComponentByClass<UVolumetricCloudComponent>();
		if (CloudComponent == nullptr)
		{
			OutError = TEXT("Generated Volumetric Cloud has no component.");
			return false;
		}
		CloudComponent->SetMaterial(Resources.CloudMaterial);
		if (!ConfigureAlwaysLoadedUnboundVolume(*PostProcess))
		{
			OutError = TEXT("Generated Post Process Volume cannot be made always loaded.");
			return false;
		}
		PostProcess->BlendWeight = 1.0f;
		ConfigureFixedExposure(PostProcess->Settings, Profile.FixedExposureEv100);

		SetIdentity(*Sun, TEXT("Sun"), Bundle, Profile);
		SetIdentity(*Atmosphere, TEXT("SkyAtmosphere"), Bundle, Profile);
		SetIdentity(*SkyLight, TEXT("SkyLight"), Bundle, Profile);
		SetIdentity(*Fog, TEXT("HeightFog"), Bundle, Profile);
		SetIdentity(*Cloud, TEXT("VolumetricCloud"), Bundle, Profile);
		SetIdentity(*PostProcess, TEXT("PostProcess"), Bundle, Profile);

		for (const FProjectWorldCaptureViewpoint& Viewpoint : Profile.CaptureViewpoints)
		{
			const FString Role = TEXT("Capture_") + Viewpoint.Name;
			ACameraActor* Camera = ReuseOrSpawn<ACameraActor>(World, Role, Existing, Bundle, Profile, OutResult);
			if (Camera == nullptr)
			{
				OutError = FString::Printf(TEXT("Cannot create capture viewpoint: %s"), *Viewpoint.Name);
				return false;
			}
			const FVector Location = FProjectWorldCanonicalLoader::CanonicalToUnreal(
				Bundle,
				CanonicalPoint(Bundle, Viewpoint.LocationNormalized, Viewpoint.HeightMeters));
			const FVector LookAt = FProjectWorldCanonicalLoader::CanonicalToUnreal(
				Bundle,
				CanonicalPoint(Bundle, Viewpoint.LookAtNormalized, Viewpoint.LookAtHeightMeters));
			Camera->SetActorLocationAndRotation(Location, (LookAt - Location).Rotation());
			UCameraComponent* CameraComponent = Camera->GetCameraComponent();
			CameraComponent->SetFieldOfView(Viewpoint.FieldOfViewDegrees);
			CameraComponent->PostProcessBlendWeight = 1.0f;
			ConfigureFixedExposure(CameraComponent->PostProcessSettings, Profile.FixedExposureEv100);
			SetIdentity(*Camera, Role, Bundle, Profile);
		}
		OutResult.PresentationActorCount = ExpectedClasses.Num();
		OutResult.CaptureViewpointCount = Profile.CaptureViewpoints.Num();
		return true;
	}
}
