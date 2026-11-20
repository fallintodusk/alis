#include "ProjectWorldDaylightRealization.h"
#include "Algo/AllOf.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldRuntimeProfile.h"
#include "ProjectWorldLightSettings.h"
#include "ProjectWorldRealizationService.h"
#include "ProjectTimeWorldContextComponent.h"
#include "ProjectTimeZone.h"
#include "ProjectLightComponent.h"
#include "GeoReferencingSystem.h"
#include "EngineUtils.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/VolumetricCloudComponent.h"

namespace ProjectWorldDaylightRealization
{
	bool Apply(UWorld& World, const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldRuntimeProfile* Runtime, const FProjectWorldPresentationProfile& Presentation,
		const FProjectWorldPresentationResources& Resources, FProjectWorldRealizationResult& Result, FString& Error)
	{
		if (Runtime && Runtime->bEnvironmentTimeEnabled && (Runtime->ProfileId.IsEmpty() || Runtime->ProfileHash.Len() != 64 ||
			!Algo::AllOf(Runtime->ProfileHash, [](TCHAR Char) { return FChar::IsHexDigit(Char); }) ||
			!ProjectTimeZone::IsValid(Runtime->EnvironmentTimeZone)))
		{
			Error = TEXT("Invalid cooked time context input.");
			return false;
		}
		const FName GridTag(*(TEXT("ProjectWorld.Grid=") + Bundle.GridId));
		AGeoReferencingSystem* Geo = nullptr;
		TMap<FString, AActor*> Roles;
		for (TActorIterator<AActor> It(&World); It; ++It)
		{
			if (!It->Tags.Contains(GridTag) || !It->Tags.Contains(TEXT("ProjectWorld.Generated.v1"))) continue;
			if (auto* Candidate = Cast<AGeoReferencingSystem>(*It))
			{
				if (Geo) { Error = TEXT("Duplicate generated georeference context."); return false; }
				Geo = Candidate;
			}
			for (FName Tag : It->Tags)
			{
				const FString Text = Tag.ToString();
				const FString Prefix(TEXT("ProjectWorld.PresentationRole="));
				if (Text.StartsWith(Prefix))
				{
					const FString Role = Text.RightChop(Prefix.Len());
					if (Roles.Contains(Role)) { Error = TEXT("Duplicate lighting role."); return false; }
					Roles.Add(Role, *It);
				}
			}
		}
		if (!Geo)
		{
			if (Runtime && Runtime->bEnvironmentTimeEnabled) Error = TEXT("Time context requires the generated georeference actor.");
			return !Runtime || !Runtime->bEnvironmentTimeEnabled;
		}
		auto* Context = Geo->FindComponentByClass<UProjectTimeWorldContextComponent>();
		auto* Controller = Geo->FindComponentByClass<UProjectLightComponent>();
		AActor* NightOwner = Roles.FindRef(TEXT("Sun"));
		auto* Night = NightOwner ? FindObject<UDirectionalLightComponent>(NightOwner, TEXT("ProjectNightFill")) : nullptr;
		if (!IsValid(Night)) Night = nullptr;
		TArray<UProjectTimeWorldContextComponent*> Contexts;
		TArray<UProjectLightComponent*> Controllers;
		Geo->GetComponents(Contexts);
		Geo->GetComponents(Controllers);
		if (Contexts.Num() > 1 || Controllers.Num() > 1)
		{
			Error = TEXT("Ambiguous time/light component ownership.");
			return false;
		}
		const bool bEnabled = Runtime ? Runtime->bEnvironmentTimeEnabled : Context != nullptr;
		if (!bEnabled)
		{
			if (Context || Controller || Night)
			{
				Geo->Modify();
				if (Context) Context->DestroyComponent();
				if (Controller) Controller->DestroyComponent();
				if (Night) { NightOwner->Modify(); Night->DestroyComponent(); NightOwner->MarkPackageDirty(); ++Result.UpdatedActorCount; }
				Geo->MarkPackageDirty();
				++Result.UpdatedActorCount;
			}
			return true;
		}
		if (!Runtime && !Context->IsValidContext())
		{
			Error = TEXT("Cannot reuse an invalid retained time context without its source profile.");
			return false;
		}
		const FString Zone = Runtime ? Runtime->EnvironmentTimeZone : Context->ZoneId;
		const FString ProfileId = Runtime ? Runtime->ProfileId : Context->ProfileId;
		const FString ProfileHash = Runtime ? Runtime->ProfileHash : Context->ProfileHash;
		FProjectLightBindings Bindings;
		auto* Sun = Cast<ADirectionalLight>(Roles.FindRef(TEXT("Sun")));
		auto* Sky = Cast<ASkyLight>(Roles.FindRef(TEXT("SkyLight")));
		auto* Atmosphere = Cast<ASkyAtmosphere>(Roles.FindRef(TEXT("SkyAtmosphere")));
		auto* Fog = Cast<AExponentialHeightFog>(Roles.FindRef(TEXT("HeightFog")));
		auto* Cloud = Cast<AVolumetricCloud>(Roles.FindRef(TEXT("VolumetricCloud")));
		Bindings.Sun = Sun ? Sun->GetComponent() : nullptr;
		Bindings.NightFill = Night;
		Bindings.SkyLight = Sky ? Sky->GetLightComponent() : nullptr;
		Bindings.Atmosphere = Atmosphere ? Atmosphere->GetComponent() : nullptr;
		Bindings.Fog = Fog ? Fog->GetComponent() : nullptr;
		Bindings.Cloud = Cloud ? Cloud->FindComponentByClass<UVolumetricCloudComponent>() : nullptr;
		Bindings.PostProcess = Cast<APostProcessVolume>(Roles.FindRef(TEXT("PostProcess")));
		Bindings.CloudMaterial = Resources.CloudMaterial;
		const auto Settings = ProjectWorldLightSettings::FromProfile(Presentation);
		if (!Bindings.IsValidForWorld(World) || !Settings.IsValid())
		{
			Error = TEXT("Incomplete generated lighting binding.");
			return false;
		}
		if (Context && Controller && Context->IsRegistered() && Controller->IsRegistered() && Context->ZoneId == Zone &&
			Context->ProfileHash == ProfileHash && Context->ProfileId == ProfileId &&
			Controller->Settings.IntegrityRecord() == Settings.IntegrityRecord() &&
			Controller->Bindings.IsValidForWorld(World) && Controller->Bindings.Sun == Bindings.Sun &&
			Controller->Bindings.NightFill == Bindings.NightFill &&
			Controller->Bindings.SkyLight == Bindings.SkyLight && Controller->Bindings.Atmosphere == Bindings.Atmosphere &&
			Controller->Bindings.Fog == Bindings.Fog && Controller->Bindings.Cloud == Bindings.Cloud &&
			Controller->Bindings.CloudMaterial == Bindings.CloudMaterial && Controller->Bindings.PostProcess == Bindings.PostProcess)
		{
			if (Night && Night->IsRegistered() && ProjectLight::IsNightFillConfigured(*Night)) return true;
		}
		Geo->Modify();
		if (!Night)
		{
			NightOwner->Modify();
			Night = NewObject<UDirectionalLightComponent>(NightOwner, TEXT("ProjectNightFill"), RF_Transactional);
			NightOwner->AddInstanceComponent(Night);
			NightOwner->MarkPackageDirty();
			++Result.UpdatedActorCount;
		}
		if (!ProjectLight::IsNightFillConfigured(*Night))
		{
			NightOwner->Modify();
			Night->Modify();
			ProjectLight::ConfigureNightFill(*Night);
			NightOwner->MarkPackageDirty();
			if (Night->IsRegistered()) ++Result.UpdatedActorCount;
		}
		if (!Night->IsRegistered()) Night->RegisterComponent();
		Bindings.NightFill = Night;
		if (!Context)
		{
			Context = NewObject<UProjectTimeWorldContextComponent>(Geo, TEXT("ProjectTimeContext"), RF_Transactional);
			Geo->AddInstanceComponent(Context);
		}
		if (!Controller)
		{
			Controller = NewObject<UProjectLightComponent>(Geo, TEXT("ProjectDaylight"), RF_Transactional);
			Geo->AddInstanceComponent(Controller);
		}
		Context->ZoneId = Zone;
		Context->ProfileId = ProfileId;
		Context->ProfileHash = ProfileHash;
		Controller->Settings = Settings;
		Controller->Bindings = Bindings;
		if (!Context->IsRegistered()) Context->RegisterComponent();
		if (!Controller->IsRegistered()) Controller->RegisterComponent();
		if (!Context->IsRegistered() || !Controller->IsRegistered() || !Night->IsRegistered())
		{
			Error = TEXT("Cooked time/light component registration failed.");
			return false;
		}
		Geo->MarkPackageDirty();
		++Result.UpdatedActorCount;
		return true;
	}
}
