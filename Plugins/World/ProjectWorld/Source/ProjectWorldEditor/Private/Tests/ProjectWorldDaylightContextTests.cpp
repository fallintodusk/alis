#include "ProjectWorldDaylightRealization.h"
#include "ProjectWorldDaylightTestGameInstance.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldRuntimeProfile.h"
#include "ProjectWorldPresentationProfile.h"
#include "ProjectWorldRealizationService.h"
#include "ProjectWorldSemanticEvidence.h"
#include "ProjectTimeWorldContextComponent.h"
#include "ProjectTimeSubsystem.h"
#include "ProjectLightComponent.h"
#include "GeoReferencingSystem.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/GameModeBase.h"
#include "Materials/MaterialInterface.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "HAL/FileManager.h"
#include "UObject/SavePackage.h"
#include "PackageTools.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldDaylightContextTest,
	"Project.World.Realization.Presentation.DaylightLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldDaylightContextTest::RunTest(const FString&)
{
	const FString Folder = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() /
		TEXT("tmp/world/time_light/persistence") / FGuid::NewGuid().ToString());
	IFileManager::Get().MakeDirectory(*Folder, true);
	const FString Mount(TEXT("/ProjectDaylightFixture/"));
	FPackageName::RegisterMountPoint(Mount, Folder + TEXT("/"));
	ON_SCOPE_EXIT { FPackageName::UnRegisterMountPoint(Mount, Folder + TEXT("/")); };
	const FString PackageName = Mount + TEXT("L_DaylightContext");
	UPackage* Package = CreatePackage(*PackageName);
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false, TEXT("L_DaylightContext"), Package, false);
	if (!TestNotNull(TEXT("A tiny editor scene exists"), World)) return false;
	World->SetFlags(RF_Public | RF_Standalone);
	bool bGameContext = false;
	ON_SCOPE_EXIT
	{
		if (World)
		{
			if (World->HasBegunPlay()) World->EndPlay(EEndPlayReason::Quit);
			if (bGameContext) GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
	};
	TestNull(TEXT("Editor scene has no automatic runtime clock"), World->GetSubsystem<UProjectTimeSubsystem>());
	FProjectWorldCanonicalBundle Bundle;
	Bundle.GridId = TEXT("grid_0000000000000000");
	TestEqual(TEXT("No canonical Terrain cells are needed"), Bundle.Cells.Num(), 0);
	const FName GridTag(*(TEXT("ProjectWorld.Grid=") + Bundle.GridId));
	auto SpawnRole = [&](UClass* Class, const TCHAR* Role)
	{
		AActor* Actor = World->SpawnActor<AActor>(Class);
		Actor->Tags.Add(TEXT("ProjectWorld.Generated.v1"));
		Actor->Tags.Add(GridTag);
		if (Role) Actor->Tags.Add(FName(*(FString(TEXT("ProjectWorld.PresentationRole=")) + Role)));
		return Actor;
	};
	auto* Geo = CastChecked<AGeoReferencingSystem>(SpawnRole(AGeoReferencingSystem::StaticClass(), nullptr));
	Geo->SetIsSpatiallyLoaded(false);
	SpawnRole(ADirectionalLight::StaticClass(), TEXT("Sun"));
	SpawnRole(ASkyLight::StaticClass(), TEXT("SkyLight"));
	SpawnRole(ASkyAtmosphere::StaticClass(), TEXT("SkyAtmosphere"));
	SpawnRole(AExponentialHeightFog::StaticClass(), TEXT("HeightFog"));
	SpawnRole(AVolumetricCloud::StaticClass(), TEXT("VolumetricCloud"));
	SpawnRole(APostProcessVolume::StaticClass(), TEXT("PostProcess"));
	AActor* Authored = World->SpawnActor<AActor>();
	FProjectWorldRuntimeProfile Runtime;
	Runtime.bEnvironmentTimeEnabled = true;
	Runtime.EnvironmentTimeZone = TEXT("Europe/Moscow");
	Runtime.ProfileId = TEXT("daylight_fixture");
	Runtime.ProfileHash = FString::ChrN(64, TCHAR('a'));
	FProjectWorldPresentationProfile Presentation;
	Presentation.SunIntensityLux = 50000;
	Presentation.SkyLightIntensity = 1;
	Presentation.FogDensity = 0.002f;
	Presentation.FogHeightFalloff = 0.2f;
	Presentation.FixedExposureEv100 = 12;
	FProjectWorldPresentationResources Resources;
	Resources.CloudMaterial = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst.m_SimpleVolumetricCloud_Inst"));
	FString Error;
	FProjectWorldRealizationResult First;
	if (!TestTrue(TEXT("World projects time/light context without Terrain"),
		ProjectWorldDaylightRealization::Apply(*World, Bundle, &Runtime, Presentation, Resources, First, Error)))
	{
		AddError(Error);
		return false;
	}
	auto* Context = Geo->FindComponentByClass<UProjectTimeWorldContextComponent>();
	auto* Light = Geo->FindComponentByClass<UProjectLightComponent>();
	if (!TestNotNull(TEXT("Context component"), Context) || !TestNotNull(TEXT("Light component"), Light)) return false;
	TestTrue(TEXT("Static realization configures movable native lights"), ProjectLight::ApplyStatic(Light->Settings, Light->Bindings));
	TestTrue(TEXT("One non-shadowing night fill is explicitly bound"), Light->Bindings.NightFill &&
		ProjectLight::IsNightFillConfigured(*Light->Bindings.NightFill));
	TestFalse(TEXT("Night fill uses a visible owner, not the hidden geographic AInfo"),
		Light->Bindings.NightFill->GetOwner()->IsHidden());
	TestEqual(TEXT("Clock cannot mutate Editor lights"), Light->GetApplyCount(), uint64(0));
	FProjectWorldRealizationResult NoOp;
	ProjectWorldDaylightRealization::Apply(*World, Bundle, &Runtime, Presentation, Resources, NoOp, Error);
	TestEqual(TEXT("No-op leaves context package alone"), NoOp.UpdatedActorCount, 0);
	const FString ExpectedLightPolicy = Light->Settings.IntegrityRecord();
	Light->Settings.NightMinEv100 += 1;
	FProjectWorldRealizationResult PolicyRepair;
	TestTrue(TEXT("Source-backed replay repairs stale night policy"), ProjectWorldDaylightRealization::Apply(
		*World, Bundle, &Runtime, Presentation, Resources, PolicyRepair, Error));
	TestEqual(TEXT("Night-policy repair updates only the context owner"), PolicyRepair.UpdatedActorCount, 1);
	TestEqual(TEXT("Replay restores the complete Light policy"), Light->Settings.IntegrityRecord(), ExpectedLightPolicy);
	const FString OriginalHash = Runtime.ProfileHash;
	Runtime.ProfileHash = FString::ChrN(64, TCHAR('z'));
	FProjectWorldRealizationResult Refusal;
	TestFalse(TEXT("Malformed source identity refuses before writes"),
		ProjectWorldDaylightRealization::Apply(*World, Bundle, &Runtime, Presentation, Resources, Refusal, Error));
	TestEqual(TEXT("Refusal changes no owner"), Refusal.UpdatedActorCount, 0);
	TestEqual(TEXT("Refusal preserves valid context"), Context->ProfileHash, OriginalHash);
	Runtime.ProfileHash = OriginalHash;
	FProjectWorldRealizationResult Before;
	ProjectWorldSemanticEvidence::Capture(World, Before, Error);
	Context->ZoneId = TEXT("Unknown/Invalid");
	FProjectWorldRealizationResult Repair;
	TestTrue(TEXT("Source repairs corrupted context"),
		ProjectWorldDaylightRealization::Apply(*World, Bundle, &Runtime, Presentation, Resources, Repair, Error));
	TestEqual(TEXT("Repair affects one existing owner"), Repair.UpdatedActorCount, 1);
	FProjectWorldRealizationResult After;
	ProjectWorldSemanticEvidence::Capture(World, After, Error);
	TestEqual(TEXT("Repaired semantics equal original"), After.SemanticFingerprint, Before.SemanticFingerprint);
	TestTrue(TEXT("Authored neighbor retained"), IsValid(Authored));
	Light->Bindings.NightFill->SetCastShadows(true);
	FProjectWorldRealizationResult NightRepair;
	TestTrue(TEXT("Source-backed replay repairs the night owner"), ProjectWorldDaylightRealization::Apply(
		*World, Bundle, &Runtime, Presentation, Resources, NightRepair, Error));
	TestEqual(TEXT("Repair records Sun and context persistence"), NightRepair.UpdatedActorCount, 2);
	TestTrue(TEXT("Night policy is restored"), ProjectLight::IsNightFillConfigured(*Light->Bindings.NightFill));
	const FGuid GeoGuid = Geo->GetActorGuid();
	const FString Filename = Folder / TEXT("L_DaylightContext.umap");
	FSavePackageArgs Save;
	Save.TopLevelFlags = RF_Public | RF_Standalone;
	Save.SaveFlags = SAVE_NoError;
	if (!TestTrue(TEXT("Save actual map package"), UPackage::SavePackage(Package, World, *Filename, Save))) return false;
	World->DestroyWorld(false);
	World = nullptr;
	FText UnloadError;
	if (!TestTrue(TEXT("Unload before reloading disk bytes"), UPackageTools::UnloadPackages({Package}, UnloadError, true)))
	{
		AddError(UnloadError.ToString());
		return false;
	}
	// Match GameEngine's preload envelope before world/subsystem construction.
	UWorld::WorldTypePreLoadMap.Add(FName(*PackageName), EWorldType::Game);
	UPackage* ReloadedPackage = LoadPackage(nullptr, *PackageName, LOAD_None);
	UWorld::WorldTypePreLoadMap.Remove(FName(*PackageName));
	World = ReloadedPackage ? UWorld::FindWorldInPackage(ReloadedPackage) : nullptr;
	if (!TestNotNull(TEXT("Reload actual saved world"), World)) return false;
	World->WorldType = EWorldType::Game;
	auto& GameContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	GameContext.SetCurrentWorld(World);
	bGameContext = true;
	auto* Instance = NewObject<UProjectWorldDaylightTestGameInstance>(GEngine);
	Instance->BindFixtureContext(GameContext);
	World->SetGameInstance(Instance);
	GameContext.OwningGameInstance = Instance;
	if (!World->IsInitialized()) World->InitWorld(UWorld::InitializationValues().AllowAudioPlayback(false));
	AddInfo(FString::Printf(TEXT("Reloaded startup: type=%d time=%s initialized=%d"),
		static_cast<int32>(World->WorldType), World->GetSubsystem<UProjectTimeSubsystem>() ? TEXT("present") : TEXT("absent"), World->IsInitialized()));
	World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
	World->SetGameMode(FURL());
	World->FlushLevelStreaming(EFlushLevelStreamingType::Visibility);
	World->InitializeActorsForPlay(FURL());
	AGeoReferencingSystem* ReloadedGeo = nullptr;
	for (TActorIterator<AGeoReferencingSystem> It(World); It; ++It) ReloadedGeo = *It;
	if (!TestNotNull(TEXT("Georeference persisted"), ReloadedGeo)) return false;
	Context = ReloadedGeo->FindComponentByClass<UProjectTimeWorldContextComponent>();
	Light = ReloadedGeo->FindComponentByClass<UProjectLightComponent>();
	if (!TestNotNull(TEXT("Serialized context persisted"), Context) || !TestNotNull(TEXT("Serialized Light binding persisted"), Light)) return false;
	TestEqual(TEXT("Complete Light policy survives disk reload"), Light->Settings.IntegrityRecord(), ExpectedLightPolicy);
	TestTrue(TEXT("Reloaded context validates"), Context->IsValidContext());
	TestEqual(TEXT("Zone survives disk reload"), Context->ZoneId, Runtime.EnvironmentTimeZone);
	TestEqual(TEXT("Geographic owner identity unchanged"), ReloadedGeo->GetActorGuid(), GeoGuid);
	TestFalse(TEXT("Context owner is not spatially streamed"), ReloadedGeo->GetIsSpatiallyLoaded());
	TestTrue(TEXT("Actor/component references survive reload"), Light->Bindings.IsValidForWorld(*World));
	TestTrue(TEXT("Night fill survives disk reload on the existing Sun owner"), Light->Bindings.NightFill &&
		Light->Bindings.NightFill->GetOwner() == Light->Bindings.Sun->GetOwner() && ProjectLight::IsNightFillConfigured(*Light->Bindings.NightFill));
	World->BeginPlay();
	auto* RuntimeTime = World->GetSubsystem<UProjectTimeSubsystem>();
	FProjectTimeSnapshot Snapshot;
	TestTrue(TEXT("Runtime composition binds clock from saved context"), RuntimeTime && RuntimeTime->TryReadTime(Snapshot));
	TestTrue(TEXT("Runtime composition applies native daylight"), Light->GetApplyCount() > 0);
	TestFalse(TEXT("Ordinary GameMode grants no demo override"), RuntimeTime && RuntimeTime->CanControlTime());
	Light->Stop();
	if (RuntimeTime) RuntimeTime->UnbindContext();
	if (!RuntimeTime) return false;
	const FString ContextIdentity = Context->IntegrityRecord();
	const FString LightIdentity = Light->Settings.IntegrityRecord();
	auto RepairRegistration = [&](UActorComponent& Component, const TCHAR* Label)
	{
		Component.UnregisterComponent();
		ON_SCOPE_EXIT { if (!Component.IsRegistered()) Component.RegisterComponent(); };
		TestFalse(FString::Printf(TEXT("%s corruption is unregistered"), Label), Component.IsRegistered());
		if (&Component == Context)
			TestFalse(TEXT("Runtime binding refuses the unregistered context"), RuntimeTime->BindContext(*Context, Error));
		FProjectWorldRealizationResult RegistrationRepair;
		TestTrue(FString::Printf(TEXT("%s source replay succeeds"), Label), ProjectWorldDaylightRealization::Apply(
			*World, Bundle, &Runtime, Presentation, Resources, RegistrationRepair, Error));
		TestEqual(FString::Printf(TEXT("%s repair records one existing owner"), Label), RegistrationRepair.UpdatedActorCount, 1);
		if (!TestTrue(FString::Printf(TEXT("%s registration is repaired"), Label), Component.IsRegistered())) return;
		TestEqual(TEXT("Repair retains the context instance"), ReloadedGeo->FindComponentByClass<UProjectTimeWorldContextComponent>(), Context);
		TestEqual(TEXT("Repair retains the controller instance"), ReloadedGeo->FindComponentByClass<UProjectLightComponent>(), Light);
		TestEqual(TEXT("Registration repair preserves context identity"), Context->IntegrityRecord(), ContextIdentity);
		TestEqual(TEXT("Registration repair preserves Light policy"), Light->Settings.IntegrityRecord(), LightIdentity);
		TestEqual(TEXT("Registration repair creates no actors"), RegistrationRepair.CreatedActorCount, 0);
		TestTrue(TEXT("Repaired context binds through the runtime provider"), RuntimeTime->BindContext(*Context, Error));
		TestTrue(TEXT("Repaired context supplies actual runtime time"), RuntimeTime->TryReadTime(Snapshot));
		FProjectWorldRealizationResult RegistrationNoOp;
		TestTrue(TEXT("Repeated registered replay succeeds"), ProjectWorldDaylightRealization::Apply(
			*World, Bundle, &Runtime, Presentation, Resources, RegistrationNoOp, Error));
		TestEqual(TEXT("Repeated registered replay writes no owner"), RegistrationNoOp.UpdatedActorCount, 0);
		RuntimeTime->UnbindContext();
	};
	RepairRegistration(*Context, TEXT("Context"));
	RepairRegistration(*Light, TEXT("Controller"));
	TestTrue(TEXT("Repaired runtime context binds for daylight"), RuntimeTime->BindContext(*Context, Error));
	FGeographicCoordinates Coordinates;
	FVector East, North, Up;
	ReloadedGeo->EngineToGeographic(FVector::ZeroVector, Coordinates);
	ReloadedGeo->GetENUVectorsAtEngineLocation(FVector::ZeroVector, East, North, Up);
	const uint64 ApplyCount = Light->GetApplyCount();
	TestTrue(TEXT("Repaired controller starts with the real clock"), Light->Start(
		*RuntimeTime, Coordinates.Latitude, Coordinates.Longitude, East, North, Up, Error));
	TestTrue(TEXT("Repaired controller applies native daylight"), Light->GetApplyCount() > ApplyCount);
	TestTrue(TEXT("Repaired controller owns a registered native tick"), Light->PrimaryComponentTick.IsTickFunctionRegistered());
	RuntimeTime->TimeChanged().Broadcast();
	TestTrue(TEXT("Time notification schedules a controller tick"), Light->IsComponentTickEnabled());
	World->Tick(LEVELTICK_All, 1.0f / 60.0f);
	TestFalse(TEXT("Actual world tick consumes the pending daylight apply"), Light->IsComponentTickEnabled());
	Light->Stop();
	RuntimeTime->UnbindContext();
	AActor* NightOwner = Light->Bindings.Sun->GetOwner();
	Runtime.bEnvironmentTimeEnabled = false;
	FProjectWorldRealizationResult Disable;
	TestTrue(TEXT("Explicit opt-out retires context"),
		ProjectWorldDaylightRealization::Apply(*World, Bundle, &Runtime, Presentation, Resources, Disable, Error));
	TestNull(TEXT("Time component retired"), ReloadedGeo->FindComponentByClass<UProjectTimeWorldContextComponent>());
	TestNull(TEXT("Light controller retired"), ReloadedGeo->FindComponentByClass<UProjectLightComponent>());
	TArray<UDirectionalLightComponent*> RemainingLights;
	NightOwner->GetComponents(RemainingLights);
	TestFalse(TEXT("Opt-out retires the owned night fill from the actor"), RemainingLights.ContainsByPredicate(
		[](const UDirectionalLightComponent* Component) { return Component->GetFName() == TEXT("ProjectNightFill"); }));
	FProjectWorldRealizationResult DisabledNoOp;
	TestTrue(TEXT("Repeated opt-out succeeds"), ProjectWorldDaylightRealization::Apply(
		*World, Bundle, &Runtime, Presentation, Resources, DisabledNoOp, Error));
	TestEqual(TEXT("Retired objects do not dirty repeated opt-out"), DisabledNoOp.UpdatedActorCount, 0);
	Runtime.bEnvironmentTimeEnabled = true;
	FProjectWorldRealizationResult EnableAgain;
	TestTrue(TEXT("Opt-in after opt-out creates a valid binding"), ProjectWorldDaylightRealization::Apply(
		*World, Bundle, &Runtime, Presentation, Resources, EnableAgain, Error));
	return true;
}

REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FProjectWorldDaylightContextTest, "Project.World.Realization.Presentation.DaylightLifecycle", "[Slow][Integration][World]")
