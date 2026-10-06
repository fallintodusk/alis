// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldAuthoredOverlay.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldVegetationRealization.h"
#include "ProjectWorldVerifyTestFlow.h"

#if WITH_DEV_AUTOMATION_TESTS

// Fixture: a wood area and a tree point on one cell, an authored mask strip, both declared tree
// meshes, jittered lattice placement, and the road and water layer contracts vegetation reads.
namespace ProjectWorldVegetationVerifyTest
{
	const ProjectWorldVerifyTestFlow::FSpec Spec{
		TEXT("Project.World.Realization.Verify.Vegetation"), TEXT("Vegetation"), TEXT("project_vegetation_instances"), 1};

	FProjectWorldCanonicalBundle MakeBundle()
	{
		FProjectWorldCanonicalBundle Bundle;
		Bundle.GridId = TEXT("vegetation_grid");
		Bundle.HeightOriginMeters = 0.0;
		FProjectWorldCanonicalCell Cell;
		Cell.CellId = TEXT("vegetation_grid:x0:y0");
		Cell.Bounds = FVector4d(0.0, 0.0, 100.0, 100.0);
		Cell.Terrain.ArtifactHash = FString::ChrN(64, TEXT('b'));
		Cell.Terrain.Bounds = Cell.Bounds;
		Cell.Terrain.SampleSpacing = FVector2D(100.0, 100.0);
		Cell.Terrain.SamplesX = 2;
		Cell.Terrain.SamplesY = 2;
		Cell.Terrain.HeightsMeters = {2.0, 2.0, 2.0, 2.0};
		FProjectWorldCanonicalFeature Area;
		Area.FeatureId = TEXT("alis:test:vegetation:area");
		Area.FeatureClass = TEXT("vegetation_area");
		Area.OwnerCellId = Cell.CellId;
		Area.GeometryType = TEXT("MultiPolygon");
		Area.VegetationClass = TEXT("wood");
		Area.LeafType = TEXT("broadleaved");
		FProjectWorldCanonicalPolygon& Square = Area.GeometryPolygons.AddDefaulted_GetRef();
		Square.Outer = {
			FVector2D(0.0, 0.0), FVector2D(100.0, 0.0), FVector2D(100.0, 100.0),
			FVector2D(0.0, 100.0), FVector2D(0.0, 0.0)};
		FProjectWorldCanonicalFeature Point;
		Point.FeatureId = TEXT("alis:test:vegetation:point");
		Point.FeatureClass = TEXT("foliage_point");
		Point.OwnerCellId = Cell.CellId;
		Point.GeometryType = TEXT("Point");
		Point.FoliageClass = TEXT("tree");
		Point.GeometryPoints = {FVector2D(15.0, 85.0)};
		Cell.OwnedFeatureIds = {Area.FeatureId, Point.FeatureId};
		Bundle.Features.Add(Area.FeatureId, Area);
		Bundle.Features.Add(Point.FeatureId, Point);
		Bundle.Cells.Add(Cell);
		return Bundle;
	}

	FProjectWorldRealizationLayer StubLayer(const TCHAR* LayerId, const TCHAR* GeneratorId, const TCHAR* Settings)
	{
		FProjectWorldRealizationLayer Layer;
		Layer.LayerId = LayerId;
		Layer.GeneratorId = GeneratorId;
		Layer.GeneratorVersion = 1;
		Layer.NormalizedSettings = Settings;
		return ProjectWorldVerifyTestFlow::Finalize(MoveTemp(Layer));
	}

