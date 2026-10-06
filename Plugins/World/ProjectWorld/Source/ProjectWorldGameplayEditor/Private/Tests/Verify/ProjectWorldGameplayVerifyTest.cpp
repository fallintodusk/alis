// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectServiceLocator.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldGameplayPlacement.h"
#include "ProjectWorldVerifyTestFlow.h"
#include "Services/IObjectSpawnService.h"

#if WITH_DEV_AUTOMATION_TESTS

// Fixture: the tracked twin placement document on two in-code cells; each placed definition's
// provider identity joins the verification identity.
namespace ProjectWorldGameplayVerifyTest
{
	const ProjectWorldVerifyTestFlow::FSpec Spec{
		TEXT("Project.World.Realization.Verify.Gameplay"), TEXT("Gameplay"), TEXT("project_gameplay_placement"), 1};
	const TCHAR* PlacementSource = TEXT("GameplayPlacement/synthetic_territory_twin.json");
	const TCHAR* PlacementFile = TEXT("Plugins/World/ProjectWorldTestData/Data/GameplayPlacement/synthetic_territory_twin.json");

	FProjectWorldCanonicalBundle MakeBundle()
	{
		FProjectWorldCanonicalBundle Bundle;
		Bundle.ProfileId = TEXT("synthetic_territory_twin");
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

	FProjectWorldRealizationProfile MakeProfile()
	{
		FProjectWorldRealizationLayer Layer;
		Layer.LayerId = TEXT("gameplay");
		Layer.LayerKind = EProjectWorldLayerKind::GeneratedGameplayPlacement;
		Layer.GeneratorId = TEXT("project_gameplay_placement");
		Layer.GeneratorVersion = 1;
		Layer.DependsOn = {TEXT("terrain")};
		Layer.CanonicalSelectors = {TEXT("gameplay_placements")};
		Layer.ArtifactRoot = Spec.ArtifactRoot();
		Layer.SpatialOwnership = TEXT("object_local");
		Layer.DirtyGranularity = EProjectWorldDirtyGranularity::ObjectId;
		Layer.RuntimeMapping = TEXT("world_partition_spatial");
		Layer.NormalizedSettings = FString::Printf(
			TEXT("{\"placement_source\":\"%s\",\"runtime_state_policy\":\"external_to_generation\",")
			TEXT("\"surface_policy\":\"canonical_terrain_snap\"}"), PlacementSource);
		FProjectWorldRealizationProfile Profile;
		Profile.ProfileId = TEXT("verify_gameplay");
		Profile.WorldDataPluginName = TEXT("ProjectWorldTestData");
		Profile.CanonicalProfileId = TEXT("synthetic_territory_twin");
		Profile.Layers.Add(ProjectWorldVerifyTestFlow::Finalize(MoveTemp(Layer)));
		return Profile;
	}

	bool AddDefinitions(
		const FProjectWorldRealizationProfile& Profile,
		FProjectWorldVerifyIdentity& Identity,
		FString& OutError)
	{
		FProjectWorldGameplayPlacementSet Set;
		TSharedPtr<IObjectSpawnService> SpawnService = FProjectServiceLocator::Resolve<IObjectSpawnService>();
		if (!ProjectWorldGameplayPlacement::Load(Profile, Profile.Layers[0], Set, OutError) || !SpawnService.IsValid())
		{
			OutError = OutError.IsEmpty() ? TEXT("ObjectDefinition spawn provider is unavailable.") : OutError;
			return false;
		}
		TArray<FString> Definitions;
		for (const FProjectWorldGameplayPlacement& Placement : Set.Placements)
		{
			Definitions.AddUnique(Placement.DefinitionId.ToString());
		}
		Definitions.Sort();
		for (const FString& Definition : Definitions)
		{
			FString DefinitionIdentity;
			FText ProviderError;
			if (!SpawnService->GetDefinitionIdentity(FPrimaryAssetId::FromString(Definition), DefinitionIdentity, &ProviderError))
			{
				OutError = ProviderError.ToString();
				return false;
			}
			Identity.AddFixtureText(TEXT("definition:") + Definition, DefinitionIdentity);
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldGameplayVerifyTest,
	"Project.World.Realization.Verify.Gameplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldGameplayVerifyTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldGameplayVerifyTest;
	const FProjectWorldCanonicalBundle Bundle = MakeBundle();
	const FProjectWorldRealizationProfile Profile = MakeProfile();
	return ProjectWorldVerifyTestFlow::Run(*this, Spec,
		[&](FProjectWorldVerifyIdentity& Identity, FString& OutError)
		{
			Identity.AddFixtureBundle(TEXT("memory:bundle"), Bundle);
			Identity.AddFixtureText(TEXT("memory:profile"), ProjectWorldVerifyTestFlow::SerializeProfile(Profile));
			return Identity.AddFixtureFile(PlacementFile, OutError) && AddDefinitions(Profile, Identity, OutError);
		},
		[&](UWorld* World, FString& OutError)
		{
			FProjectWorldRealizationResult Result;
			Result.LayerInventories.Add(ProjectWorldVerifyTestFlow::WholeLayer(Profile.Layers[0]));
			return ProjectWorldGameplayPlacement::Apply(World, Bundle, Profile, Result, OutError);
		});
}

REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FProjectWorldGameplayVerifyTest,
	"Project.World.Realization.Verify.Gameplay",
	"[Slow][Integration][World]")

#endif
