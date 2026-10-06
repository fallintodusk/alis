// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldRoadRealization.h"
#include "ProjectWorldVerifyTestFlow.h"

#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"

#if WITH_DEV_AUTOMATION_TESTS

// Fixture: a cross-cell and a single-cell primary road over a peaked cell and a flat cell, so the
// drape, the cell clipping, and the bound material are all observed.
namespace ProjectWorldRoadVerifyTest
{
	const ProjectWorldVerifyTestFlow::FSpec Spec{
		TEXT("Project.World.Realization.Verify.Road"), TEXT("Road"), TEXT("project_road_mesh"), 1};
	// Not the engine default surface material, so binding the default is an observable change.
	const TCHAR* RoadMaterial = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");

	FProjectWorldCanonicalCell MakeCell(int32 CellX, const FVector2D& Spacing, int32 SamplesX, const TArray<double>& Heights, TCHAR Hash)
	{
		FProjectWorldCanonicalCell Cell;
		Cell.CellId = FString::Printf(TEXT("cell_%d"), CellX);
		Cell.CellX = CellX;
		Cell.Bounds = FVector4d(CellX * 10.0, 0.0, (CellX + 1) * 10.0, 10.0);
		Cell.Terrain.ArtifactHash = FString::ChrN(64, Hash);
		Cell.Terrain.Bounds = Cell.Bounds;
		Cell.Terrain.SampleSpacing = Spacing;
		Cell.Terrain.SamplesX = SamplesX;
		Cell.Terrain.SamplesY = 2;
		Cell.Terrain.HeightsMeters = Heights;
		return Cell;
	}

	FProjectWorldCanonicalFeature MakeRoad(const FString& Id, double Width)
	{
		FProjectWorldCanonicalFeature Road;
		Road.FeatureId = Id;
		Road.FeatureClass = TEXT("road");
		Road.RoadClass = TEXT("primary");
		Road.WidthMeters = Width;
		Road.OwnerCellId = TEXT("cell_0");
		return Road;
	}

	void AddFragment(FProjectWorldCanonicalFeature& Road, const FString& CellId, const FVector2D& Start, const FVector2D& End)
	{
		FProjectWorldCanonicalRepresentation& Representation = Road.Representations.AddDefaulted_GetRef();
		Representation.CellId = CellId;
		Representation.Kind = TEXT("road_fragment");
		Representation.Parts.Add({Start, End});
		Road.GeometryParts.Add({Start, End});
	}

	FProjectWorldCanonicalBundle MakeBundle()
	{
		FProjectWorldCanonicalBundle Bundle;
		Bundle.GridId = TEXT("road_grid");
		Bundle.InputsHash = TEXT("road_input");
		Bundle.CoordinateQuantizationMeters = 0.01;
		Bundle.HeightQuantizationMeters = 0.1;
		Bundle.Cells.Add(MakeCell(0, FVector2D(5.0, 10.0), 3, {0.0, 10.0, 0.0, 0.0, 10.0, 0.0}, TEXT('a')));
		Bundle.Cells.Add(MakeCell(1, FVector2D(10.0, 10.0), 2, {0.0, 0.0, 0.0, 0.0}, TEXT('b')));
		FProjectWorldCanonicalFeature CrossCell = MakeRoad(TEXT("alis:test:road:cross-cell"), 4.0);
		AddFragment(CrossCell, TEXT("cell_0"), FVector2D(5.0, 5.0), FVector2D(10.0, 5.0));
		AddFragment(CrossCell, TEXT("cell_1"), FVector2D(10.0, 5.0), FVector2D(15.0, 5.0));
		FProjectWorldCanonicalFeature SingleCell = MakeRoad(TEXT("alis:test:road:single-cell"), 3.0);
		AddFragment(SingleCell, TEXT("cell_0"), FVector2D(1.0, 2.0), FVector2D(4.0, 2.0));
		Bundle.Cells[0].OwnedFeatureIds = {CrossCell.FeatureId, SingleCell.FeatureId};
		Bundle.Cells[1].ReferencedFeatureIds = {CrossCell.FeatureId};
		Bundle.Features.Add(CrossCell.FeatureId, MoveTemp(CrossCell));
		Bundle.Features.Add(SingleCell.FeatureId, MoveTemp(SingleCell));
		return Bundle;
	}

