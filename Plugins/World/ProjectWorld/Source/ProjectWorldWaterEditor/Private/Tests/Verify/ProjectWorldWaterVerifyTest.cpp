// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldVerifyTestFlow.h"
#include "ProjectWorldWaterRealization.h"
#include "ProjectWorldWaterMeshBuilder.h"
#include "ProjectWorldTerrainSurface.h"
#include "StaticMeshAttributes.h"

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldWaterCoverageEnvelopeTest,
	"Project.World.Realization.Alignment.WaterTriangleEnvelope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldWaterCoverageEnvelopeTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldWaterVerifyTest;
	auto Bundle = MakeBundle();
	auto Lake = Bundle.Features.FindChecked(TEXT("water/test/lake"));
	auto Ribbon = Lake;
	Ribbon.GeometryPolygons.Reset();
	Ribbon.GeometryParts = {{FVector2D(0, 15), FVector2D(15, 25), FVector2D(63, 15)}};
	Ribbon.WidthMeters = 3;
	Ribbon.WaterSurface.Geometry = TEXT("ribbon");
	Ribbon.WaterSurface.Behavior = TEXT("flowing");
	Ribbon.WaterSurface.FunctionId = TEXT("rolling_quantile_l1_isotonic");
	Ribbon.WaterSurface.Knots = {FVector(0, 15, 10), FVector(15, 25, 11), FVector(63, 15, 12)};
	for (const auto& Feature : {Lake, Ribbon})
	{
		FProjectWorldWaterMeshBuildResult Output;
		FString Error;
		if (!TestTrue(TEXT("Native production water triangulates and clips."), ProjectWorldWaterMeshBuilder::BuildCellSurface(
			Bundle, Bundle.Cells[0], Feature, 0.25, Output, Error))) { AddError(Error); return false; }
		TestTrue(TEXT("Final clipped water coverage is nonempty."), Output.TriangleCount > 0 && Output.CanonicalAreaSquareMeters > 0);
		const double Floor = Feature.WaterSurface.Behavior == TEXT("standing") ? 12 : 10;
		const auto Positions = FStaticMeshConstAttributes(Output.MeshDescription).GetVertexPositions();
		for (const auto Id : Output.MeshDescription.Triangles().GetElementIDs())
		{
			FProjectWorldSurfaceTriangle Water;
			for (int32 I = 0; I < 3; ++I)
			{
				const auto Vertex = Output.MeshDescription.GetVertexInstanceVertex(Output.MeshDescription.GetTriangleVertexInstance(Id, I));
				Water.Points[I] = FProjectWorldCanonicalLoader::UnrealToCanonical(Bundle, FVector(Positions[Vertex]) + Output.ActorOrigin);
				TestTrue(TEXT("Final affine triangle vertices bound its entire interior above canonical support."), Water.Points[I].Z >= Floor + 0.25 - 1e-5);
			}
			if (Feature.WaterSurface.Behavior != TEXT("standing")) continue;
			const FProjectWorldSurfaceTriangle Holes[] = {
				{{FVector(20, 20, 0), FVector(30, 20, 0), FVector(20, 30, 0)}},
				{{FVector(30, 20, 0), FVector(30, 30, 0), FVector(20, 30, 0)}}};
			for (const auto& Hole : Holes)
			{
				TArray<FVector2D> Intersection;
				TestTrue(TEXT("Native water holes have no positive-area coverage."), ProjectWorldTerrainSurface::IntersectXY(Water, Hole, Intersection, Error) && ProjectWorldTerrainSurface::Area(Intersection) < 1e-8);
			}
		}
		if (Feature.WaterSurface.Behavior == TEXT("standing"))
			TestTrue(TEXT("Standing coverage equals the independent outer-minus-hole area."), FMath::Abs(Output.CanonicalAreaSquareMeters - 2709) < 1e-6);
	}
	return true;
}

#endif
