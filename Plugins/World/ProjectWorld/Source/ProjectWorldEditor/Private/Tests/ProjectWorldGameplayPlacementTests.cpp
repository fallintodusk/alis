// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldGameplayPlacement.h"

#include "ProjectWorldActor.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldRealizationProfile.h"
#include "ProjectWorldRealizationService.h"

#include "Editor.h"
#include "EngineUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ProjectWorldGameplayPlacementTests
{
	FString ProfilePath()
	{
		return FPaths::Combine(
			FPaths::ProjectPluginsDir(),
			TEXT("World/ProjectWorldTestData/Data/Profiles/Realization/synthetic_landscape_water_twin.realization.json"));
	}

	FProjectWorldCanonicalBundle MakeBundle()
	{
		FProjectWorldCanonicalBundle Bundle;
		Bundle.ProfileId = TEXT("synthetic_landscape_water_twin");
		Bundle.WorldDataPluginName = TEXT("ProjectWorldTestData");
		Bundle.GridId = TEXT("grid_24b9032e5f87005d");
		for (int32 CellIndex = 0; CellIndex < 2; ++CellIndex)
		{
			FProjectWorldCanonicalCell Cell;
			Cell.CellId = FString::Printf(TEXT("grid_24b9032e5f87005d:x%d:y0"), CellIndex);
			Cell.CellX = CellIndex;
			Cell.CellY = 0;
			Cell.Bounds = FVector4d(CellIndex * 63.0, 0.0, (CellIndex + 1) * 63.0, 63.0);
			Cell.Terrain.ArtifactHash = FString::ChrN(64, CellIndex == 0 ? TEXT('a') : TEXT('b'));
			Cell.Terrain.Bounds = Cell.Bounds;
			Cell.Terrain.SampleSpacing = FVector2D(63.0, 63.0);
			Cell.Terrain.SamplesX = 2;
			Cell.Terrain.SamplesY = 2;
			Cell.Terrain.HeightsMeters = {20.0, 21.0, 22.0, 23.0};
			Bundle.Cells.Add(MoveTemp(Cell));
		}
		return Bundle;
	}

	FProjectWorldLayerInventory GameplayInventory(const FProjectWorldRealizationProfile& Profile, TArray<FString> Dirty)
	{
		const FProjectWorldRealizationLayer* Layer = Profile.Layers.FindByPredicate([](const auto& Candidate)
		{
			return Candidate.LayerId == TEXT("gameplay");
		});
		FProjectWorldLayerInventory Inventory;
		Inventory.LayerId = TEXT("gameplay");
		Inventory.GeneratorId = TEXT("project_gameplay_placement");
		Inventory.GeneratorVersion = 1;
		Inventory.NormalizedLayerContractHash = Layer != nullptr ? Layer->ContractHash : FString();
		Inventory.FinalDirtyUnits = MoveTemp(Dirty);
		return Inventory;
	}

	AProjectWorldActor* FindActor(UWorld* World, const FString& ObjectId)
	{
		for (TActorIterator<AProjectWorldActor> It(World); It; ++It)
		{
			if (It->Tags.Contains(FName(*(TEXT("ProjectWorld.GameplayObject=") + ObjectId)))) return *It;
		}
		return nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldGameplayPlacementLifecycleTest,
	"Project.World.Realization.GameplayPlacement.Lifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldGameplayPlacementLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldGameplayPlacementTests;
	FProjectWorldRealizationProfile Profile;
	FString ErrorCode;
	FString Error;
	if (!ProjectWorldRealizationProfile::Load(ProfilePath(), Profile, ErrorCode, Error))
	{
		AddError(Error);
		return false;
	}
	const FProjectWorldCanonicalBundle Bundle = MakeBundle();
	UWorld* World = GEditor->NewMap(true);
	if (World == nullptr)
	{
		AddError(TEXT("Cannot create gameplay placement test world."));
		return false;
	}

	FProjectWorldRealizationResult First;
	First.LayerInventories.Add(GameplayInventory(Profile, {TEXT("*")}));
	TestTrue(TEXT("First Apply realizes the ObjectDefinition placement set."),
		ProjectWorldGameplayPlacement::Apply(World, Bundle, Profile, First, Error));
	TestEqual(TEXT("First Apply rewrites two placements."), First.GameplayPlacementRewriteCount, 2);
	AProjectWorldActor* FirstObject = FindActor(World, TEXT("synthetic_water_01"));
	AProjectWorldActor* StableSibling = FindActor(World, TEXT("synthetic_water_02"));
	TestNotNull(TEXT("First object exists."), FirstObject);
	TestNotNull(TEXT("Stable sibling exists."), StableSibling);
	const FGuid FirstActorGuid = FirstObject != nullptr ? FirstObject->GetActorGuid() : FGuid();
	const FGuid FirstDataId = FirstObject != nullptr ? FirstObject->DataId : FGuid();

	FProjectWorldRealizationResult NoOp;
	NoOp.LayerInventories.Add(GameplayInventory(Profile, {}));
	TestTrue(TEXT("Unchanged Apply is accepted."),
		ProjectWorldGameplayPlacement::Apply(World, Bundle, Profile, NoOp, Error));
	TestEqual(TEXT("Unchanged Apply rewrites no placement."), NoOp.GameplayPlacementRewriteCount, 0);

	FProjectWorldRealizationResult ObjectLocal;
	ObjectLocal.LayerInventories.Add(GameplayInventory(Profile, {TEXT("synthetic_water_01")}));
	TestTrue(TEXT("Object-ID dirty Apply is accepted."),
		ProjectWorldGameplayPlacement::Apply(World, Bundle, Profile, ObjectLocal, Error));
	TestEqual(TEXT("Only one selected placement is rewritten."), ObjectLocal.GameplayPlacementRewriteCount, 1);
	TestTrue(TEXT("The unselected sibling actor remains the same object."),
		FindActor(World, TEXT("synthetic_water_02")) == StableSibling);

	FProjectWorldRealizationProfile Changed = Profile;
	FProjectWorldRealizationLayer* ChangedLayer = Changed.Layers.FindByPredicate([](const auto& Candidate)
	{
		return Candidate.LayerId == TEXT("gameplay");
	});
	if (ChangedLayer == nullptr)
	{
		AddError(TEXT("Gameplay layer is missing."));
		return false;
	}
	ChangedLayer->NormalizedSettings =
		TEXT("{\"placement_source\":\"GameplayPlacement/synthetic_landscape_water_twin_changed.json\",")
		TEXT("\"runtime_state_policy\":\"external_to_generation\",\"surface_policy\":\"canonical_terrain_snap\"}");
	if (!ProjectWorldRealizationProfile::ValidateAndFinalize(Changed, Error))
	{
		AddError(Error);
		return false;
	}
	FProjectWorldRealizationResult ChangedResult;
	ChangedResult.LayerInventories.Add(GameplayInventory(Changed, {TEXT("*")}));
	TestTrue(TEXT("Move, definition change, addition, and removal are accepted together."),
		ProjectWorldGameplayPlacement::Apply(World, Bundle, Changed, ChangedResult, Error));
	TestNull(TEXT("Removed placement actor is retired."), FindActor(World, TEXT("synthetic_water_02")));
	AProjectWorldActor* ChangedFirst = FindActor(World, TEXT("synthetic_water_01"));
	TestNotNull(TEXT("Changed first placement remains present."), ChangedFirst);
	if (ChangedFirst != nullptr)
	{
		TestEqual(TEXT("Definition change is applied through ProjectObject."),
			ChangedFirst->ObjectDefinitionId.ToString(), FString(TEXT("ObjectDefinition:WaterBottleBig")));
		TestEqual(TEXT("Stable object identity survives definition changes."), ChangedFirst->DataId, FirstDataId);
	}
	TestNotNull(TEXT("Added placement actor is realized."), FindActor(World, TEXT("synthetic_water_03")));

	FProjectWorldRealizationResult Reconstructed;
	Reconstructed.LayerInventories.Add(GameplayInventory(Profile, {TEXT("*")}));
	TestTrue(TEXT("Original placement set reconstructs cleanly."),
		ProjectWorldGameplayPlacement::Apply(World, Bundle, Profile, Reconstructed, Error));
	AProjectWorldActor* RebuiltFirst = FindActor(World, TEXT("synthetic_water_01"));
	TestNotNull(TEXT("Reconstructed object exists."), RebuiltFirst);
	if (RebuiltFirst != nullptr)
	{
		TestEqual(TEXT("Clean reconstruction preserves actor GUID."), RebuiltFirst->GetActorGuid(), FirstActorGuid);
		TestEqual(TEXT("Clean reconstruction preserves DataId."), RebuiltFirst->DataId, FirstDataId);
	}
	return true;
}

#endif