	FProjectWorldRealizationProfile MakeProfile()
	{
		FProjectWorldRealizationLayer Layer;
		Layer.LayerId = TEXT("roads");
		Layer.LayerKind = EProjectWorldLayerKind::GeneratedGeography;
		Layer.GeneratorId = TEXT("project_road_mesh");
		Layer.GeneratorVersion = 1;
		Layer.DependsOn = {TEXT("terrain")};
		Layer.CanonicalSelectors = {TEXT("roads")};
		Layer.ArtifactRoot = Spec.ArtifactRoot();
		Layer.SpatialOwnership = TEXT("cell_local");
		Layer.DirtyGranularity = EProjectWorldDirtyGranularity::CanonicalCell;
		Layer.DependencyHaloCells = 1;
		Layer.RuntimeMapping = TEXT("world_partition_spatial");
		Layer.NormalizedSettings =
			TEXT("{\"collision\":\"complex_as_simple\",\"intersection_policy\":\"overlap_same_owner\",")
			TEXT("\"maximum_segment_length_m\":7.5,\"nanite\":true,\"selected_classes\":[\"primary\",\"primary_link\",")
			TEXT("\"secondary\",\"secondary_link\",\"tertiary\",\"tertiary_link\",\"residential\",\"unclassified\",")
			TEXT("\"living_street\"],\"structure_fallback\":\"terrain_drape\",\"surface_offset_m\":0.15}");
		FProjectWorldRealizationProfile Profile;
		Profile.ProfileId = TEXT("verify_road");
		Profile.Layers.Add(ProjectWorldVerifyTestFlow::Finalize(MoveTemp(Layer)));
		return Profile;
	}

	FString BoundMaterial(UWorld* World)
	{
		for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
		{
			FString CellId;
			FString Semantic;
			if (!ProjectWorldRoadRealization::ReadActorIdentity(*It, CellId, Semantic))
			{
				continue;
			}
			const UStaticMesh* Mesh = It->GetStaticMeshComponent()->GetStaticMesh();
			if (Mesh != nullptr && Mesh->GetStaticMaterials().Num() > 0 &&
				Mesh->GetStaticMaterials()[0].MaterialInterface != nullptr)
			{
				return Mesh->GetStaticMaterials()[0].MaterialInterface->GetPathName();
			}
		}
		return FString();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldRoadVerifyTest,
	"Project.World.Realization.Verify.Road",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldRoadVerifyTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldRoadVerifyTest;
	const FProjectWorldCanonicalBundle Bundle = MakeBundle();
	const FProjectWorldRealizationProfile Profile = MakeProfile();
	return ProjectWorldVerifyTestFlow::Run(*this, Spec,
		[&](FProjectWorldVerifyIdentity& Identity, FString& OutError)
		{
			Identity.AddFixtureBundle(TEXT("memory:bundle"), Bundle);
			Identity.AddFixtureText(TEXT("memory:profile"), ProjectWorldVerifyTestFlow::SerializeProfile(Profile));
			return Identity.AddFixtureObject(RoadMaterial, OutError);
		},
		[&](UWorld* World, FString& OutError)
		{
			UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, RoadMaterial);
			FProjectWorldRealizationResult Result;
			Result.LayerInventories.Add(ProjectWorldVerifyTestFlow::WholeLayer(Profile.Layers[0]));
			return ProjectWorldRoadRealization::Apply(World, Bundle, Profile, Material, Result, OutError);
		});
}

REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FProjectWorldRoadVerifyTest,
	"Project.World.Realization.Verify.Road",
	"[Slow][Integration][World]")

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldRoadMaterialIdentityTest,
	"Project.World.Realization.Identity.RoadMaterialChangeDirtiesRoads",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldRoadMaterialIdentityTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldRoadVerifyTest;
	const FProjectWorldCanonicalBundle Bundle = MakeBundle();
	const FProjectWorldRealizationProfile Profile = MakeProfile();
	const TCHAR* Replacement = TEXT("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial");
	UMaterialInterface* FirstMaterial = LoadObject<UMaterialInterface>(nullptr, RoadMaterial);
	UMaterialInterface* SecondMaterial = LoadObject<UMaterialInterface>(nullptr, Replacement);
	FProjectWorldVerifyWorld World(Spec.Folder);
	FString Error;
	if (!TestTrue(TEXT("Road identity world and materials are available."),
		World.IsReady(Error) && FirstMaterial != nullptr && SecondMaterial != nullptr))
	{
		return false;
	}
	FProjectWorldRealizationResult First;
	First.LayerInventories.Add(ProjectWorldVerifyTestFlow::WholeLayer(Profile.Layers[0]));
	if (!TestTrue(TEXT("Initial road material is applied and saved."),
		ProjectWorldRoadRealization::Apply(World.GetWorld(), Bundle, Profile, FirstMaterial, First, Error) &&
		World.SaveOwned(Error) && World.Reload(Error)))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("Initial road mesh binds the first material."), BoundMaterial(World.GetWorld()), FString(RoadMaterial));
	FProjectWorldRealizationResult Second;
	FProjectWorldLayerInventory& Inventory = Second.LayerInventories.Add_GetRef(
		ProjectWorldVerifyTestFlow::WholeLayer(Profile.Layers[0]));
	Inventory.FinalDirtyUnits.Reset();
	if (!TestTrue(TEXT("Presentation-only road update is applied and reloaded."),
		ProjectWorldRoadRealization::Apply(World.GetWorld(), Bundle, Profile, SecondMaterial, Second, Error) &&
		World.SaveOwned(Error) && World.Reload(Error)))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("Changed presentation material reaches the saved road mesh."),
		BoundMaterial(World.GetWorld()), FString(Replacement));
	return true;
}

REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FProjectWorldRoadMaterialIdentityTest,
	"Project.World.Realization.Identity.RoadMaterialChangeDirtiesRoads",
	"[Slow][Integration][World]")

#endif
