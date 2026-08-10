// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldDataRoots.h"
#include "ProjectWorldGeneratedGeometry.h"
#include "ProjectWorldLandscapeRealization.h"
#include "ProjectWorldRealizationService.h"
#include "ProjectWorldRealizeCommandlet.h"

#include "Editor.h"
#include "EngineUtils.h"
#include "Landscape.h"
#include "LandscapeComponent.h"
#include "LandscapeDataAccess.h"
#include "LandscapeEdit.h"
#include "LandscapeEditLayer.h"
#include "LandscapeInfo.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Materials/MaterialInstance.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldDataRootsTest,
	"Project.World.Realization.WorldDataRoots",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldDataRootsTest::RunTest(const FString& Parameters)
{
	FProjectWorldDataRoots FixtureRoots;
	FProjectWorldDataRoots ProductionRoots;
	FString Error;
	TestTrue(TEXT("Fixture roots resolve from the ProjectWorld plugin descriptor."),
		FProjectWorldDataRoots::Resolve(TEXT("ProjectWorld"), FixtureRoots, Error));
	TestTrue(TEXT("Production roots resolve from the ProjectWorldData plugin descriptor."),
		FProjectWorldDataRoots::Resolve(TEXT("ProjectWorldData"), ProductionRoots, Error));
	TestEqual(TEXT("Production generated mount is derived."),
		ProductionRoots.GeneratedPackageRoot, FString(TEXT("/ProjectWorldData/Generated/")));
	TestEqual(TEXT("Production authored mount is derived."),
		ProductionRoots.AuthoredPackageRoot, FString(TEXT("/ProjectWorldData/Authored/")));
	TestTrue(TEXT("Production maps stay inside their generated mount."),
		ProductionRoots.IsGeneratedPackage(TEXT("/ProjectWorldData/Generated/Kazan/L_Kazan")));
	TestFalse(TEXT("Fixture maps cannot cross into the production owner."),
		FixtureRoots.IsGeneratedPackage(TEXT("/ProjectWorldData/Generated/Kazan/L_Kazan")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldCanonicalCoordinatesRoundTripTest,
	"Project.World.Realization.CanonicalCoordinatesRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldCanonicalCoordinatesRoundTripTest::RunTest(const FString& Parameters)
{
	FProjectWorldCanonicalBundle Bundle;
	Bundle.LatticeOriginMeters = FVector2D(379760.0, 6184170.0);
	Bundle.EngineGeoreferenceOriginMeters = FVector2D(379760.0, 6184170.0);
	Bundle.HeightOriginMeters = 72.4;
	const FVector Canonical(380123.45, 6184987.65, 103.7);
	const FVector Unreal = FProjectWorldCanonicalLoader::CanonicalToUnreal(Bundle, Canonical);
	const FVector RoundTripped = FProjectWorldCanonicalLoader::UnrealToCanonical(Bundle, Unreal);
	TestTrue(TEXT("Projected coordinates survive the UE left-handed mapping."), Canonical.Equals(RoundTripped, 0.000001));
	Bundle.EngineGeoreferenceOriginMeters = FVector2D(381409.0, 6185051.0);
	const FVector Rebased = FProjectWorldCanonicalLoader::CanonicalToUnreal(Bundle, Canonical);
	TestFalse(TEXT("Changing only the engine origin rebases world space."), Rebased.Equals(Unreal));
	TestEqual(
		TEXT("Changing only the engine origin cannot change lattice identity."),
		Bundle.LatticeOriginMeters,
		FVector2D(379760.0, 6184170.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldLandscapeLayoutFailClosedTest,
	"Project.World.Realization.LandscapeLayoutFailClosed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldLandscapeLayoutFailClosedTest::RunTest(const FString& Parameters)
{
	FProjectWorldCanonicalBundle Bundle;
	Bundle.CellQuads = FIntPoint(2, 2);
	FProjectWorldCanonicalCell Left;
	Left.CellX = 0;
	Left.CellY = 0;
	FProjectWorldCanonicalCell Right;
	Right.CellX = 1;
	Right.CellY = 0;
	Bundle.Cells = {Left, Right};

	const FProjectWorldLandscapeLayout FixtureLayout =
		FProjectWorldCanonicalLoader::SelectLandscapeLayout(Bundle);
	TestFalse(TEXT("The tiny fixture cannot be silently resampled into Landscape."), FixtureLayout.bCompatible);
	TestEqual(TEXT("Fixture total quads X."), FixtureLayout.TotalQuads.X, 4);
	TestEqual(TEXT("Fixture total quads Y."), FixtureLayout.TotalQuads.Y, 2);

	Bundle.CellQuads = FIntPoint(63, 63);
	const FProjectWorldLandscapeLayout CompatibleLayout =
		FProjectWorldCanonicalLoader::SelectLandscapeLayout(Bundle);
	TestTrue(TEXT("Two 63-quad cells map exactly to stock Landscape components."), CompatibleLayout.bCompatible);
	TestEqual(TEXT("Landscape components X."), CompatibleLayout.ComponentCount.X, 2);
	TestEqual(TEXT("Landscape components Y."), CompatibleLayout.ComponentCount.Y, 1);
	TestEqual(TEXT("Landscape quads per section."), CompatibleLayout.QuadsPerSection, 63);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldLandscapeHeightfieldTest,
	"Project.World.Realization.LandscapeHeightfield",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldLandscapeHeightfieldTest::RunTest(const FString& Parameters)
{
	FProjectWorldCanonicalBundle Bundle;
	Bundle.CellQuads = FIntPoint(7, 7);
	Bundle.SampleSpacingMeters = FVector2D(1.0, 1.0);
	Bundle.HeightQuantizationMeters = 0.1;
	Bundle.HeightOriginMeters = 0.0;
	for (int32 CellX = 0; CellX < 2; ++CellX)
	{
		FProjectWorldCanonicalCell Cell;
		Cell.CellId = FString::Printf(TEXT("cell_%d"), CellX);
		Cell.CellX = CellX;
		Cell.CellY = 0;
		Cell.Terrain.SamplesX = 8;
		Cell.Terrain.SamplesY = 8;
		Cell.Terrain.SampleSpacing = Bundle.SampleSpacingMeters;
		Cell.Terrain.ArtifactHash = FString::Printf(TEXT("terrain_%d"), CellX);
		Cell.Terrain.Bounds = FVector4d(CellX * 7.0, 0.0, (CellX + 1) * 7.0, 7.0);
		for (int32 Row = 0; Row < 8; ++Row)
		{
			for (int32 Column = 0; Column < 8; ++Column)
			{
				Cell.Terrain.HeightsMeters.Add(100.0 - Row * 10.0 + CellX * 7 + Column);
			}
		}
		Bundle.Cells.Add(MoveTemp(Cell));
	}

	const FProjectWorldLandscapeLayout Layout =
		FProjectWorldCanonicalLoader::SelectLandscapeLayout(Bundle);
	FProjectWorldLandscapeHeightfield Heightfield;
	FString Error;
	TestTrue(TEXT("Exact stock Landscape layout is selected."), Layout.bCompatible);
	TestTrue(
		TEXT("Adjacent canonical terrain assembles without resampling."),
		ProjectWorldLandscapeRealization::BuildHeightfield(Bundle, Layout, Heightfield, Error));
	TestEqual(TEXT("Landscape vertex width."), Heightfield.VertexCount.X, 15);
	TestEqual(TEXT("Landscape vertex height."), Heightfield.VertexCount.Y, 8);
	TestEqual(
		TEXT("Landscape row zero preserves the canonical northern row."),
		LandscapeDataAccess::GetLocalHeight(Heightfield.EncodedHeights[0]),
		100.0f);
	TestEqual(
		TEXT("Canonical row zero maps to the northern terrain bound."),
		FProjectWorldCanonicalLoader::TerrainRowNorthing(Bundle.Cells[0].Terrain, 0),
		7.0);
	TestEqual(
		TEXT("Canonical south maps to the final terrain row."),
		FProjectWorldCanonicalLoader::TerrainSampleRow(Bundle.Cells[0].Terrain, 0.0),
		7.0);

	Bundle.Cells[1].Terrain.HeightsMeters[0] += 1.0;
	TestFalse(
		TEXT("A mismatched shared terrain edge fails closed."),
		ProjectWorldLandscapeRealization::BuildHeightfield(Bundle, Layout, Heightfield, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldGeoReferencingPlacementTest,
	"Project.World.Realization.GeoReferencingMatchesPlacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldGeoReferencingPlacementTest::RunTest(const FString& Parameters)
{
	FProjectWorldCanonicalBundle Bundle;
	Bundle.CanonicalCrs = TEXT("EPSG:32639");
	Bundle.LatticeOriginMeters = FVector2D(379760.0, 6184170.0);
	Bundle.EngineGeoreferenceOriginMeters = FVector2D(379760.0, 6184170.0);
	Bundle.HeightOriginMeters = 72.0;
	Bundle.CoordinateQuantizationMeters = 0.01;
	for (int32 CellX = 0; CellX < 2; ++CellX)
	{
		FProjectWorldCanonicalCell Cell;
		Cell.CellId = FString::Printf(TEXT("cell_%d"), CellX);
		Cell.Bounds = FVector4d(379760.0 + CellX * 930.0, 6184170.0, 380690.0 + CellX * 930.0, 6185100.0);
		Bundle.Cells.Add(MoveTemp(Cell));
	}

	FProjectWorldRealizationResult Result;
	FString Error;
	const double ErrorMeters = ProjectWorldGeneratedGeometry::MeasureCoordinateRoundTrip(
		GEditor->NewMap(false), Bundle, false, Result, Error);
	TestTrue(TEXT("GeoReferencing reverse round trip stays inside tolerance."), ErrorMeters <= 0.01);
	TestTrue(
		TEXT("GeoReferencing placement equals the canonical actor transform."),
		Result.GeoReferencingPlacementErrorMeters <= 0.01);
	TestTrue(TEXT("Both cell corners and shared edge are probed."), Result.GeoReferencingProbePointCount >= 8);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldCrossCellRoadTest,
	"Project.World.Realization.CrossCellRoadIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldCrossCellRoadTest::RunTest(const FString& Parameters)
{
	FProjectWorldCanonicalBundle Bundle;
	Bundle.GridId = TEXT("road_grid");
	Bundle.InputsHash = TEXT("road_input");
	Bundle.CoordinateQuantizationMeters = 0.01;
	for (int32 CellX = 0; CellX < 2; ++CellX)
	{
		FProjectWorldCanonicalCell Cell;
		Cell.CellId = FString::Printf(TEXT("cell_%d"), CellX);
		Cell.CellX = CellX;
		Cell.Bounds = FVector4d(CellX * 10.0, 0.0, (CellX + 1) * 10.0, 10.0);
		Cell.Terrain.Bounds = Cell.Bounds;
		Cell.Terrain.SampleSpacing = FVector2D(10.0, 10.0);
		Cell.Terrain.SamplesX = 2;
		Cell.Terrain.SamplesY = 2;
		Cell.Terrain.HeightsMeters.Init(0.0, 4);
		Bundle.Cells.Add(MoveTemp(Cell));
	}
	FProjectWorldCanonicalFeature Road;
	Road.FeatureId = TEXT("alis:test:road:cross-cell");
	Road.FeatureClass = TEXT("road");
	Road.WidthMeters = 4.0;
	for (int32 CellX = 0; CellX < 2; ++CellX)
	{
		FProjectWorldCanonicalRepresentation Representation;
		Representation.CellId = Bundle.Cells[CellX].CellId;
		Representation.Kind = TEXT("road_fragment");
		Representation.Parts.Add({FVector2D(5.0 + CellX * 5.0, 5.0), FVector2D(10.0 + CellX * 5.0, 5.0)});
		Road.Representations.Add(MoveTemp(Representation));
	}
	Bundle.Features.Add(Road.FeatureId, MoveTemp(Road));
	FProjectWorldCanonicalFeature OtherRoad;
	OtherRoad.FeatureId = TEXT("alis:test:road:single-cell");
	OtherRoad.FeatureClass = TEXT("road");
	OtherRoad.WidthMeters = 3.0;
	FProjectWorldCanonicalRepresentation OtherRepresentation;
	OtherRepresentation.CellId = Bundle.Cells[0].CellId;
	OtherRepresentation.Kind = TEXT("road_fragment");
	OtherRepresentation.Parts.Add({FVector2D(1.0, 2.0), FVector2D(4.0, 2.0)});
	OtherRoad.Representations.Add(MoveTemp(OtherRepresentation));
	Bundle.Features.Add(OtherRoad.FeatureId, MoveTemp(OtherRoad));

	FProjectWorldRealizationResult Result;
	FString Error;
	TestTrue(
		TEXT("Cross-cell road realizes through one canonical identity."),
		ProjectWorldGeneratedGeometry::CreateOwnedActors(
			GEditor->NewMap(false), Bundle, false, 2, 0, Result, Error));
	TestEqual(TEXT("Selected road identity is recorded."), Result.CrossCellRoadFeatureId, FString(TEXT("alis:test:road:cross-cell")));
	TestEqual(TEXT("Two canonical fragments are expected."), Result.CrossCellRoadExpectedFragmentCount, 2);
	TestEqual(TEXT("Two canonical fragments are realized."), Result.CrossCellRoadRealizedFragmentCount, 2);
	TestTrue(TEXT("Fragments share a boundary coordinate."), Result.CrossCellRoadSharedBoundaryPointCount >= 1);
	TestEqual(TEXT("The second distinct road is not consumed by the primary-road counter."), Result.RoadSectionCount, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldGeneratedCellPlacementTest,
	"Project.World.Realization.GeneratedCellPlacementMatchesLandscape",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldGeneratedCellPlacementTest::RunTest(const FString& Parameters)
{
	FProjectWorldCanonicalBundle Bundle;
	Bundle.GridId = TEXT("placement_grid");
	Bundle.InputsHash = TEXT("placement_input");
	Bundle.LatticeOriginMeters = FVector2D(100.0, 200.0);
	Bundle.EngineGeoreferenceOriginMeters = FVector2D(100.0, 200.0);
	Bundle.CellQuads = FIntPoint(7, 7);
	Bundle.SampleSpacingMeters = FVector2D(1.0, 1.0);
	Bundle.HeightQuantizationMeters = 0.1;
	Bundle.CoordinateQuantizationMeters = 0.01;
	for (int32 CellX = 0; CellX < 2; ++CellX)
	{
		FProjectWorldCanonicalCell Cell;
		Cell.CellId = FString::Printf(TEXT("cell_%d"), CellX);
		Cell.CellX = CellX;
		Cell.CellY = 0;
		Cell.Bounds = FVector4d(100.0 + CellX * 7.0, 200.0, 107.0 + CellX * 7.0, 207.0);
		Cell.Terrain.Bounds = Cell.Bounds;
		Cell.Terrain.SampleSpacing = Bundle.SampleSpacingMeters;
		Cell.Terrain.SamplesX = 8;
		Cell.Terrain.SamplesY = 8;
		Cell.Terrain.HeightsMeters.Init(0.0, 64);
		Cell.Terrain.ArtifactHash = FString::Printf(TEXT("terrain_%d"), CellX);
		Bundle.Cells.Add(MoveTemp(Cell));
	}

	FProjectWorldCanonicalFeature Road;
	Road.FeatureId = TEXT("alis:test:road:placement");
	Road.FeatureClass = TEXT("road");
	Road.WidthMeters = 1.0;
	for (int32 CellX = 0; CellX < 2; ++CellX)
	{
		FProjectWorldCanonicalRepresentation Representation;
		Representation.CellId = Bundle.Cells[CellX].CellId;
		Representation.Kind = TEXT("road_fragment");
		Representation.Parts.Add({
			FVector2D(102.0 + CellX * 5.0, 203.0),
			FVector2D(107.0 + CellX * 5.0, 203.0)});
		Road.Representations.Add(MoveTemp(Representation));
	}
	Bundle.Features.Add(Road.FeatureId, MoveTemp(Road));

	UWorld* World = GEditor->NewMap(false);
	const FProjectWorldLandscapeLayout Layout =
		FProjectWorldCanonicalLoader::SelectLandscapeLayout(Bundle);
	FProjectWorldRealizationResult Result;
	FString Error;
	UMaterialInterface* PresentationMaterial = LoadObject<UMaterialInterface>(
		nullptr,
		TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"));
	TestNotNull(TEXT("Landscape material test fixture resolves."), PresentationMaterial);
	TestTrue(
		TEXT("Landscape is created for the placement fixture."),
		ProjectWorldLandscapeRealization::CreateOrUpdate(
			World,
			Bundle,
			Layout,
			Result,
			Error,
			PresentationMaterial));
	TestTrue(
		TEXT("Generated cells are created for the placement fixture."),
		ProjectWorldGeneratedGeometry::CreateOwnedActors(World, Bundle, false, 1, 0, Result, Error));

	ALandscape* Landscape = nullptr;
	for (TActorIterator<ALandscape> It(World); It; ++It)
	{
		if (ProjectWorldLandscapeRealization::IsGeneratedLandscape(*It))
		{
			Landscape = *It;
			break;
		}
	}
	TestNotNull(TEXT("Generated Landscape exists for bounds comparison."), Landscape);
	if (Landscape == nullptr)
	{
		return false;
	}
	for (const ULandscapeComponent* Component : Landscape->LandscapeComponents)
	{
		const UMaterialInstance* MaterialInstance = Component->GetMaterialInstance(0, false);
		TestNotNull(TEXT("Landscape component material instance is generated."), MaterialInstance);
		if (MaterialInstance != nullptr)
		{
			TestTrue(
				TEXT("Landscape component instance uses the assigned material base."),
				MaterialInstance->GetMaterial() == PresentationMaterial->GetMaterial());
		}
	}
	const FBox LandscapeBounds = Landscape->GetComponentsBoundingBox(true);
	for (const FProjectWorldCanonicalCell& Cell : Bundle.Cells)
	{
		AActor* CellActor = nullptr;
		const FName CellTag(*FString::Printf(TEXT("ProjectWorld.Cell=%s"), *Cell.CellId));
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->Tags.Contains(CellTag))
			{
				CellActor = *It;
				break;
			}
		}
		TestNotNull(TEXT("Generated cell actor exists."), CellActor);
		if (CellActor == nullptr)
		{
			return false;
		}
		const FVector ExpectedOrigin = FProjectWorldCanonicalLoader::CanonicalToUnreal(
			Bundle,
			FVector(Cell.Bounds.X, Cell.Bounds.W, Bundle.HeightOriginMeters));
		TestTrue(TEXT("Generated cell actor keeps its canonical origin."), CellActor->GetActorLocation().Equals(ExpectedOrigin));
		const FBox CellBounds = CellActor->GetComponentsBoundingBox(true);
		TestTrue(TEXT("Generated cell has valid world bounds."), CellBounds.IsValid != 0);
		TestTrue(
			TEXT("Generated cell X bounds remain inside the Landscape."),
			CellBounds.Min.X >= LandscapeBounds.Min.X - 1.0 && CellBounds.Max.X <= LandscapeBounds.Max.X + 1.0);
		TestTrue(
			TEXT("Generated cell Y bounds remain inside the Landscape."),
			CellBounds.Min.Y >= LandscapeBounds.Min.Y - 1.0 && CellBounds.Max.Y <= LandscapeBounds.Max.Y + 1.0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldAuthoredLandscapeLayerTest,
	"Project.World.Realization.AuthoredLandscapeLayerSurvives",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldAuthoredLandscapeLayerTest::RunTest(const FString& Parameters)
{
	FProjectWorldCanonicalBundle Bundle;
	Bundle.GridId = TEXT("test_grid");
	Bundle.InputsHash = TEXT("test_input");
	Bundle.CellQuads = FIntPoint(7, 7);
	Bundle.SampleSpacingMeters = FVector2D(1.0, 1.0);
	Bundle.HeightQuantizationMeters = 0.1;
	Bundle.HeightOriginMeters = 0.0;
	for (int32 CellX = 0; CellX < 2; ++CellX)
	{
		FProjectWorldCanonicalCell Cell;
		Cell.CellId = FString::Printf(TEXT("cell_%d"), CellX);
		Cell.CellX = CellX;
		Cell.Terrain.SamplesX = 8;
		Cell.Terrain.SamplesY = 8;
		Cell.Terrain.SampleSpacing = Bundle.SampleSpacingMeters;
		Cell.Terrain.HeightsMeters.Init(static_cast<double>(CellX), 64);
		Cell.Terrain.ArtifactHash = FString::Printf(TEXT("terrain_%d"), CellX);
		Cell.Bounds = FVector4d(CellX * 7.0, 0.0, (CellX + 1) * 7.0, 7.0);
		Bundle.Cells.Add(MoveTemp(Cell));
	}
	Bundle.Cells[0].Terrain.HeightsMeters.Init(0.0, 64);
	Bundle.Cells[1].Terrain.HeightsMeters.Init(0.0, 64);

	UWorld* World = GEditor->NewMap(false);
	AActor* AuthoredActor = World->SpawnActor<AActor>();
	AuthoredActor->SetActorLocation(FVector(100.0, 200.0, 300.0));
	const FVector AuthoredLocation = AuthoredActor->GetActorLocation();
	const FProjectWorldLandscapeLayout Layout =
		FProjectWorldCanonicalLoader::SelectLandscapeLayout(Bundle);
	FProjectWorldRealizationResult FirstResult;
	FString Error;
	TestTrue(
		TEXT("Test Landscape can be created."),
		ProjectWorldLandscapeRealization::CreateOrUpdate(World, Bundle, Layout, FirstResult, Error));

	ALandscape* Landscape = nullptr;
	for (TActorIterator<ALandscape> It(World); It; ++It)
	{
		if (ProjectWorldLandscapeRealization::IsGeneratedLandscape(*It))
		{
			Landscape = *It;
			break;
		}
	}
	TestNotNull(TEXT("Generated Landscape exists."), Landscape);
	if (Landscape == nullptr)
	{
		return false;
	}
	const FVector LandscapeLocation = Landscape->GetActorLocation();

	ULandscapeEditLayerBase* AuthoredLayer = Landscape->GetEditLayer(TEXT("Authored Corrections"));
	TestNotNull(TEXT("Protected authored layer exists."), AuthoredLayer);
	if (AuthoredLayer == nullptr)
	{
		return false;
	}
	const FGuid AuthoredGuid = AuthoredLayer->GetGuid();
	const uint16 Marker = LandscapeDataAccess::GetTexHeight(5.0f);
	{
		FScopedSetLandscapeEditingLayer Scope(Landscape, AuthoredGuid);
		FLandscapeEditDataInterface EditData(Landscape->GetLandscapeInfo(), AuthoredGuid, false);
		EditData.SetHeightData(1, 1, 1, 1, &Marker, 1, false);
	}

	Bundle.Cells[0].Terrain.HeightsMeters[0] = -10.0;
	Bundle.Cells[0].Terrain.ArtifactHash = TEXT("terrain_0_changed");
	FProjectWorldRealizationResult SecondResult;
	TestTrue(
		TEXT("Generated Base can be updated."),
		ProjectWorldLandscapeRealization::CreateOrUpdate(World, Bundle, Layout, SecondResult, Error));
	uint16 PreservedMarker = 0;
	FLandscapeEditDataInterface ReadData(Landscape->GetLandscapeInfo(), AuthoredGuid, false);
	ReadData.GetHeightDataFast(1, 1, 1, 1, &PreservedMarker, 1);
	TestEqual(TEXT("Authored correction payload survives base regeneration."), PreservedMarker, Marker);
	TestTrue(TEXT("A lower terrain minimum cannot move an authored actor."), AuthoredActor->GetActorLocation().Equals(AuthoredLocation));
	TestTrue(TEXT("A lower terrain minimum cannot move the generated Landscape origin."), Landscape->GetActorLocation().Equals(LandscapeLocation));
	TestTrue(TEXT("Regeneration reports authored-layer preservation."), SecondResult.bAuthoredCorrectionLayerPreserved);
	TestEqual(TEXT("Only the changed canonical cell updates one Landscape component."), SecondResult.UpdatedLandscapeComponentCount, 1);

	Landscape->Tags.Remove(FName(TEXT("ProjectWorld.TerrainRows=north_to_south_v1")));
	FProjectWorldRealizationResult RowContractResult;
	TestTrue(
		TEXT("A stale terrain-row contract can be corrected."),
		ProjectWorldLandscapeRealization::CreateOrUpdate(World, Bundle, Layout, RowContractResult, Error));
	TestEqual(
		TEXT("Changing the terrain-row contract refreshes the complete Landscape."),
		RowContractResult.UpdatedLandscapeComponentCount,
		2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldOwnedDeletionTest,
	"Project.World.Realization.OwnedDeletionPreservesAuthoredActor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldOwnedDeletionTest::RunTest(const FString& Parameters)
{
	UWorld* World = GEditor->NewMap(false);
	AActor* AuthoredActor = World->SpawnActor<AActor>();
	AActor* GeneratedActor = World->SpawnActor<AActor>();
	GeneratedActor->Tags.Add(FName(TEXT("ProjectWorld.Generated.v1")));
	FProjectWorldRealizationResult Result;
	TestTrue(
		TEXT("Owned deletion succeeds."),
		ProjectWorldGeneratedGeometry::RemoveOwnedActors(World, false, Result));
	TestTrue(TEXT("Untagged authored actor remains valid."), IsValid(AuthoredActor));
	TestEqual(TEXT("Exactly one generated actor is removed."), Result.RemovedActorCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldCanonicalOutputIntegrityTest,
	"Project.World.Realization.CanonicalOutputIntegrity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldCanonicalOutputIntegrityTest::RunTest(const FString& Parameters)
{
	const FString Root = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/ProjectWorld"));
	const FString Payload = FPaths::Combine(Root, TEXT("payload.json"));
	IFileManager::Get().MakeDirectory(*Root, true);
	TestTrue(TEXT("Test payload can be written."), FFileHelper::SaveStringToFile(TEXT("abc"), *Payload));

	FString Hash;
	TestTrue(TEXT("Canonical payload can be hashed."), FProjectWorldCanonicalLoader::ComputeFileSha256(Payload, Hash));
	TestEqual(
		TEXT("SHA-256 uses the portable ProjectCore implementation."),
		Hash,
		FString(TEXT("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")));

	FString Resolved;
	TestTrue(
		TEXT("Owned relative output resolves inside its receipt root."),
		FProjectWorldCanonicalLoader::ResolveOwnedOutputPath(Root, TEXT("canonical/cell.json"), Resolved));
	TestFalse(
		TEXT("Parent traversal cannot escape the receipt root."),
		FProjectWorldCanonicalLoader::ResolveOwnedOutputPath(Root, TEXT("../cell.json"), Resolved));
	IFileManager::Get().Delete(*Payload, false, true, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldCommandletBoundaryTest,
	"Project.World.Realization.CommandletBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldCommandletBoundaryTest::RunTest(const FString& Parameters)
{
	UProjectWorldRealizeCommandlet* Commandlet = NewObject<UProjectWorldRealizeCommandlet>();
	const FString MissingCompile = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/MissingCompileResult.json"));
	const FString UnsafeResult = FPaths::Combine(FPaths::ProjectDir(), TEXT("tmp/world/unsafe_realization.json"));
	const FString SafeResult = FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("Validation/WorldRealization/Automation/delete_without_presentation.json"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(SafeResult), true);
	AddExpectedError(
		TEXT("Unsafe result path"),
		EAutomationExpectedErrorFlags::Contains,
		1);
	AddExpectedError(
		TEXT("Rejected - code=receipt-read"),
		EAutomationExpectedErrorFlags::Contains,
		1);
	TestEqual(
		TEXT("The real commandlet rejects evidence outside its owned validation root."),
		Commandlet->Main(FString::Printf(
			TEXT("-CompileResult=\"%s\" -Result=\"%s\" -Mode=delete"),
			*MissingCompile,
			*UnsafeResult)),
		2);
	TestFalse(TEXT("Unsafe commandlet evidence is not emitted."), IFileManager::Get().FileExists(*UnsafeResult));
	TestEqual(
		TEXT("Delete reaches canonical validation without a presentation profile."),
		Commandlet->Main(FString::Printf(
			TEXT("-CompileResult=\"%s\" -Result=\"%s\" -Mode=delete"),
			*MissingCompile,
			*SafeResult)),
		4);
	TestTrue(TEXT("Safe commandlet evidence is emitted."), IFileManager::Get().FileExists(*SafeResult));
	IFileManager::Get().Delete(*SafeResult, false, true, true);
	return true;
}

#endif