	FProjectWorldRealizationProfile MakeProfile()
	{
		FProjectWorldRealizationLayer Vegetation;
		Vegetation.LayerId = TEXT("vegetation");
		Vegetation.LayerKind = EProjectWorldLayerKind::GeneratedGeography;
		Vegetation.GeneratorId = TEXT("project_vegetation_instances");
		Vegetation.GeneratorVersion = 1;
		Vegetation.DependsOn = {TEXT("terrain"), TEXT("water"), TEXT("roads")};
		Vegetation.CanonicalSelectors = {TEXT("vegetation")};
		Vegetation.ArtifactRoot = Spec.ArtifactRoot();
		Vegetation.SpatialOwnership = TEXT("cell_local");
		Vegetation.DirtyGranularity = EProjectWorldDirtyGranularity::CanonicalCell;
		Vegetation.RuntimeMapping = TEXT("world_partition_spatial");
		Vegetation.NormalizedSettings =
			TEXT("{\"area_jitter_fraction\":0.25,\"area_spacing_m\":20,\"collision\":\"no_collision\",")
			TEXT("\"deterministic_seed\":7,\"maximum_instances_per_cell\":100,\"maximum_scale\":1.15,")
			TEXT("\"mesh_assets\":[\"/ProjectObject/Nature/ExteriorPlant/Tree/Hornbeam/SM_Tree_Hornbeam_Medium.SM_Tree_Hornbeam_Medium\",")
			TEXT("\"/ProjectObject/Nature/ExteriorPlant/Tree/AmurCork/SM_Tree_AmurCork_Big.SM_Tree_AmurCork_Big\"],")
			TEXT("\"minimum_scale\":0.85,\"nanite\":true,\"placement_policy\":\"canonical_points_and_lattice_areas\",")
			TEXT("\"surface_offset_m\":0}");
		FProjectWorldRealizationProfile Profile;
		Profile.ProfileId = TEXT("verify_vegetation");
		// Vegetation reads only the water and road layer contracts among its dependencies.
		Profile.Layers = {
			StubLayer(TEXT("water"), TEXT("project_water_mesh"), TEXT("")),
			StubLayer(TEXT("roads"), TEXT("project_road_mesh"), TEXT("{\"selected_classes\":[\"primary\"]}")),
			ProjectWorldVerifyTestFlow::Finalize(MoveTemp(Vegetation))};
		return Profile;
	}

	FProjectWorldAuthoredOverlaySet MakeOverlays()
	{
		FProjectWorldAuthoredOverlaySet Set;
		FProjectWorldAuthoredOverlay& Mask = Set.Overlays.AddDefaulted_GetRef();
		Mask.OverlayId = TEXT("test/vegetation-mask");
		Mask.Anchor.Kind = EProjectWorldAnchorKind::Mask;
		Mask.Anchor.BoundsMeters = FVector4d(45.0, 0.0, 55.0, 100.0);
		Mask.Anchor.Excludes = {TEXT("vegetation")};
		return Set;
	}

	FString SerializeOverlays(const FProjectWorldAuthoredOverlaySet& Set)
	{
		FString Text;
		for (const FProjectWorldAuthoredOverlay& Overlay : Set.Overlays)
		{
			Text += FString::Printf(TEXT("%s|%d|%.17g,%.17g,%.17g,%.17g|%s\n"), *Overlay.OverlayId,
				static_cast<int32>(Overlay.Anchor.Kind), Overlay.Anchor.BoundsMeters.X, Overlay.Anchor.BoundsMeters.Y,
				Overlay.Anchor.BoundsMeters.Z, Overlay.Anchor.BoundsMeters.W, *FString::Join(Overlay.Anchor.Excludes, TEXT(",")));
		}
		return Text;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldVegetationVerifyTest,
	"Project.World.Realization.Verify.Vegetation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldVegetationVerifyTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldVegetationVerifyTest;
	const FProjectWorldCanonicalBundle Bundle = MakeBundle();
	const FProjectWorldRealizationProfile Profile = MakeProfile();
	const FProjectWorldAuthoredOverlaySet Overlays = MakeOverlays();
	return ProjectWorldVerifyTestFlow::Run(*this, Spec,
		[&](FProjectWorldVerifyIdentity& Identity, FString& OutError)
		{
			Identity.AddFixtureBundle(TEXT("memory:bundle"), Bundle);
			Identity.AddFixtureText(TEXT("memory:profile"), ProjectWorldVerifyTestFlow::SerializeProfile(Profile));
			Identity.AddFixtureText(TEXT("memory:overlays"), SerializeOverlays(Overlays));
			return true;
		},
		[&](UWorld* World, FString& OutError)
		{
			FProjectWorldRealizationResult Result;
			Result.LayerInventories.Add(ProjectWorldVerifyTestFlow::WholeLayer(Profile.Layers.Last()));
			return ProjectWorldVegetationRealization::Apply(World, Bundle, Profile, Overlays, Result, OutError);
		});
}

REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FProjectWorldVegetationVerifyTest,
	"Project.World.Realization.Verify.Vegetation",
	"[Slow][Integration][World]")

#endif
