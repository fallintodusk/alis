// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Experience/ProjectExperienceDefinition.h"
#include "Experience/ProjectExperienceDefinitionDescriptor.h"
#include "Experience/ProjectExperienceDefinitionRegistrar.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Types/ProjectLoadRequest.h"

#if WITH_DEV_AUTOMATION_TESTS

PROJECTLOADING_API bool BuildLoadRequestByName_ForTests(
	FName ExperienceName,
	FLoadRequest& OutRequest,
	FText& OutError);

PROJECTLOADING_API bool BuildLoadRequestWithFreshRegistry_ForTests(
	FName ExperienceName,
	FLoadRequest& OutRequest,
	FText& OutError,
	bool& bOutInitialLookupMissed);

namespace
{
	UProjectExperienceDefinition* MakeDefinition(
		const TCHAR* Id,
		const TCHAR* MapPath,
		const TCHAR* Traversal,
		const TCHAR* ScanDir)
	{
		UProjectExperienceDefinition* Definition = NewObject<UProjectExperienceDefinition>();
		Definition->ExperienceName = Id;
		Definition->Map = TSoftObjectPtr<UWorld>(FSoftObjectPath(MapPath));
		Definition->TraversalMode = Traversal;
		Definition->AssetScanDirectory = ScanDir;
		return Definition;
	}
}

