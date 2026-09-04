// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldLandscapeRealization.h"
#include "ProjectWorldRealizationService.h"
#include "ProjectWorldTerrainWaterConformance.h"
#include "ProjectWorldTerrainVerification.h"
#include "ProjectWorldWaterMeshBuilder.h"

#include "Editor.h"
#include "EngineUtils.h"
#include "Landscape.h"
#include "LandscapeDataAccess.h"
#include "Misc/AutomationTest.h"
#include "StaticMeshAttributes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldTerrainWaterLayerOrderTest,
	"Project.World.Realization.NativeTwin.TerrainWaterLayerOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldTerrainWaterLayerOrderTest::RunTest(const FString& Parameters)
{
	FProjectWorldCanonicalBundle Bundle;
	Bundle.CellQuads = FIntPoint(7, 7);
	Bundle.SampleSpacingMeters = FVector2D(1.0, 1.0);
	Bundle.CoordinateQuantizationMeters = 0.01;
	Bundle.HeightQuantizationMeters = 0.1;
	Bundle.HeightOriginMeters = 0.0;

	FProjectWorldCanonicalCell Cell;
	Cell.CellId = TEXT("layer_order:x0:y0");
	Cell.Bounds = FVector4d(0.0, 0.0, 7.0, 7.0);
	Cell.Terrain.Bounds = Cell.Bounds;
	Cell.Terrain.SampleSpacing = Bundle.SampleSpacingMeters;
	Cell.Terrain.SamplesX = 8;
	Cell.Terrain.SamplesY = 8;
	Cell.Terrain.HeightsMeters.Init(10.0, 64);
	const int32 WaterCoveredHighSample = 3 * 8 + 3;
	const int32 IslandHighSample = 3 * 8 + 4;
	const int32 DryHighSample = 6 * 8 + 6;
	Cell.Terrain.HeightsMeters[WaterCoveredHighSample] = 20.0;
	Cell.Terrain.HeightsMeters[IslandHighSample] = 20.0;
	Cell.Terrain.HeightsMeters[DryHighSample] = 20.0;

	FProjectWorldCanonicalFeature Water;
	Water.FeatureId = TEXT("water/layer_order");
	Water.FeatureClass = TEXT("water");
	Water.GeometryType = TEXT("Polygon");
	Water.WaterSurface.bValid = true;
	Water.WaterSurface.SurfaceGroupId = TEXT("layer_order");
	Water.WaterSurface.Geometry = TEXT("polygon");
	Water.WaterSurface.Behavior = TEXT("standing");
	Water.WaterSurface.FunctionId = TEXT("standing_polygon_quantile");
	Water.WaterSurface.FunctionVersion = 1;
	Water.WaterSurface.LevelMeters = 12.0;
	FProjectWorldCanonicalPolygon Polygon;
	Polygon.Outer = {
		FVector2D(2.0, 2.0), FVector2D(5.0, 2.0), FVector2D(5.0, 5.0),
		FVector2D(2.0, 5.0), FVector2D(2.0, 2.0)};
	Polygon.Holes.Add({
		FVector2D(3.5, 3.5), FVector2D(3.5, 4.5), FVector2D(4.5, 4.5),
		FVector2D(4.5, 3.5), FVector2D(3.5, 3.5)});
	Water.GeometryPolygons.Add(MoveTemp(Polygon));
	Cell.OwnedFeatureIds.Add(Water.FeatureId);
	Bundle.Features.Add(Water.FeatureId, Water);
	Bundle.Cells.Add(Cell);

	TArray<double> Conditioned;
	FProjectWorldTerrainWaterConformanceStats Stats;
	FString Error;
	TestTrue(
		TEXT("The Landscape projection accepts canonical Terrain plus Water."),
		ProjectWorldTerrainWaterConformance::BuildCellHeights(
			Bundle, Bundle.Cells[0], Conditioned, Stats, Error));
	TestEqual(
		TEXT("Terrain above a Water footprint is lowered to the canonical Water surface."),
		Conditioned[WaterCoveredHighSample],
		12.0);
	TestEqual(
		TEXT("Terrain inside a canonical Water polygon hole remains land."),
		Conditioned[IslandHighSample],
		20.0);
	TestEqual(
		TEXT("Terrain outside Water remains byte-semantically unchanged."),
		Conditioned[DryHighSample],
		20.0);
	TestEqual(TEXT("Exactly one elevated Water sample is conditioned."), Stats.ConditionedSampleCount, 1);
	TestEqual(TEXT("The maximum correction is measured."), Stats.MaximumCorrectionMeters, 8.0);

	const FProjectWorldLandscapeLayout Layout =
		FProjectWorldCanonicalLoader::SelectLandscapeLayout(Bundle);
	FProjectWorldLandscapeHeightfield Heightfield;
	TestTrue(TEXT("The fixture selects a stock Landscape layout."), Layout.bCompatible);
	TestTrue(
		TEXT("The realized Landscape heightfield uses the conditioned projection."),
		ProjectWorldLandscapeRealization::BuildHeightfield(Bundle, Layout, Heightfield, Error));
	TestEqual(
		TEXT("The encoded Landscape stays at or below canonical Water."),
		LandscapeDataAccess::GetLocalHeight(Heightfield.EncodedHeights[WaterCoveredHighSample]),
		12.0f);
	TestEqual(
		TEXT("The encoded dry Landscape remains unchanged."),
		LandscapeDataAccess::GetLocalHeight(Heightfield.EncodedHeights[DryHighSample]),
		20.0f);

	FProjectWorldWaterMeshBuildResult Mesh;
	TestTrue(
		TEXT("The same canonical Water realizes through the production mesh builder."),
		ProjectWorldWaterMeshBuilder::BuildCellSurface(
			Bundle, Bundle.Cells[0], Water, 0.25, Mesh, Error));
	FStaticMeshAttributes Attributes(Mesh.MeshDescription);
	for (const FVertexID Vertex : Mesh.MeshDescription.Vertices().GetElementIDs())
	{
		TestEqual(
			TEXT("Water keeps the profile-owned clearance above the conditioned Landscape."),
			Attributes.GetVertexPositions()[Vertex].Z,
			1225.0f);
	}
	return true;
}

REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FProjectWorldTerrainWaterLayerOrderTest,
	"Project.World.Realization.NativeTwin.TerrainWaterLayerOrder",
	"[Fast][Architecture][World]")

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldTerrainWaterLayerCompositionTest,
	"Project.World.Realization.NativeTwin.TerrainWaterLayerComposition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldTerrainWaterLayerCompositionTest::RunTest(const FString& Parameters)
{
	FProjectWorldCanonicalBundle Bundle;
	Bundle.GridId = TEXT("terrain_water_composition");
	Bundle.InputsHash = TEXT("terrain_water_composition_v1");
	Bundle.CellQuads = FIntPoint(7, 7);
	Bundle.SampleSpacingMeters = FVector2D(1.0, 1.0);
	Bundle.CoordinateQuantizationMeters = 0.01;
	Bundle.HeightQuantizationMeters = 0.1;

	FProjectWorldCanonicalCell Cell;
	Cell.CellId = TEXT("terrain_water_composition:x0:y0");
	Cell.Bounds = FVector4d(0.0, 0.0, 7.0, 7.0);
	Cell.Terrain.Bounds = Cell.Bounds;
	Cell.Terrain.SampleSpacing = Bundle.SampleSpacingMeters;
	Cell.Terrain.SamplesX = 8;
	Cell.Terrain.SamplesY = 8;
	Cell.Terrain.ArtifactHash = TEXT("terrain_v1");
	Cell.Terrain.HeightsMeters.Init(20.0, 64);

	FProjectWorldCanonicalFeature Water;
	Water.FeatureId = TEXT("water/composition");
	Water.FeatureClass = TEXT("water");
	Water.GeometryType = TEXT("Polygon");
	Water.WaterSurface.bValid = true;
	Water.WaterSurface.SurfaceGroupId = TEXT("composition");
	Water.WaterSurface.Geometry = TEXT("polygon");
	Water.WaterSurface.Behavior = TEXT("standing");
	Water.WaterSurface.FunctionId = TEXT("standing_polygon_quantile");
	Water.WaterSurface.FunctionVersion = 1;
	Water.WaterSurface.LevelMeters = 15.0;
	FProjectWorldCanonicalPolygon Polygon;
	Polygon.Outer = {
		FVector2D(0.0, 0.0), FVector2D(7.0, 0.0), FVector2D(7.0, 7.0),
		FVector2D(0.0, 7.0), FVector2D(0.0, 0.0)};
	Water.GeometryPolygons.Add(MoveTemp(Polygon));
	Cell.OwnedFeatureIds.Add(Water.FeatureId);
	Bundle.Features.Add(Water.FeatureId, Water);
	Bundle.Cells.Add(MoveTemp(Cell));

	UWorld* World = GEditor->NewMap(true);
	const FProjectWorldLandscapeLayout Layout =
		FProjectWorldCanonicalLoader::SelectLandscapeLayout(Bundle);
	FProjectWorldRealizationResult CreateResult;
	FString Error;
	TestTrue(TEXT("The layer-order fixture selects a stock Landscape layout."), Layout.bCompatible);
	TestTrue(
		TEXT("The first terrain-over-water state realizes."),
		ProjectWorldLandscapeRealization::CreateOrUpdate(
			World, Bundle, Layout, TEXT("terrain_water_composition"), 1, CreateResult, Error));

	ALandscape* Landscape = nullptr;
	for (TActorIterator<ALandscape> It(World); It; ++It)
	{
		if (ProjectWorldLandscapeRealization::IsGeneratedLandscape(*It))
		{
			Landscape = *It;
			break;
		}
	}
	TestNotNull(TEXT("The realized layer-order fixture owns a Landscape."), Landscape);
	if (Landscape == nullptr)
	{
		return false;
	}

	Bundle.Features[TEXT("water/composition")].WaterSurface.LevelMeters = 12.0;
	Bundle.InputsHash = TEXT("terrain_water_composition_v2");
	FProjectWorldRealizationResult UpdateResult;
	TestTrue(
		TEXT("A Water-only change updates the existing Generated Base."),
		ProjectWorldLandscapeRealization::CreateOrUpdate(
			World, Bundle, Layout, TEXT("terrain_water_composition"), 1, UpdateResult, Error));

	FProjectWorldTerrainHeightComparison FinalComparison;
	TestTrue(
		TEXT("The final visible and collidable Landscape is recomposed after the source write."),
		FProjectWorldTerrainVerification::CompareFinalHeightmapToCanonical(
			Landscape, Bundle, FinalComparison, Error));
	TestEqual(
		TEXT("No final Landscape sample remains above its canonical Water surface."),
		FinalComparison.MismatchCount,
		0);
	return true;
}

REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FProjectWorldTerrainWaterLayerCompositionTest,
	"Project.World.Realization.NativeTwin.TerrainWaterLayerComposition",
	"[Slow][Integration][World]")

#endif
