#include "ProjectLightComponent.h"
#include "ProjectLightTestTimeView.h"
#include "ProjectWorldDaylightTestGameInstance.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/Engine.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectLightForwardPriorityTest,
	"Project.Time.Integration.LightForwardPriority", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProjectLightForwardPriorityTest::RunTest(const FString&)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	auto* Instance = NewObject<UProjectWorldDaylightTestGameInstance>(GEngine);
	Instance->BindFixtureContext(Context);
	Context.OwningGameInstance = Instance;
	World->SetGameInstance(Instance);
	if (!World->IsInitialized()) World->InitWorld(UWorld::InitializationValues().AllowAudioPlayback(false));
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* Owner = World->SpawnActor<AActor>();
	auto* Light = NewObject<UProjectLightComponent>(Owner);
	Owner->AddInstanceComponent(Light);
	Light->RegisterComponent();
	ON_SCOPE_EXIT { Light->Stop(); };
	auto Component = [&]<typename T>()
	{
		auto* Result = NewObject<T>(Owner);
		Owner->AddInstanceComponent(Result);
		Result->RegisterComponent();
		return Result;
	};
	auto& Bindings = Light->Bindings;
	Bindings.Sun = Component.operator()<UDirectionalLightComponent>();
	Bindings.NightFill = Component.operator()<UDirectionalLightComponent>();
	Bindings.SkyLight = Component.operator()<USkyLightComponent>();
	Bindings.Atmosphere = Component.operator()<USkyAtmosphereComponent>();
	Bindings.Fog = Component.operator()<UExponentialHeightFogComponent>();
	Bindings.Cloud = Component.operator()<UVolumetricCloudComponent>();
	Bindings.PostProcess = World->SpawnActor<APostProcessVolume>();
	Bindings.CloudMaterial = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst.m_SimpleVolumetricCloud_Inst"));
	if (!TestTrue(TEXT("Native fixture bindings"), Bindings.IsValidForWorld(*World))) return false;
	Light->Settings.bHistogram = true;
	TestTrue(TEXT("Existing static policy applied"), ProjectLight::ApplyStatic(Light->Settings, Bindings));
	auto& Post = Bindings.PostProcess->Settings;
	Post.bOverride_LumenSceneLightingUpdateSpeed = true;
	Post.LumenSceneLightingUpdateSpeed = 2.0f;
	const auto* Extended = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DefaultFeature.AutoExposure.ExtendDefaultLuminanceRange"));
	if (!TestNotNull(TEXT("Native exposure-unit configuration exists"), Extended)) return false;
	auto NativeBound = [Extended](float Ev) { return Extended->GetValueOnGameThread() != 0 ? Ev : FMath::Exp2(Ev); };
	auto* Time = NewObject<UProjectLightTestTimeView>(World);
	Time->World = World;
	Time->Snapshot.Rate = 0;
	Time->Snapshot.Utc = FDateTime(2026, 3, 21, 12);
	FString Error;
	if (!TestTrue(TEXT("Read-only provider drives the real controller"), Light->Start(
		*Time, 0, 0, FVector(1, 0, 0), FVector(0, -1, 0), FVector(0, 0, 1), Error))) return false;
	TestTrue(TEXT("Day selects Sun"), Bindings.Sun->ForwardShadingPriority > Bindings.NightFill->ForwardShadingPriority);
	TestTrue(TEXT("Day retains its native Histogram bound"), FMath::IsNearlyEqual(
		Bindings.PostProcess->Settings.AutoExposureMinBrightness, NativeBound(Light->Settings.MinEv100)));
	auto Seek = [&](FDateTime Utc)
	{
		Time->Snapshot.Utc = Utc;
		Time->Changed.Broadcast();
		Light->TickComponent(0, LEVELTICK_All, nullptr);
	};
	Seek(FDateTime(2026, 3, 21, 0));
	TestTrue(TEXT("Large explicit seek temporarily accelerates Scene lighting"),
		Post.bOverride_LumenSceneLightingUpdateSpeed && Post.LumenSceneLightingUpdateSpeed >= 4.0f);
	TestTrue(TEXT("Night disables direct Sun"), Bindings.Sun->Intensity == 0 && Bindings.NightFill->Intensity > 0);
	TestTrue(TEXT("Night selects fill, not zero-intensity Sun"), Bindings.NightFill->ForwardShadingPriority > Bindings.Sun->ForwardShadingPriority);
	TestTrue(TEXT("Qualified night floor reaches native Histogram settings"), FMath::IsNearlyEqual(
		Bindings.PostProcess->Settings.AutoExposureMinBrightness, NativeBound(-4.5f)));
	FDateTime Twilight;
	for (int32 Minute = 0; Minute < 60; ++Minute)
	{
		const FDateTime Utc = FDateTime(2026, 3, 21, 18) + FTimespan::FromMinutes(Minute);
		FVector Direction; double Elevation;
		ProjectLight::SunDirection(Utc, 0, 0, FVector(1, 0, 0), FVector(0, -1, 0), FVector(0, 0, 1), Direction, Elevation);
		if (Elevation < -1 && Elevation > -5) { Twilight = Utc; break; }
	}
	if (!TestTrue(TEXT("Native solar twilight sample exists"), Twilight.GetTicks() > 0)) return false;
	Seek(Twilight);
	TestTrue(TEXT("Both directional sources active at twilight"), Bindings.Sun->Intensity > 0 && Bindings.NightFill->Intensity > 0);
	TestTrue(TEXT("Bright twilight Sun has unique forward priority"), Bindings.Sun->ForwardShadingPriority > Bindings.NightFill->ForwardShadingPriority);
	Light->Settings.SunIntensityLux = 0.01f;
	Seek(Twilight + FTimespan::FromSeconds(1));
	TestTrue(TEXT("Dominant twilight fill takes forward priority"), Bindings.NightFill->ForwardShadingPriority > Bindings.Sun->ForwardShadingPriority);
	const uint64 Before = Light->GetApplyCount();
	Seek(Time->Snapshot.Utc);
	TestEqual(TEXT("Repeated paused instant remains a no-op"), Light->GetApplyCount(), Before);
	Light->Stop();
	TestTrue(TEXT("Stop restores the original authored lighting budget"),
		Post.bOverride_LumenSceneLightingUpdateSpeed && Post.LumenSceneLightingUpdateSpeed == 2.0f);
	Bindings.NightFill = nullptr;
	Light->Settings.NightFillLux = 0;
	TestTrue(TEXT("No-fill configuration remains supported"), Light->Start(
		*Time, 0, 0, FVector(1, 0, 0), FVector(0, -1, 0), FVector(0, 0, 1), Error));
	Seek(Time->Snapshot.Utc);
	TestEqual(TEXT("Same-time notification does not boost lighting"), Post.LumenSceneLightingUpdateSpeed, 2.0f);
	Time->Snapshot.Rate = 60;
	Seek(Time->Snapshot.Utc + FTimespan::FromSeconds(6));
	TestEqual(TEXT("Small accelerated clock steps retain their budget"), Post.LumenSceneLightingUpdateSpeed, 2.0f);
	Time->Snapshot.Utc += FTimespan::FromHours(2);
	Light->TickComponent(0, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Timer-driven clock reads do not request seek recovery"), Post.LumenSceneLightingUpdateSpeed, 2.0f);
	Light->Stop();
	Post.bOverride_LumenSceneLightingUpdateSpeed = true;
	Post.LumenSceneLightingUpdateSpeed = 8.0f;
	TestTrue(TEXT("Higher authored budget remains supported"), Light->Start(
		*Time, 0, 0, FVector(1, 0, 0), FVector(0, -1, 0), FVector(0, 0, 1), Error));
	Seek(Time->Snapshot.Utc - FTimespan::FromHours(12));
	TestEqual(TEXT("Seek recovery never lowers a higher authored budget"), Post.LumenSceneLightingUpdateSpeed, 8.0f);
	Light->Stop();
	TestEqual(TEXT("Higher authored budget survives Stop"), Post.LumenSceneLightingUpdateSpeed, 8.0f);
	return true;
}
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FProjectLightForwardPriorityTest,
	"Project.Time.Integration.LightForwardPriority", "[Fast][Integration][Light]")