/**
 * A configured experience must resolve through the one generic descriptor, so a new
 * territory is data only. This deliberately uses a synthetic third experience: if it needs
 * a C++ change to work, the generic contract has regressed.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectLoading_ExperienceDefinitionProjection,
	"ProjectLoading.ExperienceDefinition.Generic.Projection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectLoading_ExperienceDefinitionProjection::RunTest(const FString& Parameters)
{
	const FString MapPath(TEXT("/ProjectWorldData/Generated/Showcase/Synthetic/L_Synthetic.L_Synthetic"));
	UProjectExperienceDefinition* Definition = MakeDefinition(
		TEXT("SyntheticShowcase"), *MapPath, TEXT("PreviewFlight"),
		TEXT("/ProjectWorldData/Generated/Showcase/Synthetic"));

	UProjectExperienceDefinitionDescriptor* Descriptor = NewObject<UProjectExperienceDefinitionDescriptor>();
	TestTrue(TEXT("A well-formed configured experience initializes."),
		Descriptor->InitializeFromDefinition(*Definition));

	FLoadRequest Request;
	Descriptor->BuildLoadRequest(Request);
	TestEqual(TEXT("Identity comes from data."), Request.ExperienceName, FName(TEXT("SyntheticShowcase")));
	TestEqual(TEXT("The map comes from data."), Request.MapSoftPath.ToString(), MapPath);
	TestEqual(TEXT("Exactly one experience option is projected."), Request.CustomOptions.Num(), 1);
	TestEqual(TEXT("The traversal token is forwarded verbatim."),
		Request.CustomOptions.FindRef(TEXT("Traversal")), FString(TEXT("PreviewFlight")));
	TestFalse(TEXT("A showcase selects no survival scenario."), Request.CustomOptions.Contains(TEXT("Scenario")));

	TArray<FExperienceAssetScanSpec> Specs;
	Descriptor->GetAssetScanSpecs(Specs);
	TestEqual(TEXT("One map scan spec is projected."), Specs.Num(), 1);
	if (Specs.Num() == 1)
	{
		TestEqual(TEXT("The scan targets maps."), Specs[0].PrimaryAssetType, FName(TEXT("Map")));
		TestTrue(TEXT("The scan targets the configured directory."),
			Specs[0].Directories.Contains(TEXT("/ProjectWorldData/Generated/Showcase/Synthetic")));
	}

	return true;
}

/** Malformed and optional-field cases must fail closed rather than register something unusable. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectLoading_ExperienceDefinitionFailsClosed,
	"ProjectLoading.ExperienceDefinition.Generic.FailsClosed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectLoading_ExperienceDefinitionFailsClosed::RunTest(const FString& Parameters)
{
	UProjectExperienceDefinitionDescriptor* Descriptor = NewObject<UProjectExperienceDefinitionDescriptor>();

	UProjectExperienceDefinition* NoId = MakeDefinition(
		TEXT(""), TEXT("/Game/Maps/L_X.L_X"), TEXT(""), TEXT(""));
	NoId->ExperienceName = NAME_None;
	TestFalse(TEXT("A definition without an identity is rejected."),
		Descriptor->InitializeFromDefinition(*NoId));

	UProjectExperienceDefinition* NoMap = NewObject<UProjectExperienceDefinition>();
	NoMap->ExperienceName = TEXT("NoMap");
	TestFalse(TEXT("A definition without a map is rejected."),
		Descriptor->InitializeFromDefinition(*NoMap));

	// Optional fields stay optional: no traversal token and no scan directory are valid.
	UProjectExperienceDefinition* Minimal = MakeDefinition(
		TEXT("Minimal"), TEXT("/Game/Maps/L_Min.L_Min"), TEXT(""), TEXT(""));
	TestTrue(TEXT("A minimal configured experience is accepted."),
		Descriptor->InitializeFromDefinition(*Minimal));

	FLoadRequest Request;
	Descriptor->BuildLoadRequest(Request);
	TestFalse(TEXT("An empty traversal token projects no option."),
		Request.CustomOptions.Contains(TEXT("Traversal")));

	TArray<FExperienceAssetScanSpec> Specs;
	Descriptor->GetAssetScanSpecs(Specs);
	TestEqual(TEXT("An empty scan directory projects no spec."), Specs.Num(), 0);

	return true;
}

/**
 * Ownership boundaries this refactor exists to hold. These are cheap file assertions, but
 * they are the only automated guard that the coupling does not silently come back.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectLoading_ExperienceOwnershipBoundaries,
	"ProjectLoading.ExperienceDefinition.Ownership.Boundaries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectLoading_ExperienceOwnershipBoundaries::RunTest(const FString& Parameters)
{
	// The application module must carry no concrete experience knowledge.
	FString GameModule;
	const FString GameModulePath = FPaths::Combine(
		FPaths::ProjectDir(), TEXT("Source/Alis/Private/Alis.cpp"));
	TestTrue(TEXT("The application module source is readable."),
		FFileHelper::LoadFileToString(GameModule, *GameModulePath));
	TestFalse(TEXT("The application module registers no experience descriptor."),
		GameModule.Contains(TEXT("RegisterDescriptor")));
	TestFalse(TEXT("The application module names no territory."),
		GameModule.Contains(TEXT("Kazan")) || GameModule.Contains(TEXT("Manhattan")));

	// The concrete experience data owner stays content/data-only.
	FString ExperienceDataPlugin;
	const FString ExperienceDataPluginPath = FPaths::Combine(
		FPaths::ProjectPluginsDir(),
		TEXT("Resources/ProjectExperienceData/ProjectExperienceData.uplugin"));
	TestTrue(TEXT("The experience data plugin descriptor is readable."),
		FFileHelper::LoadFileToString(ExperienceDataPlugin, *ExperienceDataPluginPath));
	TestTrue(TEXT("The experience data owner has no source module."),
		ExperienceDataPlugin.Contains(TEXT("\"Modules\": []")));

	// Geography stays geography: WorldData owns no experience or traversal policy.
	FString WorldDataPlugin;
	const FString WorldDataPluginPath = FPaths::Combine(
		FPaths::ProjectPluginsDir(), TEXT("World/ProjectWorldData/ProjectWorldData.uplugin"));
	TestTrue(TEXT("The world data plugin descriptor is readable."),
		FFileHelper::LoadFileToString(WorldDataPlugin, *WorldDataPluginPath));
	TestTrue(TEXT("The world data owner still has no source module."),
		WorldDataPlugin.Contains(TEXT("\"Modules\": []")));

	// Configured records are text-first, and every one is generated from JSON.
	const FString ExperiencesDir = FPaths::Combine(
		FPaths::ProjectPluginsDir(), TEXT("Resources/ProjectExperienceData/Data/Experiences"));
	TArray<FString> RecordFiles;
	IFileManager::Get().FindFiles(RecordFiles, *(ExperiencesDir / TEXT("*.json")), true, false);
	TestTrue(TEXT("Configured experiences are authored as JSON source of truth."), RecordFiles.Num() >= 2);

	return true;
}

/**
 * The production route: resolve by name through the real loader, with no manual call to
 * EnsureRegistered. Passing this proves configured definitions are actually discovered
 * through the AssetManager on a descriptor-lookup miss and that the descriptor's own map
 * scan runs afterwards - the integration seam synthetic projection tests cannot cover.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectLoading_ExperienceDefinitionProductionRoute,
	"ProjectLoading.ExperienceDefinition.Production.FirstMissRoute",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectLoading_ExperienceDefinitionProductionRoute::RunTest(const FString& Parameters)
{
	// The unknown-id probe below deliberately drives the loader's failure path, which logs an
	// error; declare it so the harness does not read the intended refusal as a test failure.
	AddExpectedError(TEXT("Descriptor not found for experience 'NoSuchConfiguredExperience'"),
		EAutomationExpectedErrorFlags::Contains, 1);

	// Force discovery to re-run so this is the miss path, not a cached earlier result.
	ProjectExperienceDefinitions::ResetForTests();

	// A fresh registry guarantees the initial lookup misses. With the shared process registry an
	// earlier case may already have registered Manhattan, which would resolve immediately and
	// silently skip the discovery seam this test exists to pin.
	FLoadRequest Request;
	FText Error;
	bool bInitialLookupMissed = false;
	const bool bResolved = BuildLoadRequestWithFreshRegistry_ForTests(
		TEXT("ManhattanShowcase"), Request, Error, bInitialLookupMissed);
	TestTrue(TEXT("The initial descriptor lookup genuinely missed."), bInitialLookupMissed);
	TestTrue(FString::Printf(TEXT("The real loader resolves a configured experience by name: %s"),
		*Error.ToString()), bResolved);
	if (!bResolved)
	{
		return false;
	}

	TestEqual(TEXT("The production route resolves the configured identity."),
		Request.ExperienceName, FName(TEXT("ManhattanShowcase")));
	TestEqual(TEXT("The production route resolves the configured map."),
		Request.MapSoftPath.ToString(),
		FString(TEXT("/ProjectWorldData/Generated/Showcase/Manhattan/"
			"L_ProjectWorldManhattanShowcase.L_ProjectWorldManhattanShowcase")));
	TestEqual(TEXT("The production route projects the configured traversal."),
		Request.CustomOptions.FindRef(TEXT("Traversal")), FString(TEXT("PreviewFlight")));

	// Kazan resolves through the same generic path, differing only by configuration.
	FLoadRequest KazanRequest;
	FText KazanError;
	bool bKazanMissed = false;
	const bool bKazanResolved = BuildLoadRequestWithFreshRegistry_ForTests(
		TEXT("KazanTerritory"), KazanRequest, KazanError, bKazanMissed);
	TestTrue(TEXT("The other configured experience also starts from a genuine miss."), bKazanMissed);
	TestTrue(TEXT("The same generic route resolves the other configured experience."), bKazanResolved);
	if (bKazanResolved)
	{
		TestEqual(TEXT("Configured experiences differ only by data."),
			KazanRequest.MapSoftPath.ToString(),
			FString(TEXT("/ProjectWorldData/Generated/Territory/"
				"L_ProjectWorldKazanTerritory.L_ProjectWorldKazanTerritory")));
	}

	// An unknown id must fail closed rather than resolve to something arbitrary.
	FLoadRequest UnknownRequest;
	FText UnknownError;
	bool bUnknownMissed = false;
	TestFalse(TEXT("An unconfigured experience id fails closed."),
		BuildLoadRequestWithFreshRegistry_ForTests(
			TEXT("NoSuchConfiguredExperience"), UnknownRequest, UnknownError, bUnknownMissed));

	return true;
}

#endif
