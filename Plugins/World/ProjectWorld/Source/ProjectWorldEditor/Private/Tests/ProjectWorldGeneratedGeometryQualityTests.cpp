// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldGeneratedGeometry.h"
#include "ProjectWorldGeometryParsing.h"
#include "ProjectWorldRealizationService.h"

#include "Dom/JsonObject.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Misc/AutomationTest.h"
#include "ProceduralMeshComponent.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FProjectWorldCanonicalCell MakeCell(
		const FVector2D& SampleSpacing,
		int32 SamplesX,
		int32 SamplesY,
		const TArray<double>& Heights)
	{
		FProjectWorldCanonicalCell Cell;
		Cell.CellId = TEXT("cell_0");
		Cell.CellX = 0;
		Cell.CellY = 0;
		Cell.Bounds = FVector4d(0.0, 0.0, 10.0, 10.0);
		Cell.Terrain.Bounds = Cell.Bounds;
		Cell.Terrain.SampleSpacing = SampleSpacing;
		Cell.Terrain.SamplesX = SamplesX;
		Cell.Terrain.SamplesY = SamplesY;
		Cell.Terrain.HeightsMeters = Heights;
		return Cell;
	}

	AActor* FindGeneratedCellActor(UWorld* World, const FString& CellId)
	{
		const FName CellTag(*FString::Printf(TEXT("ProjectWorld.Cell=%s"), *CellId));
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->Tags.Contains(CellTag))
			{
				return *It;
			}
		}
		return nullptr;
	}

	UProceduralMeshComponent* FindGeneratedMesh(UWorld* World, const FString& CellId)
	{
		AActor* Actor = FindGeneratedCellActor(World, CellId);
		return Actor != nullptr ? Actor->FindComponentByClass<UProceduralMeshComponent>() : nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldMultiPolygonPartsTest,
	"Project.World.Realization.Geometry.MultiPolygonPartsPreserved",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldMultiPolygonPartsTest::RunTest(const FString& Parameters)
{
	const FString Json = TEXT("{\"type\":\"MultiPolygon\",\"coordinates\":[[[[0,0],[2,0],[2,2],[0,0]]],[[[6,0],[8,0],[8,2],[6,0]]]]}");
	TSharedPtr<FJsonObject> Geometry;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	TestTrue(TEXT("MultiPolygon fixture parses as JSON."), FJsonSerializer::Deserialize(Reader, Geometry));
	if (!Geometry.IsValid())
	{
		return false;
	}

	FString Type;
	TArray<FVector2D> FlattenedPoints;
	TArray<TArray<FVector2D>> Parts;
	FProjectWorldCanonicalValidation Validation;
	TestTrue(
		TEXT("Canonical geometry parser accepts the MultiPolygon."),
		ProjectWorldGeometryParsing::ReadGeometry(
			Geometry,
			Type,
			FlattenedPoints,
			Validation,
			&Parts));
	TestEqual(TEXT("Both disjoint polygon parts remain explicit."), Parts.Num(), 2);
	TestEqual(TEXT("Flattened compatibility points remain available."), FlattenedPoints.Num(), 8);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldPointGeometryTest,
	"Project.World.Realization.Geometry.PointInputsAccepted",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldPointGeometryTest::RunTest(const FString& Parameters)
{
	const TArray<FString> Fixtures = {
		TEXT("{\"type\":\"Point\",\"coordinates\":[1,2]}"),
		TEXT("{\"type\":\"MultiPoint\",\"coordinates\":[[1,2],[3,4]]}"),
	};
	for (const FString& Json : Fixtures)
	{
		TSharedPtr<FJsonObject> Geometry;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		TestTrue(TEXT("Point fixture parses as JSON."), FJsonSerializer::Deserialize(Reader, Geometry));
		FString Type;
		TArray<FVector2D> Points;
		TArray<TArray<FVector2D>> Parts;
		FProjectWorldCanonicalValidation Validation;
		TestTrue(
			TEXT("Canonical geometry parser accepts the point input."),
			ProjectWorldGeometryParsing::ReadGeometry(Geometry, Type, Points, Validation, &Parts));
		TestEqual(TEXT("Every point remains explicit."), Points.Num(), Parts.Num());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldRoadTerrainDrapeTest,
	"Project.World.Realization.Geometry.RoadTerrainDrape",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldRoadTerrainDrapeTest::RunTest(const FString& Parameters)
{
	FProjectWorldCanonicalBundle Bundle;
	Bundle.GridId = TEXT("road_drape_grid");
	Bundle.InputsHash = TEXT("road_drape_input");
	Bundle.CoordinateQuantizationMeters = 0.01;
	Bundle.Cells.Add(MakeCell(
		FVector2D(5.0, 10.0),
		3,
		2,
		{0.0, 10.0, 0.0, 0.0, 10.0, 0.0}));

	FProjectWorldCanonicalFeature Road;
	Road.FeatureId = TEXT("alis:test:road:draped");
	Road.FeatureClass = TEXT("road");
	Road.WidthMeters = 2.0;
	FProjectWorldCanonicalRepresentation Representation;
	Representation.CellId = Bundle.Cells[0].CellId;
	Representation.Kind = TEXT("road_fragment");
	Representation.Parts.Add({FVector2D(0.0, 5.0), FVector2D(10.0, 5.0)});
	Road.Representations.Add(MoveTemp(Representation));
	Bundle.Features.Add(Road.FeatureId, MoveTemp(Road));

	UWorld* World = GEditor->NewMap(false);
	FProjectWorldRealizationResult Result;
	FString Error;
	TestTrue(
		TEXT("Road preview is generated over the peaked terrain."),
		ProjectWorldGeneratedGeometry::CreateOwnedActors(
			World,
			Bundle,
			false,
			1,
			0,
			Result,
			Error));
	UProceduralMeshComponent* Mesh = FindGeneratedMesh(World, Bundle.Cells[0].CellId);
	TestNotNull(TEXT("Generated road mesh exists."), Mesh);
	if (Mesh == nullptr)
	{
		return false;
	}
	const FProcMeshSection* Section = Mesh->GetProcMeshSection(0);
	TestNotNull(TEXT("Generated road section exists."), Section);
	if (Section == nullptr)
	{
		return false;
	}

	double MaximumHeightCentimeters = -DBL_MAX;
	for (const FProcMeshVertex& Vertex : Section->ProcVertexBuffer)
	{
		MaximumHeightCentimeters = FMath::Max(MaximumHeightCentimeters, Vertex.Position.Z);
	}
	TestTrue(TEXT("Road is tessellated beyond one endpoint quad."), Section->ProcVertexBuffer.Num() > 4);
	TestTrue(TEXT("Road keeps render-safe clearance over the ten-meter terrain peak."), MaximumHeightCentimeters >= 1060.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldBuildingPartMassingTest,
	"Project.World.Realization.Geometry.BuildingPartsMassIndependently",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldBuildingPartMassingTest::RunTest(const FString& Parameters)
{
	FProjectWorldCanonicalBundle Bundle;
	Bundle.GridId = TEXT("building_parts_grid");
	Bundle.InputsHash = TEXT("building_parts_input");
	Bundle.Cells.Add(MakeCell(FVector2D(10.0, 10.0), 2, 2, {0.0, 0.0, 0.0, 0.0}));

	FProjectWorldCanonicalFeature Building;
	Building.FeatureId = TEXT("alis:test:building:parts");
	Building.FeatureClass = TEXT("building");
	Building.OwnerCellId = Bundle.Cells[0].CellId;
	Building.GeometryType = TEXT("MultiPolygon");
	Building.HeightMeters = 5.0;
	Building.GeometryParts = {
		{FVector2D(0.0, 1.0), FVector2D(2.0, 1.0), FVector2D(2.0, 3.0), FVector2D(0.0, 3.0), FVector2D(0.0, 1.0)},
		{FVector2D(6.0, 1.0), FVector2D(8.0, 1.0), FVector2D(8.0, 3.0), FVector2D(6.0, 3.0), FVector2D(6.0, 1.0)}};
	for (const TArray<FVector2D>& Part : Building.GeometryParts)
	{
		Building.GeometryPoints.Append(Part);
	}
	Bundle.Features.Add(Building.FeatureId, MoveTemp(Building));

	UWorld* World = GEditor->NewMap(false);
	FProjectWorldRealizationResult Result;
	FString Error;
	TestTrue(
		TEXT("Building preview is generated from both polygon parts."),
		ProjectWorldGeneratedGeometry::CreateOwnedActors(
			World,
			Bundle,
			false,
			0,
			1,
			Result,
			Error));
	UProceduralMeshComponent* Mesh = FindGeneratedMesh(World, Bundle.Cells[0].CellId);
	TestNotNull(TEXT("Generated building mesh exists."), Mesh);
	if (Mesh == nullptr)
	{
		return false;
	}
	const FProcMeshSection* Section = Mesh->GetProcMeshSection(0);
	TestNotNull(TEXT("Generated building section exists."), Section);
	if (Section == nullptr)
	{
		return false;
	}
	TestEqual(TEXT("Two polygon parts produce two independent boxes."), Section->ProcVertexBuffer.Num(), 16);
	TestEqual(TEXT("Two boxes retain independent triangle topology."), Section->ProcIndexBuffer.Num(), 72);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldGeneratedActorRefreshTest,
	"Project.World.Realization.Geometry.ExistingActorPayloadRefreshes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldGeneratedActorRefreshTest::RunTest(const FString& Parameters)
{
	FProjectWorldCanonicalBundle Bundle;
	Bundle.GridId = TEXT("refresh_grid");
	Bundle.InputsHash = TEXT("refresh_input");
	Bundle.CoordinateQuantizationMeters = 0.01;
	Bundle.Cells.Add(MakeCell(FVector2D(10.0, 10.0), 2, 2, {0.0, 0.0, 0.0, 0.0}));
	FProjectWorldCanonicalFeature Road;
	Road.FeatureId = TEXT("alis:test:road:refresh");
	Road.FeatureClass = TEXT("road");
	Road.WidthMeters = 2.0;
	FProjectWorldCanonicalRepresentation Representation;
	Representation.CellId = Bundle.Cells[0].CellId;
	Representation.Kind = TEXT("road_fragment");
	Representation.Parts.Add({FVector2D(0.0, 5.0), FVector2D(10.0, 5.0)});
	Road.Representations.Add(MoveTemp(Representation));
	Bundle.Features.Add(Road.FeatureId, MoveTemp(Road));

	UWorld* World = GEditor->NewMap(false);
	FProjectWorldRealizationResult FirstResult;
	FString Error;
	TestTrue(
		TEXT("Initial generated actor is created."),
		ProjectWorldGeneratedGeometry::CreateOwnedActors(
			World, Bundle, false, 1, 0, FirstResult, Error));
	AActor* FirstActor = FindGeneratedCellActor(World, Bundle.Cells[0].CellId);
	TestNotNull(TEXT("Initial cell actor exists."), FirstActor);

	Bundle.Cells[0].Terrain.HeightsMeters.Init(5.0, 4);
	FProjectWorldRealizationResult SecondResult;
	TestTrue(
		TEXT("Current generated identity survives stale-actor cleanup."),
		ProjectWorldGeneratedGeometry::RemoveStaleOwnedActorsForApply(
			World, Bundle, FString(), false, SecondResult));
	TestTrue(
		TEXT("Existing generated actor payload is rebuilt in place."),
		ProjectWorldGeneratedGeometry::CreateOwnedActors(
			World, Bundle, false, 1, 0, SecondResult, Error));
	AActor* SecondActor = FindGeneratedCellActor(World, Bundle.Cells[0].CellId);
	TestTrue(TEXT("Stable cell actor identity is retained."), FirstActor == SecondActor);
	TestEqual(TEXT("No current cell actor is removed."), SecondResult.RemovedActorCount, 0);
	TestEqual(TEXT("No replacement cell actor is created."), SecondResult.CreatedActorCount, 0);
	TestEqual(TEXT("One cell actor payload is updated."), SecondResult.UpdatedActorCount, 1);

	UProceduralMeshComponent* Mesh = FindGeneratedMesh(World, Bundle.Cells[0].CellId);
	const FProcMeshSection* Section = Mesh != nullptr ? Mesh->GetProcMeshSection(0) : nullptr;
	TestNotNull(TEXT("Refreshed road section exists."), Section);
	if (Section == nullptr)
	{
		return false;
	}
	double MinimumHeightCentimeters = DBL_MAX;
	for (const FProcMeshVertex& Vertex : Section->ProcVertexBuffer)
	{
		MinimumHeightCentimeters = FMath::Min(MinimumHeightCentimeters, Vertex.Position.Z);
	}
	TestTrue(TEXT("Refreshed mesh contains the changed terrain height."), MinimumHeightCentimeters >= 560.0);
	return true;
}

#endif
