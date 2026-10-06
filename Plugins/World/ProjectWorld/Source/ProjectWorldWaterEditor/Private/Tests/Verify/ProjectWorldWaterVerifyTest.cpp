// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldVerifyTestFlow.h"
#include "ProjectWorldWaterRealization.h"

#if WITH_DEV_AUTOMATION_TESTS

// Fixture: the persistent lake with a hole on one cell, under the twin water settings.
namespace ProjectWorldWaterVerifyTest
{
	const ProjectWorldVerifyTestFlow::FSpec Spec{
		TEXT("Project.World.Realization.Verify.Water"), TEXT("Water"), TEXT("project_water_mesh"), 1};

	FProjectWorldCanonicalBundle MakeBundle()
	{
		FProjectWorldCanonicalBundle Bundle;
		Bundle.GridId = TEXT("persistent_water_test");
		Bundle.InputsHash = TEXT("persistent_water_input");
		Bundle.CoordinateQuantizationMeters = 0.01;
		Bundle.HeightQuantizationMeters = 0.1;
		Bundle.HeightOriginMeters = 10.0;
		FProjectWorldCanonicalCell Cell;
		Cell.CellId = TEXT("persistent_water:x0:y0");
		Cell.Bounds = FVector4d(0.0, 0.0, 63.0, 63.0);
		Cell.OwnedFeatureIds.Add(TEXT("water/test/lake"));
		Bundle.Cells.Add(Cell);
		FProjectWorldCanonicalFeature Lake;
		Lake.FeatureId = TEXT("water/test/lake");
		Lake.FeatureClass = TEXT("water");
		Lake.OwnerCellId = Cell.CellId;
		Lake.WaterSurface.bValid = true;
		Lake.WaterSurface.SurfaceGroupId = TEXT("water/test/lake");
		Lake.WaterSurface.Geometry = TEXT("polygon");
		Lake.WaterSurface.Behavior = TEXT("standing");
		Lake.WaterSurface.FunctionId = TEXT("standing_polygon_quantile");
		Lake.WaterSurface.FunctionVersion = 1;
		Lake.WaterSurface.LevelMeters = 12.0;
		FProjectWorldCanonicalPolygon Polygon;
		Polygon.Outer = {
			FVector2D(5.0, 5.0), FVector2D(58.0, 5.0), FVector2D(58.0, 58.0),
			FVector2D(5.0, 58.0), FVector2D(5.0, 5.0)};
		Polygon.Holes.Add({
			FVector2D(20.0, 20.0), FVector2D(20.0, 30.0), FVector2D(30.0, 30.0),
			FVector2D(30.0, 20.0), FVector2D(20.0, 20.0)});
		Lake.GeometryPolygons.Add(MoveTemp(Polygon));
		Bundle.Features.Add(Lake.FeatureId, MoveTemp(Lake));
		return Bundle;
	}

	FProjectWorldRealizationProfile MakeProfile()
	{
		FProjectWorldRealizationLayer Layer;
		Layer.LayerId = TEXT("water");
		Layer.LayerKind = EProjectWorldLayerKind::GeneratedGeography;
		Layer.GeneratorId = TEXT("project_water_mesh");
		Layer.GeneratorVersion = 1;
		Layer.DependsOn = {TEXT("terrain")};
		Layer.CanonicalSelectors = {TEXT("water")};
		Layer.ArtifactRoot = Spec.ArtifactRoot();
		Layer.SpatialOwnership = TEXT("cell_local");
		Layer.DirtyGranularity = EProjectWorldDirtyGranularity::CanonicalCell;
		Layer.RuntimeMapping = TEXT("world_partition_spatial");
		Layer.NormalizedSettings =
			TEXT("{\"material_shading_model\":\"solid_opaque\",\"nanite\":false,\"surface_offset_m\":0.25}");
		FProjectWorldRealizationProfile Profile;
		Profile.ProfileId = TEXT("verify_water");
		Profile.Layers.Add(ProjectWorldVerifyTestFlow::Finalize(MoveTemp(Layer)));
		return Profile;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldWaterVerifyTest,
	"Project.World.Realization.Verify.Water",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldWaterVerifyTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldWaterVerifyTest;
	const FProjectWorldCanonicalBundle Bundle = MakeBundle();
	const FProjectWorldRealizationProfile Profile = MakeProfile();
	return ProjectWorldVerifyTestFlow::Run(*this, Spec,
		[&](FProjectWorldVerifyIdentity& Identity, FString& OutError)
		{
			Identity.AddFixtureBundle(TEXT("memory:bundle"), Bundle);
			Identity.AddFixtureText(TEXT("memory:profile"), ProjectWorldVerifyTestFlow::SerializeProfile(Profile));
			return true;
		},
		[&](UWorld* World, FString& OutError)
		{
			FProjectWorldRealizationResult Result;
			Result.LayerInventories.Add(ProjectWorldVerifyTestFlow::WholeLayer(Profile.Layers[0]));
			return ProjectWorldWaterRealization::Apply(World, Bundle, Profile, Result, OutError);
		});
}

REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FProjectWorldWaterVerifyTest,
	"Project.World.Realization.Verify.Water",
	"[Slow][Integration][World]")

#endif
