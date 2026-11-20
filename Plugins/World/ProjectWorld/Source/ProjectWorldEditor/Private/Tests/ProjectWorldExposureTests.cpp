// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldPresentationProfile.h"
#include "ProjectWorldPresentationRealization.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldRealizationService.h"
#include "Tests/ProjectWorldSchemaTestUtilities.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/UObjectGlobals.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldExposureTests
{
	bool WriteProfile(const FString& Path, bool bHistogram, bool bInverted = false)
	{
		FString Source;
		const FString Fixture = FPaths::Combine(FPaths::ProjectPluginsDir(),
			TEXT("World/ProjectWorldTestData/Data/Presentation/synthetic_representative_v1.json"));
		if (!FFileHelper::LoadFileToString(Source, *Fixture)) return false;
		TSharedPtr<FJsonObject> Document;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Source), Document)) return false;
		Document->SetStringField(TEXT("$schema"), ProjectWorldSchemaTestUtilities::ReferenceFor(
			Path, TEXT("project_world_presentation_profile.schema.json")));
		if (bHistogram)
		{
			const auto Exposure = MakeShared<FJsonObject>();
			Exposure->SetNumberField(TEXT("min_ev100"), bInverted ? 15.0 : 6.0);
			Exposure->SetNumberField(TEXT("max_ev100"), 14.0);
			Exposure->SetNumberField(TEXT("histogram_min_ev100"), 0.0);
			Exposure->SetNumberField(TEXT("histogram_max_ev100"), 16.0);
			Exposure->SetNumberField(TEXT("speed_up"), 3.0);
			Exposure->SetNumberField(TEXT("speed_down"), 1.0);
			Document->GetObjectField(TEXT("outdoor_environment"))->SetObjectField(
				TEXT("gameplay_histogram_exposure"), Exposure);
		}
		FString Output;
		FJsonSerializer::Serialize(Document.ToSharedRef(), TJsonWriterFactory<>::Create(&Output));
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
		return FFileHelper::SaveStringToFile(Output, *Path);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldHistogramExposureContractTest,
	"Project.World.Realization.Presentation.HistogramContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldHistogramExposureContractTest::RunTest(const FString&)
{
	using namespace ProjectWorldExposureTests;
	const FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("tmp/world/presentation/histogram-contract/profile.json"));
	FProjectWorldPresentationProfile Profile;
	FString Code, Error;
	if (!TestTrue(TEXT("A faithful presentation fixture is writable."), WriteProfile(Path, true))) return false;
	if (!TestTrue(TEXT("The runtime parser admits the demonstrated bounded Histogram policy."),
		ProjectWorldPresentationProfile::Load(Path, Profile, Code, Error))) { AddError(Error); return false; }
	TestTrue(TEXT("Gameplay adaptation is explicit."), Profile.GameplayHistogramExposure.IsSet());
	TestEqual(TEXT("The lower EV limit retains its units."), Profile.GameplayHistogramExposure->MinEv100, 6.0f);
	TestTrue(TEXT("An inverted-range fixture is writable."), WriteProfile(Path, true, true));
	TestFalse(TEXT("Inverted exposure limits refuse before realization."),
		ProjectWorldPresentationProfile::Load(Path, Profile, Code, Error));
	TestEqual(TEXT("Invalid exposure has a structured owner error."), Code, FString(TEXT("presentation-profile-environment")));
	TestTrue(TEXT("The fixed-exposure fixture is writable."), WriteProfile(Path, false));
	TestTrue(TEXT("Fixed presentation remains an admitted intentional policy."),
		ProjectWorldPresentationProfile::Load(Path, Profile, Code, Error));
	TestFalse(TEXT("Reloading fixed input cannot retain a previous adaptive policy."), Profile.GameplayHistogramExposure.IsSet());
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldHistogramExposureLifecycleTest,
	"Project.World.Realization.Presentation.HistogramLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldHistogramExposureLifecycleTest::RunTest(const FString&)
{
	using namespace ProjectWorldExposureTests;
	const FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("tmp/world/presentation/histogram-lifecycle/profile.json"));
	FProjectWorldPresentationProfile Fixed, Adaptive;
	FProjectWorldPresentationResources Resources;
	FString Code, Error;
	if (!WriteProfile(Path, false) || !ProjectWorldPresentationProfile::Load(Path, Fixed, Code, Error) ||
		!WriteProfile(Path, true) || !ProjectWorldPresentationProfile::Load(Path, Adaptive, Code, Error) ||
		!ProjectWorldPresentationProfile::ResolveResources(Adaptive, Resources, Error)) { AddError(Error); return false; }
	FProjectWorldCanonicalBundle Bundle;
	Bundle.GridId = TEXT("grid_histogram_test");
	FProjectWorldCanonicalCell Cell;
	Cell.Bounds = FVector4d(0, 0, 100, 100);
	Bundle.Cells.Add(Cell);
	const FString Mount(TEXT("/ProjectWorldExposureTests/"));
	const FString Content = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("tmp/world/presentation/histogram-lifecycle/content/"));
	IFileManager::Get().MakeDirectory(*Content, true);
	FPackageName::RegisterMountPoint(Mount, Content);
	ON_SCOPE_EXIT { GEditor->NewMap(false); FPackageName::UnRegisterMountPoint(Mount, Content); };
	UWorld* World = GEditor->NewMap(false);
	AActor* Authored = World->SpawnActor<AActor>();
	Authored->Tags.Add(TEXT("Authored.ExposureTest"));
	FProjectWorldRealizationResult Initial, Changed, NoOp, Restored;
	if (!TestTrue(TEXT("The existing owner creates fixed presentation."),
		ProjectWorldPresentationRealization::Apply(World, Bundle, Fixed, Resources, Initial, Error)) ||
		!TestTrue(TEXT("The same owner applies the adaptive profile."),
		ProjectWorldPresentationRealization::Apply(World, Bundle, Adaptive, Resources, Changed, Error))) return false;
	TestEqual(TEXT("The transition creates no duplicate roles."), Changed.CreatedActorCount, 0);
	TestEqual(TEXT("Only nine presentation roles update."), Changed.UpdatedActorCount, 9);
	APostProcessVolume* Volume = nullptr;
	for (TActorIterator<APostProcessVolume> It(World); It; ++It) Volume = *It;
	if (!TestNotNull(TEXT("The generated volume exists."), Volume)) return false;
	TestEqual(TEXT("Gameplay uses native Histogram."), Volume->Settings.AutoExposureMethod, EAutoExposureMethod::AEM_Histogram);
	TestFalse(TEXT("Exposure remains always loaded."), Volume->GetIsSpatiallyLoaded());
	const auto* Extended = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DefaultFeature.AutoExposure.ExtendDefaultLuminanceRange"));
	if (!TestNotNull(TEXT("The native exposure-unit setting exists."), Extended)) return false;
	TestTrue(TEXT("Minimum exposure matches the native unit convention."), FMath::IsNearlyEqual(
		Volume->Settings.AutoExposureMinBrightness, Extended->GetValueOnGameThread() ? 6.0f : 64.0f));
	TestTrue(TEXT("Maximum exposure matches the native unit convention."), FMath::IsNearlyEqual(
		Volume->Settings.AutoExposureMaxBrightness, Extended->GetValueOnGameThread() ? 14.0f : 16384.0f));
	for (TActorIterator<ACameraActor> It(World); It; ++It)
	{
		TestEqual(TEXT("Explicit capture cameras retain deterministic exposure."),
			It->GetCameraComponent()->PostProcessSettings.AutoExposureMethod, EAutoExposureMethod::AEM_Manual);
	}
	TestTrue(TEXT("Authored content survives the adaptive transition."), IsValid(Authored));
	const FString Map = Mount + TEXT("Histogram");
	if (!TestTrue(TEXT("The normal native map saver persists adaptive presentation under tmp."),
		UEditorLoadingAndSavingUtils::SaveMap(World, Map))) return false;
	Volume = nullptr; Authored = nullptr;
	GEditor->NewMap(false);
	CollectGarbage(RF_NoFlags);
	World = UEditorLoadingAndSavingUtils::LoadMap(Map);
	if (!TestNotNull(TEXT("The saved map reloads through the native loader."), World)) return false;
	if (!ProjectWorldPresentationProfile::ResolveResources(Adaptive, Resources, Error)) { AddError(Error); return false; }
	for (TActorIterator<APostProcessVolume> It(World); It; ++It) Volume = *It;
	for (TActorIterator<AActor> It(World); It; ++It) if (It->ActorHasTag(TEXT("Authored.ExposureTest"))) Authored = *It;
	if (!TestNotNull(TEXT("The adaptive volume survives disk save/reload."), Volume)) return false;
	TestEqual(TEXT("Reloaded metering is Histogram."), Volume->Settings.AutoExposureMethod, EAutoExposureMethod::AEM_Histogram);
	TestEqual(TEXT("Reloaded adaptation speed is retained."), Volume->Settings.AutoExposureSpeedUp, 3.0f);
	TestTrue(TEXT("An unchanged adaptive profile applies after reload."),
		ProjectWorldPresentationRealization::Apply(World, Bundle, Adaptive, Resources, NoOp, Error));
	TestEqual(TEXT("Adaptive no-op updates no actor."), NoOp.UpdatedActorCount, 0);
	TestEqual(TEXT("Adaptive no-op preserves all roles."), NoOp.PreservedActorCount, 9);
	TestTrue(TEXT("Fixed exposure can be restored through the same owner."),
		ProjectWorldPresentationRealization::Apply(World, Bundle, Fixed, Resources, Restored, Error));
	TestEqual(TEXT("Restoration selects manual metering."), Volume->Settings.AutoExposureMethod, EAutoExposureMethod::AEM_Manual);
	TestFalse(TEXT("Restoration clears Histogram limits."), bool(Volume->Settings.bOverride_AutoExposureMinBrightness));
	TestFalse(TEXT("Restoration clears Histogram bins."), bool(Volume->Settings.bOverride_HistogramLogMin));
	TestFalse(TEXT("Restoration clears adaptive speed."), bool(Volume->Settings.bOverride_AutoExposureSpeedUp));
	TestTrue(TEXT("Authored content survives both policy transitions."), IsValid(Authored));
	return !HasAnyErrors();
}
#endif
