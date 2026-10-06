// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldAuthoredOverlay.h"
#include "ProjectWorldBuildingRealization.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldVerify.h"
#include "ProjectWorldVerifyTestFlow.h"

#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"

#if WITH_DEV_AUTOMATION_TESTS

// Fixture: a logical v2 building with overlapping raised and grounded volumes in one cell and an
// outline building with one volume in the next cell.
namespace ProjectWorldBuildingMassingVerifyTest
{
	const ProjectWorldVerifyTestFlow::FSpec Spec{
		TEXT("Project.World.Realization.Verify.BuildingMassing"), TEXT("Building"), TEXT("project_building_massing"), 2};
	// Not the engine default surface material, so binding the default is an observable change.
	const TCHAR* BuildingMaterial = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");

	FProjectWorldCanonicalPolygon Square(double MinimumX, double MinimumY, double MaximumX, double MaximumY)
	{
		FProjectWorldCanonicalPolygon Polygon;
		Polygon.Outer = {
			FVector2D(MinimumX, MinimumY), FVector2D(MaximumX, MinimumY),
			FVector2D(MaximumX, MaximumY), FVector2D(MinimumX, MaximumY),
			FVector2D(MinimumX, MinimumY)};
		return Polygon;
	}

	FProjectWorldCanonicalBuildingVolume Volume(
		const FString& Id, const FString& SourceId, const FProjectWorldCanonicalPolygon& Polygon, double Minimum, double Height)
	{
		FProjectWorldCanonicalBuildingVolume Result;
		Result.VolumeId = Id;
		Result.SourceFeatureId = SourceId;
		Result.GeometryType = TEXT("Polygon");
		Result.GeometryPolygons.Add(Polygon);
		Result.MinHeightMeters = Minimum;
		Result.HeightMeters = Height;
		return Result;
	}

	FProjectWorldCanonicalFeature Building(
		const FString& Id, const FString& OwnerCellId, const FProjectWorldCanonicalPolygon& Polygon, double Height)
	{
		FProjectWorldCanonicalFeature Feature;
		Feature.FeatureId = Id;
		Feature.FeatureClass = TEXT("building");
		Feature.OwnerCellId = OwnerCellId;
		Feature.GeometryType = TEXT("Polygon");
		Feature.GeometryPolygons.Add(Polygon);
		Feature.HeightMeters = Height;
		return Feature;
	}

	FProjectWorldCanonicalCell Cell(const FString& Id, double MinimumX, double MaximumX, TCHAR Hash)
	{
		FProjectWorldCanonicalCell Result;
		Result.CellId = Id;
		Result.Bounds = FVector4d(MinimumX, 0.0, MaximumX, 100.0);
		Result.Terrain.ArtifactHash = FString::ChrN(64, Hash);
		Result.Terrain.Bounds = Result.Bounds;
		Result.Terrain.SampleSpacing = FVector2D(100.0, 100.0);
		Result.Terrain.SamplesX = 2;
		Result.Terrain.SamplesY = 2;
		Result.Terrain.HeightsMeters = {2.0, 2.0, 2.0, 2.0};
		return Result;
	}

	FProjectWorldCanonicalBundle MakeBundle()
	{
		FProjectWorldCanonicalBundle Bundle;
		Bundle.GridId = TEXT("building_verify_test");
		Bundle.CoordinateQuantizationMeters = 0.01;
		Bundle.HeightQuantizationMeters = 0.1;
		Bundle.Cells.Add(Cell(TEXT("building_verify_test:a"), 0.0, 100.0, TEXT('a')));
		Bundle.Cells.Add(Cell(TEXT("building_verify_test:b"), 100.0, 200.0, TEXT('b')));
		FProjectWorldCanonicalFeature Logical = Building(
			TEXT("building/logical"), Bundle.Cells[0].CellId, Square(0.0, 0.0, 20.0, 20.0), 20.0);
		Logical.BuildingVolumes = {
			Volume(TEXT("volume/lower"), TEXT("way/2"), Square(0.0, 0.0, 10.0, 20.0), 0.0, 10.0),
			Volume(TEXT("volume/raised"), TEXT("way/3"), Square(5.0, 0.0, 20.0, 20.0), 10.0, 20.0)};
		FProjectWorldCanonicalFeature Outline = Building(
			TEXT("building/persistent"), Bundle.Cells[1].CellId, Square(110.0, 10.0, 140.0, 40.0), 12.0);
		Outline.BuildingVolumes = {
			Volume(TEXT("volume/persistent"), TEXT("way/4"), Square(110.0, 10.0, 140.0, 40.0), 0.0, 12.0)};
		Bundle.Cells[0].OwnedFeatureIds = {Logical.FeatureId};
		Bundle.Cells[1].OwnedFeatureIds = {Outline.FeatureId};
		Bundle.Features.Add(Logical.FeatureId, MoveTemp(Logical));
		Bundle.Features.Add(Outline.FeatureId, MoveTemp(Outline));
		return Bundle;
	}

