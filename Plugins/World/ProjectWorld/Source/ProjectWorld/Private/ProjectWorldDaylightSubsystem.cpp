#include "ProjectWorldDaylightSubsystem.h"
#include "ProjectLightComponent.h"
#include "ProjectTimeSubsystem.h"
#include "ProjectTimeWorldContextComponent.h"
#include "GeoReferencingSystem.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/PostProcessVolume.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectWorldDaylight, Log, All);

bool UProjectWorldDaylightSubsystem::DoesSupportWorldType(EWorldType::Type Type) const
{
	return Type == EWorldType::Game || Type == EWorldType::PIE;
}

void UProjectWorldDaylightSubsystem::OnWorldBeginPlay(UWorld& World)
{
	Super::OnWorldBeginPlay(World);
	if (World.GetNetMode() != NM_Standalone) return;
	AGeoReferencingSystem* Geo = nullptr;
	UProjectTimeWorldContextComponent* Context = nullptr;
	for (TActorIterator<AGeoReferencingSystem> It(&World); It; ++It)
	{
		TArray<UProjectTimeWorldContextComponent*> Contexts;
		It->GetComponents(Contexts);
		if (Contexts.Num() > 1)
		{
			UE_LOG(LogProjectWorldDaylight, Error, TEXT("Duplicate cooked time components; daylight refused."));
			return;
		}
		if (auto* Candidate = It->FindComponentByClass<UProjectTimeWorldContextComponent>())
		{
			if (Context)
			{
				UE_LOG(LogProjectWorldDaylight, Error, TEXT("Duplicate cooked time contexts; daylight refused."));
				return;
			}
			Context = Candidate;
			Geo = *It;
		}
	}
	// Existing static scenes deliberately have no clock context.
	if (!Context) return;
	auto* Time = World.GetSubsystem<UProjectTimeSubsystem>();
	auto* Controller = Geo->FindComponentByClass<UProjectLightComponent>();
	TArray<UProjectLightComponent*> Controllers;
	Geo->GetComponents(Controllers);
	FName GridTag;
	for (FName Tag : Geo->Tags)
		if (Tag.ToString().StartsWith(TEXT("ProjectWorld.Grid="))) GridTag = Tag;
	auto HasRole = [GridTag](const AActor* Actor, const TCHAR* Role)
	{
		return Actor && !GridTag.IsNone() && Actor->Tags.Contains(GridTag) &&
			Actor->Tags.Contains(TEXT("ProjectWorld.Generated.v1")) &&
			Actor->Tags.Contains(FName(*(FString(TEXT("ProjectWorld.PresentationRole=")) + Role)));
	};
	const bool bRoles = Controller && Controller->Bindings.IsValidForWorld(World) &&
		Controller->Bindings.NightFill && Controller->Bindings.NightFill->GetOwner() == Controller->Bindings.Sun->GetOwner() &&
		Controller->Bindings.NightFill->GetFName() == TEXT("ProjectNightFill") &&
		ProjectLight::IsNightFillConfigured(*Controller->Bindings.NightFill) &&
		HasRole(Controller->Bindings.Sun->GetOwner(), TEXT("Sun")) &&
		HasRole(Controller->Bindings.SkyLight->GetOwner(), TEXT("SkyLight")) &&
		HasRole(Controller->Bindings.Atmosphere->GetOwner(), TEXT("SkyAtmosphere")) &&
		HasRole(Controller->Bindings.Fog->GetOwner(), TEXT("HeightFog")) &&
		HasRole(Controller->Bindings.Cloud->GetOwner(), TEXT("VolumetricCloud")) &&
		HasRole(Controller->Bindings.PostProcess, TEXT("PostProcess"));
	FString Error(TEXT("ambiguous-or-foreign-generated-daylight-binding"));
	if (!Time || Controllers.Num() != 1 || !Geo->Tags.Contains(TEXT("ProjectWorld.Generated.v1")) ||
		!bRoles || !Time->BindContext(*Context, Error))
	{
		UE_LOG(LogProjectWorldDaylight, Error, TEXT("Daylight context/binding refused: %s"), *Error);
		return;
	}
	FGeographicCoordinates Coordinates;
	FVector East, North, Up;
	Geo->EngineToGeographic(FVector::ZeroVector, Coordinates);
	Geo->GetENUVectorsAtEngineLocation(FVector::ZeroVector, East, North, Up);
	if (!Controller->Start(*Time, Coordinates.Latitude, Coordinates.Longitude, East, North, Up, Error))
	{
		Time->UnbindContext();
		UE_LOG(LogProjectWorldDaylight, Error, TEXT("Daylight activation refused: %s"), *Error);
		return;
	}
	Light = Controller;
	UE_LOG(LogProjectWorldDaylight, Display, TEXT("Bound runtime daylight world=%s zone=%s lat=%.6f lon=%.6f"),
		*World.GetName(), *Context->ZoneId, Coordinates.Latitude, Coordinates.Longitude);
}

void UProjectWorldDaylightSubsystem::Deinitialize()
{
	if (Light.IsValid()) Light->Stop();
	Light.Reset();
	Super::Deinitialize();
}

void UProjectWorldDaylightSubsystem::OnWorldEndPlay(UWorld& World)
{
	if (Light.IsValid()) Light->Stop();
	Light.Reset();
	Super::OnWorldEndPlay(World);
}