	FProjectWorldRealizationProfile MakeProfile()
	{
		FProjectWorldRealizationLayer Layer;
		Layer.LayerId = TEXT("buildings");
		Layer.LayerKind = EProjectWorldLayerKind::GeneratedGeography;
		Layer.GeneratorId = TEXT("project_building_massing");
		Layer.GeneratorVersion = 2;
		Layer.DependsOn = {TEXT("terrain")};
		Layer.CanonicalSelectors = {TEXT("buildings")};
		Layer.ArtifactRoot = Spec.ArtifactRoot();
		Layer.SpatialOwnership = TEXT("cell_local");
		Layer.DirtyGranularity = EProjectWorldDirtyGranularity::CanonicalCell;
		Layer.RuntimeMapping = TEXT("world_partition_spatial");
		Layer.NormalizedSettings =
			TEXT("{\"collision\":\"complex_as_simple\",\"conflict_policy\":\"reject_affected_fragments\",")
			TEXT("\"contained_policy\":\"associate_with_container\",\"duplicate_policy\":\"stable_feature_id\",")
			TEXT("\"maximum_height_m\":300,\"nanite\":true,\"navigation\":\"no_navigation\",")
			TEXT("\"terrain_anchor_policy\":\"owner_cell_clamped_bounds_center\",")
			TEXT("\"topology_policy\":\"logical_building_classify_v2\"}");
		FProjectWorldRealizationProfile Profile;
		Profile.ProfileId = TEXT("verify_buildings");
		Profile.Layers.Add(ProjectWorldVerifyTestFlow::Finalize(MoveTemp(Layer)));
		return Profile;
	}

	FString BoundMaterial(UWorld* World)
	{
		for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
		{
			FString CellId;
			FString Semantic;
			if (!ProjectWorldBuildingRealization::ReadActorIdentity(*It, CellId, Semantic))
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
	FProjectWorldBuildingMassingVerifyTest,
	"Project.World.Realization.Verify.BuildingMassing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldBuildingMassingVerifyTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldBuildingMassingVerifyTest;
	const FProjectWorldCanonicalBundle Bundle = MakeBundle();
	const FProjectWorldRealizationProfile Profile = MakeProfile();
	const FProjectWorldAuthoredOverlaySet Overlays;
	return ProjectWorldVerifyTestFlow::Run(*this, Spec,
		[&](FProjectWorldVerifyIdentity& Identity, FString& OutError)
		{
			Identity.AddFixtureBundle(TEXT("memory:bundle"), Bundle);
			Identity.AddFixtureText(TEXT("memory:profile"), ProjectWorldVerifyTestFlow::SerializeProfile(Profile));
			return Identity.AddFixtureObject(BuildingMaterial, OutError);
		},
		[&](UWorld* World, FString& OutError)
		{
			UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, BuildingMaterial);
			FProjectWorldRealizationResult Result;
			Result.LayerInventories.Add(ProjectWorldVerifyTestFlow::WholeLayer(Profile.Layers[0]));
			return ProjectWorldBuildingRealization::Apply(World, Bundle, Profile, Overlays, Material, Result, OutError);
		});
}

REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FProjectWorldBuildingMassingVerifyTest,
	"Project.World.Realization.Verify.BuildingMassing",
	"[Slow][Integration][World]")

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldBuildingMaterialIdentityTest,
	"Project.World.Realization.Identity.BuildingMaterialChangeDirtiesBuildings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldBuildingMaterialIdentityTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldBuildingMassingVerifyTest;
	const FProjectWorldCanonicalBundle Bundle = MakeBundle();
	const FProjectWorldRealizationProfile Profile = MakeProfile();
	const FProjectWorldAuthoredOverlaySet Overlays;
	const TCHAR* Replacement = TEXT("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial");
	UMaterialInterface* FirstMaterial = LoadObject<UMaterialInterface>(nullptr, BuildingMaterial);
	UMaterialInterface* SecondMaterial = LoadObject<UMaterialInterface>(nullptr, Replacement);
	FProjectWorldVerifyWorld World(Spec.Folder);
	FString Error;
	if (!TestTrue(TEXT("Building identity world and materials are available."),
		World.IsReady(Error) && FirstMaterial != nullptr && SecondMaterial != nullptr))
	{
		return false;
	}
	FProjectWorldRealizationResult First;
	First.LayerInventories.Add(ProjectWorldVerifyTestFlow::WholeLayer(Profile.Layers[0]));
	if (!TestTrue(TEXT("Initial building material is applied and saved."),
		ProjectWorldBuildingRealization::Apply(World.GetWorld(), Bundle, Profile, Overlays,
			FirstMaterial, First, Error) && World.SaveOwned(Error) && World.Reload(Error)))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("Initial building mesh binds the first material."),
		BoundMaterial(World.GetWorld()), FString(BuildingMaterial));
	FProjectWorldRealizationResult Second;
	FProjectWorldLayerInventory& Inventory = Second.LayerInventories.Add_GetRef(
		ProjectWorldVerifyTestFlow::WholeLayer(Profile.Layers[0]));
	Inventory.FinalDirtyUnits.Reset();
	if (!TestTrue(TEXT("Presentation-only building update is applied and reloaded."),
		ProjectWorldBuildingRealization::Apply(World.GetWorld(), Bundle, Profile, Overlays,
			SecondMaterial, Second, Error) && World.SaveOwned(Error) && World.Reload(Error)))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("Changed presentation material reaches the saved building mesh."),
		BoundMaterial(World.GetWorld()), FString(Replacement));
	return true;
}

REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FProjectWorldBuildingMaterialIdentityTest,
	"Project.World.Realization.Identity.BuildingMaterialChangeDirtiesBuildings",
	"[Slow][Integration][World]")

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldBuildingRetiredIdentityProjectionTest,
	"Project.World.Realization.Verify.BuildingRetiredIdentityProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldBuildingRetiredIdentityProjectionTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldBuildingMassingVerifyTest;
	const FProjectWorldCanonicalBundle Bundle = MakeBundle();
	const FProjectWorldRealizationProfile Profile = MakeProfile();
	const FProjectWorldAuthoredOverlaySet Overlays;
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, BuildingMaterial);
	FProjectWorldVerifyWorld World(Spec.Folder);
	FString Error;
	if (!TestTrue(TEXT("Building projection fixture is available."), World.IsReady(Error) && Material != nullptr))
	{
		return false;
	}
	FProjectWorldRealizationResult Result;
	Result.LayerInventories.Add(ProjectWorldVerifyTestFlow::WholeLayer(Profile.Layers[0]));
	if (!TestTrue(TEXT("Building output is persisted."),
		ProjectWorldBuildingRealization::Apply(World.GetWorld(), Bundle, Profile, Overlays,
			Material, Result, Error) && World.SaveOwned(Error) && World.Reload(Error)))
	{
		AddError(Error);
		return false;
	}
	AStaticMeshActor* Retired = nullptr;
	for (TActorIterator<AStaticMeshActor> It(World.GetWorld()); It; ++It)
	{
		FString CellId;
		FString Semantic;
		if (ProjectWorldBuildingRealization::ReadActorIdentity(*It, CellId, Semantic))
		{
			Retired = *It;
			break;
		}
	}
	if (!TestNotNull(TEXT("A saved building actor exists."), Retired))
	{
		return false;
	}
	Retired->Tags.Remove(FName(TEXT("ProjectWorld.BuildingMassing.v2")));
	Retired->Tags.Add(FName(TEXT("ProjectWorld.BuildingMassing.v1")));
	Retired->MarkPackageDirty();
	if (!TestTrue(TEXT("Retired actor identity survives reload."), World.SaveOwned(Error) && World.Reload(Error)))
	{
		AddError(Error);
		return false;
	}
	int32 ComparisonVersion = 0;
	FProjectWorldProjectFn Project;
	if (!TestTrue(TEXT("Building projection is registered."),
		ProjectWorldVerify::FindProjection(TEXT("project_building_massing"), 2, ComparisonVersion, Project)))
	{
		return false;
	}
	FProjectWorldProjection Projection;
	if (!TestTrue(TEXT("Retired actor is projected from saved output."),
		Project({World.GetWorld(), Spec.ArtifactRoot(), nullptr}, Projection, Error) && Projection.IsValid(Error)))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("Both actor and mesh pairs remain observable."), Projection.Sorted().Num(), 4);
	TestTrue(TEXT("Retired identity is visible in the projection."),
		Projection.ToCanonicalJson().Contains(TEXT("ProjectWorld.BuildingMassing.v1")));
	return true;
}

REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FProjectWorldBuildingRetiredIdentityProjectionTest,
	"Project.World.Realization.Verify.BuildingRetiredIdentityProjection",
	"[Slow][Integration][World]")

#endif
