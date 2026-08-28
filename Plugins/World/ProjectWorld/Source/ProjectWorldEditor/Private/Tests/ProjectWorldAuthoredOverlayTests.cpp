// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldAuthoredOverlay.h"
#include "ProjectWorldAuthoredOverlayRealization.h"
#include "Tests/ProjectWorldSchemaTestUtilities.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldRealizationService.h"

#include "Editor.h"
#include "LevelInstance/LevelInstanceActor.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ProjectWorldAuthoredOverlayTests
{
	const TCHAR* GridId = TEXT("grid_413718bc833994e5");

	FProjectWorldCanonicalBundle MakeBundle(bool bIncludeAnchoredFeature = true)
	{
		FProjectWorldCanonicalBundle Bundle;
		Bundle.WorldDataPluginName = TEXT("ProjectWorldTestData");
		Bundle.GridId = GridId;
		Bundle.CanonicalCrs = TEXT("EPSG:32639");
		Bundle.VerticalDatum = TEXT("EPSG:3855");
		Bundle.LatticeOriginMeters = FVector2D(1000.0, 2000.0);
		Bundle.EngineGeoreferenceOriginMeters = FVector2D(1000.0, 2000.0);
		Bundle.HeightOriginMeters = 0.0;
		Bundle.CoordinateQuantizationMeters = 0.01;
		Bundle.HeightQuantizationMeters = 0.1;
		FProjectWorldCanonicalCell Cell;
		Cell.CellId = TEXT("fixture_cell");
		Cell.Bounds = FVector4d(900.0, 1900.0, 1300.0, 2200.0);
		Cell.Terrain.Bounds = Cell.Bounds;
		Cell.Terrain.SampleSpacing = FVector2D(400.0, 300.0);
		Cell.Terrain.SamplesX = 2;
		Cell.Terrain.SamplesY = 2;
		Cell.Terrain.HeightsMeters = {25.0, 25.0, 25.0, 25.0};
		Cell.Terrain.VerticalProvenanceId = TEXT("fixture_terrain");
		Cell.Terrain.VerticalDatum = TEXT("EPSG:3855");
		Cell.Terrain.VerticalConfidence = TEXT("fixture_exact");
		Cell.Terrain.VerticalSourceAccuracyMeters = 0.5;
		Cell.Terrain.SamplingQuantizationResidualMeters = 0.05;
		Bundle.Cells.Add(MoveTemp(Cell));
		if (bIncludeAnchoredFeature)
		{
			FProjectWorldCanonicalFeature Feature;
			Feature.FeatureId = TEXT("alis:osm:way:1151612452");
			Feature.FeatureClass = TEXT("road");
			Feature.GeometryType = TEXT("LineString");
			// Arc-length midpoint is (1100, 2000).
			Feature.GeometryPoints = {FVector2D(1000.0, 2000.0), FVector2D(1200.0, 2000.0)};
			Feature.GeometryParts.Add(Feature.GeometryPoints);
			Bundle.Features.Add(Feature.FeatureId, MoveTemp(Feature));
		}
		return Bundle;
	}

	FProjectWorldAuthoredOverlaySet MakeSet()
	{
		FProjectWorldAuthoredOverlaySet Set;
		Set.OverlaySetId = TEXT("fixture");
		Set.WorldDataPluginName = TEXT("ProjectWorldTestData");
		Set.GridId = GridId;
		Set.ResolverVersion = ProjectWorldAuthoredOverlay::SupportedResolverVersion;
		FProjectWorldAnchorProvenance Provenance;
		Provenance.ProvenanceId = TEXT("fixture_control");
		Provenance.HorizontalAccuracyMeters = 0.25;
		Provenance.bHasHorizontalAccuracy = true;
		Set.Provenance.Add(Provenance.ProvenanceId, MoveTemp(Provenance));
		FProjectWorldAnchorProvenance VerticalProvenance;
		VerticalProvenance.ProvenanceId = TEXT("fixture_height_control");
		VerticalProvenance.VerticalDatum = TEXT("EPSG:3855");
		VerticalProvenance.VerticalAccuracyMeters = 0.5;
		VerticalProvenance.bHasVerticalAccuracy = true;
		Set.Provenance.Add(VerticalProvenance.ProvenanceId, MoveTemp(VerticalProvenance));
		return Set;
	}

	FProjectWorldAuthoredOverlay MakeFeatureOverlay(double Tolerance)
	{
		FProjectWorldAuthoredOverlay Overlay;
		Overlay.OverlayId = TEXT("hero_prop");
		Overlay.AuthoredPackage = TEXT("/ProjectWorldTestData/Authored/Fixtures/L_HeroProp");
		Overlay.Anchor.Kind = EProjectWorldAnchorKind::Feature;
		Overlay.Anchor.FeatureId = TEXT("alis:osm:way:1151612452");
		Overlay.Anchor.ExpectedFeatureClass = TEXT("road");
		Overlay.Anchor.ExpectedGeometryType = TEXT("LineString");
		Overlay.Anchor.PlacementClass = EProjectWorldPlacementClass::Standard;
		Overlay.Anchor.ProvenanceRef = TEXT("fixture_control");
		Overlay.Anchor.HorizontalToleranceMeters = Tolerance;
		Overlay.Anchor.VerticalMode = EProjectWorldVerticalMode::SurfaceSnap;
		Overlay.Anchor.VerticalToleranceMeters = 1.0;
		Overlay.Anchor.ExpectedEastingMeters = 1100.0;
		Overlay.Anchor.ExpectedNorthingMeters = 2000.0;
		return Overlay;
	}
}

/**
 * Coordinate anchors are independent of every generated artifact: the same
 * canonical coordinate must resolve to the same world transform no matter
 * what the generated layers below it look like.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldAuthoredOverlayCoordinateTest,
	"Project.World.Authored.Anchor.Coordinate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldAuthoredOverlayCoordinateTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldAuthoredOverlayTests;
	const FProjectWorldCanonicalBundle Bundle = MakeBundle();
	const FProjectWorldAuthoredOverlaySet Set = MakeSet();

	FProjectWorldAuthoredOverlay Overlay;
	Overlay.OverlayId = TEXT("plaza_marker");
	Overlay.AuthoredPackage = TEXT("/ProjectWorldTestData/Authored/Fixtures/L_PlazaMarker");
	Overlay.Anchor.Kind = EProjectWorldAnchorKind::Coordinate;
	Overlay.Anchor.PlacementClass = EProjectWorldPlacementClass::Precision;
	Overlay.Anchor.ProvenanceRef = TEXT("fixture_control");
	Overlay.Anchor.HorizontalToleranceMeters = 1.0;
	Overlay.Anchor.CanonicalCrs = TEXT("EPSG:32639");
	Overlay.Anchor.EastingMeters = 1150.0;
	Overlay.Anchor.NorthingMeters = 2100.0;
	Overlay.Anchor.VerticalDatum = TEXT("EPSG:3855");
	Overlay.Anchor.VerticalProvenanceRef = TEXT("fixture_height_control");
	Overlay.Anchor.HeightMeters = 5.0;
	Overlay.Anchor.VerticalToleranceMeters = 1.0;
	Overlay.Anchor.VerticalMode = EProjectWorldVerticalMode::Absolute;

	FProjectWorldAnchorResolution Resolution;
	FString Error;
	TestTrue(TEXT("A coordinate anchor resolves."),
		ProjectWorldAuthoredOverlay::Resolve(Bundle, Set, Overlay, Resolution, Error));
	TestTrue(TEXT("A coordinate anchor places content."), Resolution.bPlaces);
	// Canonical metres -> centimetres, with northing mirrored into Unreal Y.
	TestEqual(TEXT("Easting maps to X."), Resolution.WorldLocation.X, 15000.0);
	TestEqual(TEXT("Northing mirrors into Y."), Resolution.WorldLocation.Y, -10000.0);
	TestEqual(TEXT("Height maps to Z."), Resolution.WorldLocation.Z, 500.0);

	// A CRS that does not match the canonical bundle is refused rather than
	// reinterpreted in the wrong reference system.
	Overlay.Anchor.CanonicalCrs = TEXT("EPSG:4326");
	TestFalse(TEXT("A mismatched CRS fails closed."),
		ProjectWorldAuthoredOverlay::Resolve(Bundle, Set, Overlay, Resolution, Error));

	Overlay.Anchor.CanonicalCrs = Bundle.CanonicalCrs;
	Overlay.Anchor.VerticalDatum = TEXT("LOCAL:ELLIPSOID");
	TestFalse(TEXT("A mismatched vertical datum fails closed."),
		ProjectWorldAuthoredOverlay::Resolve(Bundle, Set, Overlay, Resolution, Error));

	Overlay.Anchor.VerticalMode = EProjectWorldVerticalMode::SurfaceSnap;
	Overlay.Anchor.VerticalProvenanceRef.Reset();
	Overlay.Anchor.VerticalDatum.Reset();
	Overlay.Anchor.HeightMeters = 0.0;
	Overlay.Anchor.VerticalToleranceMeters = 1.0;
	Overlay.Anchor.OffsetUpMeters = 1.0;
	TestTrue(TEXT("A coordinate anchor can snap to accepted canonical terrain."),
		ProjectWorldAuthoredOverlay::Resolve(Bundle, Set, Overlay, Resolution, Error));
	TestEqual(TEXT("Surface snap applies its up offset after terrain sampling."), Resolution.WorldLocation.Z, 2600.0);
	TestTrue(TEXT("Surface snap is explicit in its resolution receipt."), Resolution.bSurfaceSnapped);
	TestEqual(TEXT("Resolution exposes horizontal provenance."), Resolution.HorizontalProvenanceId, TEXT("fixture_control"));
	TestEqual(TEXT("Resolution derives vertical provenance from terrain."), Resolution.VerticalProvenanceId, TEXT("fixture_terrain"));
	TestEqual(TEXT("Resolution exposes source vertical accuracy once."), Resolution.VerticalSourceAccuracyMeters, 0.5);
	TestEqual(TEXT("Resolution exposes vertical resolver residual."), Resolution.VerticalResolverErrorMeters, 0.05);
	TestEqual(TEXT("Resolution sums vertical error explicitly."), Resolution.VerticalTotalErrorMeters, 0.55);
	TestEqual(TEXT("Resolution includes canonical XY quantization."), Resolution.HorizontalTotalErrorMeters, 0.26);
	Overlay.Anchor.HorizontalToleranceMeters = 0.25;
	TestFalse(TEXT("Canonical XY quantization can exhaust the horizontal budget."),
		ProjectWorldAuthoredOverlay::Resolve(Bundle, Set, Overlay, Resolution, Error));
	Overlay.Anchor.HorizontalToleranceMeters = 1.0;
	Overlay.Anchor.VerticalToleranceMeters = 0.54;
	TestFalse(TEXT("Terrain sampling residual can exhaust the vertical budget."),
		ProjectWorldAuthoredOverlay::Resolve(Bundle, Set, Overlay, Resolution, Error));
	Overlay.Anchor.VerticalToleranceMeters = 1.0;

	FProjectWorldCanonicalBundle MissingTerrainProvenance = Bundle;
	MissingTerrainProvenance.Cells[0].Terrain.VerticalProvenanceId.Reset();
	TestFalse(TEXT("Surface snap without qualified terrain provenance fails closed."),
		ProjectWorldAuthoredOverlay::Resolve(
			MissingTerrainProvenance, Set, Overlay, Resolution, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldAuthoredOverlayKazanSurvivalTest,
	"ProjectIntegrationTests.ProjectWorld.AuthoredOverlay.KazanSurvival",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProjectWorldAuthoredOverlayKazanSurvivalTest::RunTest(const FString& Parameters)
{
	const FString ProfilePath = FPaths::Combine(
		FPaths::ProjectPluginsDir(),
		TEXT("World/ProjectWorldData/Data/Authored/kazan_territory_survival_v1.json"));
	FProjectWorldAuthoredOverlaySet Set;
	FString ErrorCode;
	FString Error;
	TestTrue(TEXT("The Kazan survival overlay profile loads."),
		ProjectWorldAuthoredOverlay::Load(ProfilePath, Set, ErrorCode, Error));
	TestEqual(TEXT("The profile preserves three controls plus two scenario anchors."),
		Set.Overlays.Num(), 5);

	const TMap<FString, FString> ExpectedPackages = {
		{TEXT("survival_cache"),
			TEXT("/ProjectWorldData/Authored/Survival/L_ProjectWorldKazanSurvivalCache")},
		{TEXT("survival_shelter"),
			TEXT("/ProjectWorldData/Authored/Survival/L_ProjectWorldKazanSurvivalShelter")}
	};
	for (const TPair<FString, FString>& Expected : ExpectedPackages)
	{
		int32 MatchCount = 0;
		for (const FProjectWorldAuthoredOverlay& Overlay : Set.Overlays)
		{
			if (Overlay.OverlayId == Expected.Key && Overlay.AuthoredPackage == Expected.Value)
			{
				++MatchCount;
			}
		}
		TestEqual(*FString::Printf(TEXT("%s has one canonical anchor."), *Expected.Key),
			MatchCount, 1);
		TestTrue(*FString::Printf(TEXT("%s authored package exists."), *Expected.Key),
			FPackageName::DoesPackageExist(Expected.Value));
	}
	return true;
}

/**
 * Feature anchors are the dangerous class: they depend on geography that
 * regeneration is allowed to move. They must resolve through the resolver,
 * and fail CLOSED when the feature vanishes or drifts too far - never
 * silently re-anchor to whatever is nearby.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldAuthoredOverlayFeatureTest,
	"Project.World.Authored.Anchor.Feature",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldAuthoredOverlayFeatureTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldAuthoredOverlayTests;
	const FProjectWorldAuthoredOverlaySet Set = MakeSet();
	FString Error;

	{
		const FProjectWorldCanonicalBundle Bundle = MakeBundle();
		FProjectWorldAnchorResolution Resolution;
		TestTrue(TEXT("A feature anchor resolves against its canonical feature."),
			ProjectWorldAuthoredOverlay::Resolve(Bundle, Set, MakeFeatureOverlay(1.0), Resolution, Error));
		TestEqual(TEXT("No drift when the feature is unchanged."), Resolution.DriftMeters, 0.0);
		TestEqual(TEXT("Resolved to the authored canonical point."), Resolution.WorldLocation.X, 10000.0);
		TestEqual(TEXT("Feature anchor snaps to canonical terrain."), Resolution.WorldLocation.Z, 2500.0);
	}

	{
		// The feature moved 4 m perpendicular to itself. Inside tolerance it
		// re-resolves and reports the drift; outside tolerance it must refuse.
		FProjectWorldCanonicalBundle Moved = MakeBundle();
		FProjectWorldCanonicalFeature& Feature = Moved.Features.FindChecked(TEXT("alis:osm:way:1151612452"));
		Feature.GeometryParts[0] = {FVector2D(1000.0, 2004.0), FVector2D(1200.0, 2004.0)};

		FProjectWorldAnchorResolution Resolution;
		TestTrue(TEXT("Movement inside tolerance still resolves."),
			ProjectWorldAuthoredOverlay::Resolve(Moved, Set, MakeFeatureOverlay(5.0), Resolution, Error));
		TestEqual(TEXT("Reported drift is the real distance."), Resolution.DriftMeters, 4.0);

		TestFalse(TEXT("Movement beyond tolerance fails closed."),
			ProjectWorldAuthoredOverlay::Resolve(Moved, Set, MakeFeatureOverlay(1.0), Resolution, Error));
		TestTrue(TEXT("The refusal names the placement-budget breach."), Error.Contains(TEXT("error budget")));
	}

	{
		// Additive profile growth may expose a much longer part for the same
		// feature. The authored point remains the locator and must not jump to
		// the new part's midpoint.
		FProjectWorldCanonicalBundle Expanded = MakeBundle();
		FProjectWorldCanonicalFeature& Feature = Expanded.Features.FindChecked(TEXT("alis:osm:way:1151612452"));
		Feature.GeometryParts.Add({FVector2D(5000.0, 3000.0), FVector2D(7000.0, 3000.0)});

		FProjectWorldAnchorResolution Resolution;
		TestTrue(TEXT("Additive feature growth preserves the anchor."),
			ProjectWorldAuthoredOverlay::Resolve(Expanded, Set, MakeFeatureOverlay(1.0), Resolution, Error));
		TestEqual(TEXT("Profile growth introduces no anchor drift."), Resolution.DriftMeters, 0.0);
		TestEqual(TEXT("Profile growth keeps the authored location."), Resolution.WorldLocation.X, 10000.0);
	}

	{
		// The anchored feature disappeared entirely.
		const FProjectWorldCanonicalBundle Empty = MakeBundle(false);
		FProjectWorldAnchorResolution Resolution;
		TestFalse(TEXT("A vanished feature fails closed."),
			ProjectWorldAuthoredOverlay::Resolve(Empty, Set, MakeFeatureOverlay(5.0), Resolution, Error));
		TestTrue(TEXT("The refusal names the missing feature."), Error.Contains(TEXT("no longer exists")));
	}

	{
		FProjectWorldCanonicalBundle Reclassified = MakeBundle();
		Reclassified.Features.FindChecked(TEXT("alis:osm:way:1151612452")).FeatureClass = TEXT("water");
		FProjectWorldAnchorResolution Resolution;
		TestFalse(TEXT("A changed feature class fails closed."),
			ProjectWorldAuthoredOverlay::Resolve(Reclassified, Set, MakeFeatureOverlay(1.0), Resolution, Error));
		TestTrue(TEXT("The refusal names the class change."), Error.Contains(TEXT("changed class")));
	}

	{
		FProjectWorldCanonicalBundle Retyped = MakeBundle();
		Retyped.Features.FindChecked(TEXT("alis:osm:way:1151612452")).GeometryType = TEXT("Polygon");
		FProjectWorldAnchorResolution Resolution;
		TestFalse(TEXT("A changed geometry type fails closed."),
			ProjectWorldAuthoredOverlay::Resolve(Retyped, Set, MakeFeatureOverlay(1.0), Resolution, Error));
		TestTrue(TEXT("The refusal names the geometry-type change."), Error.Contains(TEXT("geometry type")));
	}

	{
		// An overlay set authored against another grid must never be applied.
		FProjectWorldAuthoredOverlaySet OtherGrid = MakeSet();
		OtherGrid.GridId = TEXT("grid_0000000000000000");
		const FProjectWorldCanonicalBundle Bundle = MakeBundle();
		FProjectWorldAnchorResolution Resolution;
		TestFalse(TEXT("A foreign grid fails closed."),
			ProjectWorldAuthoredOverlay::Resolve(Bundle, OtherGrid, MakeFeatureOverlay(1.0), Resolution, Error));
	}
	{
		FProjectWorldAuthoredOverlaySet OtherOwner = MakeSet();
		OtherOwner.WorldDataPluginName = TEXT("ProjectWorldData");
		const FProjectWorldCanonicalBundle Bundle = MakeBundle();
		FProjectWorldAnchorResolution Resolution;
		TestFalse(TEXT("A foreign world-data owner fails closed."),
			ProjectWorldAuthoredOverlay::Resolve(Bundle, OtherOwner, MakeFeatureOverlay(1.0), Resolution, Error));
	}
	return true;
}

/**
 * The loader is the contract's enforcement point: authored content may not
 * live under a generated root, and a resolver whose semantics changed must
 * not silently re-place authored work.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldAuthoredOverlayContractTest,
	"Project.World.Authored.Anchor.Contract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldAuthoredOverlayActorNoOpTest,
	"Project.World.Authored.Anchor.ActorNoOp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldAuthoredOverlayActorNoOpTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldAuthoredOverlayTests;
	const FProjectWorldCanonicalBundle Bundle = MakeBundle();
	FProjectWorldAuthoredOverlaySet Set = MakeSet();
	Set.SetHash = TEXT("5e639e08c851f4f49ffa275cff545dad666f6f4b6f0db3e1130cc4edbe7da830");
	FProjectWorldAuthoredAnchorEvidence Evidence;
	Evidence.OverlayId = TEXT("marker");
	Evidence.AuthoredPackage = TEXT("/ProjectWorldTestData/Authored/Fixtures/L_ProjectWorldMarker");
	Evidence.WorldLocation = FVector(100.0, 200.0, 300.0);
	Evidence.WorldRotation = FRotator(0.0, 15.0, 0.0);
	Evidence.bPlaces = true;

	UWorld* World = GEditor->NewMap(false);
	FProjectWorldRealizationResult FirstResult;
	FirstResult.AuthoredAnchors.Add(Evidence);
	FString Error;
	TestTrue(
		TEXT("The real authored-overlay path creates its stable Level Instance."),
		ProjectWorldAuthoredOverlayRealization::Apply(World, Bundle, Set, FirstResult, Error));
	TestEqual(TEXT("First realization creates one authored anchor actor."), FirstResult.CreatedActorCount, 1);
	ALevelInstance* CreatedActor = Cast<ALevelInstance>(
		FindObject<AActor>(World->PersistentLevel, TEXT("ProjectWorld_AuthoredOverlay_marker")));
	TestNotNull(TEXT("The created authored anchor remains discoverable."), CreatedActor);
	TestTrue(TEXT("The created authored anchor uses persistent OFPA ownership."),
		CreatedActor != nullptr && CreatedActor->IsPackageExternal());

	FProjectWorldRealizationResult UnchangedResult;
	UnchangedResult.AuthoredAnchors.Add(Evidence);
	TestTrue(
		TEXT("The same authored-overlay contract is accepted again."),
		ProjectWorldAuthoredOverlayRealization::Apply(World, Bundle, Set, UnchangedResult, Error));
	TestEqual(TEXT("Unchanged authored-overlay identity updates no actor."), UnchangedResult.UpdatedActorCount, 0);
	TestEqual(TEXT("Unchanged authored-overlay identity preserves its actor."), UnchangedResult.PreservedActorCount, 1);
	return true;
}

bool FProjectWorldAuthoredOverlayContractTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldSchemaTestUtilities;
	const FString Root = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/ProjectWorldAuthored"));
	IFileManager::Get().MakeDirectory(*Root, true);
	const FString Path = FPaths::Combine(Root, TEXT("overlay.json"));

	FString Valid = TEXT(R"({
  "$schema": "../Schemas/project_world_authored_overlay.schema.json",
  "schema_version": 1,
  "overlay_set_id": "fixture",
  "world_data_plugin": "ProjectWorldTestData",
  "grid_id": "grid_413718bc833994e5",
  "resolver_version": 3,
  "provenance": [{
    "provenance_id": "fixture_control",
    "horizontal_accuracy_m": 0.25
  }, {
    "provenance_id": "fixture_height_control",
    "vertical_datum": "EPSG:3855",
    "vertical_accuracy_m": 0.5
  }],
  "overlays": [
    {
      "overlay_id": "plaza_marker",
      "authored_package": "/ProjectWorldTestData/Authored/Fixtures/L_PlazaMarker",
      "anchor": {
        "kind": "coordinate",
        "placement_class": "precision",
        "provenance_ref": "fixture_control",
        "horizontal_tolerance_m": 1.0,
        "canonical_crs": "EPSG:32639",
        "easting_m": 380200.0,
        "northing_m": 6184600.0,
        "vertical_mode": "absolute",
        "vertical_provenance_ref": "fixture_height_control",
        "vertical_datum": "EPSG:3855",
        "height_m": 60.0,
        "vertical_tolerance_m": 1.0
      }
    }
  ]
})");

	FString Feature = TEXT(R"({
  "$schema": "../Schemas/project_world_authored_overlay.schema.json",
  "schema_version": 1,
  "overlay_set_id": "fixture",
  "world_data_plugin": "ProjectWorldTestData",
  "grid_id": "grid_413718bc833994e5",
  "resolver_version": 3,
  "provenance": [{
    "provenance_id": "fixture_control",
    "horizontal_accuracy_m": 0.25
  }],
  "overlays": [{
    "overlay_id": "road_marker",
    "authored_package": "/ProjectWorldTestData/Authored/Fixtures/L_RoadMarker",
    "anchor": {
      "kind": "feature",
      "placement_class": "precision",
      "provenance_ref": "fixture_control",
      "horizontal_tolerance_m": 1.0,
      "vertical_mode": "surface_snap",
      "vertical_tolerance_m": 1.0,
      "feature_id": "alis:osm:way:1151612452",
      "expected_feature_class": "road",
      "expected_geometry_type": "LineString",
      "expected_easting_m": 1100.0,
      "expected_northing_m": 2000.0
    }
  }]
})");

	FString Mask = TEXT(R"({
  "$schema": "../Schemas/project_world_authored_overlay.schema.json",
  "schema_version": 1,
  "overlay_set_id": "fixture",
  "world_data_plugin": "ProjectWorldTestData",
  "grid_id": "grid_413718bc833994e5",
  "resolver_version": 3,
  "provenance": [{
    "provenance_id": "fixture_control",
    "horizontal_accuracy_m": 0.25
  }],
  "overlays": [{
    "overlay_id": "plaza_mask",
    "authored_package": "/ProjectWorldTestData/Authored/Fixtures/L_PlazaMask",
    "anchor": {
      "kind": "mask",
      "canonical_crs": "EPSG:32639",
      "bounds_m": [1000.0, 2000.0, 1100.0, 2100.0],
      "excludes": ["vegetation"]
    }
  }]
})");
	Valid = Rewrite(Valid, Path, TEXT("project_world_authored_overlay.schema.json"));
	Feature = Rewrite(Feature, Path, TEXT("project_world_authored_overlay.schema.json"));
	Mask = Rewrite(Mask, Path, TEXT("project_world_authored_overlay.schema.json"));

	FProjectWorldAuthoredOverlaySet Set;
	FString ErrorCode;
	FString Error;
	auto ExpectRejected = [this, &Path, &ErrorCode, &Error](
		const TCHAR* Label,
		const FString& Source)
	{
		FProjectWorldAuthoredOverlaySet Rejected;
		TestTrue(TEXT("Rejected fixture is writable."), FFileHelper::SaveStringToFile(Source, *Path));
		TestFalse(Label, ProjectWorldAuthoredOverlay::Load(Path, Rejected, ErrorCode, Error));
	};
	TestTrue(TEXT("Fixture is writable."), FFileHelper::SaveStringToFile(Valid, *Path));
	TestTrue(TEXT("A conforming overlay set loads."),
		ProjectWorldAuthoredOverlay::Load(Path, Set, ErrorCode, Error));
	TestEqual(TEXT("Overlay count."), Set.Overlays.Num(), 1);
	TestEqual(TEXT("Set hash is a SHA-256."), Set.SetHash.Len(), 64);

	TestTrue(TEXT("Fixture remains writable."), FFileHelper::SaveStringToFile(Valid, *Path));
	TestTrue(TEXT("Reloading into the same output succeeds."),
		ProjectWorldAuthoredOverlay::Load(Path, Set, ErrorCode, Error));
	TestEqual(TEXT("Reloading replaces instead of appending overlays."), Set.Overlays.Num(), 1);

	const FString ProductionRoot = FPaths::Combine(
		FPaths::ProjectPluginsDir(),
		TEXT("World/ProjectWorldData/Data/Profiles"));
	const FString ProductionPath = FPaths::Combine(ProductionRoot, TEXT("authored_overlay_loader_test.json"));
	const FString ProductionSchema = ReferenceFor(
		ProductionPath,
		TEXT("project_world_authored_overlay.schema.json"));
	TestEqual(
		TEXT("Production authored schema climbs to the canonical logic plugin."),
		ProductionSchema,
		FString(TEXT("../../../ProjectWorld/Data/Schemas/project_world_authored_overlay.schema.json")));
	const FString ProductionOwned = Valid
		.Replace(
			*ReferenceFor(Path, TEXT("project_world_authored_overlay.schema.json")),
			*ProductionSchema,
			ESearchCase::CaseSensitive)
		.Replace(TEXT("\"world_data_plugin\": \"ProjectWorldTestData\""),
			TEXT("\"world_data_plugin\": \"ProjectWorldData\""))
		.Replace(TEXT("/ProjectWorldTestData/Authored/"), TEXT("/ProjectWorldData/Authored/"));
	IFileManager::Get().MakeDirectory(*ProductionRoot, true);
	TestTrue(TEXT("Production-root fixture is writable."),
		FFileHelper::SaveStringToFile(ProductionOwned, *ProductionPath));
	TestTrue(TEXT("The same loader accepts the descriptor-derived production root."),
		ProjectWorldAuthoredOverlay::Load(ProductionPath, Set, ErrorCode, Error));

	// Authored content under a generated root would be destroyed by the very
	// regeneration these anchors exist to survive.
	const FString Escaped = Valid.Replace(
		TEXT("/ProjectWorldTestData/Authored/Fixtures/L_PlazaMarker"),
		TEXT("/ProjectWorldTestData/Generated/P0/L_PlazaMarker"));
	TestTrue(TEXT("Escaped fixture is writable."), FFileHelper::SaveStringToFile(Escaped, *Path));
	FProjectWorldAuthoredOverlaySet Rejected;
	TestFalse(TEXT("Authored content may not live under a generated root."),
		ProjectWorldAuthoredOverlay::Load(Path, Rejected, ErrorCode, Error));

	ExpectRejected(TEXT("Overlay-set ids enforce the schema token grammar."),
		Valid.Replace(TEXT("\"overlay_set_id\": \"fixture\""), TEXT("\"overlay_set_id\": \"Fixture\"")));
	ExpectRejected(TEXT("Grid ids require exactly 16 lowercase hexadecimal digits."),
		Valid.Replace(TEXT("grid_413718bc833994e5"), TEXT("grid_413718BC833994e5")));
	ExpectRejected(TEXT("Overlay ids enforce the schema token grammar."),
		Valid.Replace(TEXT("\"overlay_id\": \"plaza_marker\""), TEXT("\"overlay_id\": \"plaza-marker\"")));
	ExpectRejected(TEXT("Authored package paths enforce the full schema grammar."),
		Valid.Replace(TEXT("L_PlazaMarker"), TEXT("L-PlazaMarker")));
	ExpectRejected(TEXT("Yaw outside the schema range is rejected."),
		Valid.Replace(TEXT("\"height_m\": 60.0"),
			TEXT("\"height_m\": 60.0, \"orientation\": {\"yaw_degrees\": 361.0}")));
	ExpectRejected(TEXT("Feature anchors require the expected canonical point."),
		Feature.Replace(TEXT("      \"expected_easting_m\": 1100.0,\n"), TEXT("")));
	ExpectRejected(TEXT("Feature anchors require the expected feature class."),
		Feature.Replace(TEXT("      \"expected_feature_class\": \"road\",\n"), TEXT("")));
	ExpectRejected(TEXT("Feature ids may not be empty."),
		Feature.Replace(TEXT("alis:osm:way:1151612452"), TEXT("")));
	ExpectRejected(TEXT("Mask exclusions enforce the frozen enum."),
		Mask.Replace(TEXT("\"vegetation\""), TEXT("\"water\"")));
	ExpectRejected(TEXT("Mask bounds reject non-numeric values."),
		Mask.Replace(TEXT("1000.0"), TEXT("\"west\"")));
	ExpectRejected(TEXT("A declared horizontal accuracy must be numeric."),
		Valid.Replace(TEXT("\"horizontal_accuracy_m\": 0.25"),
			TEXT("\"horizontal_accuracy_m\": \"bad\"")));
	ExpectRejected(TEXT("A declared vertical datum must be a string."),
		Valid.Replace(TEXT("\"vertical_datum\": \"EPSG:3855\""),
			TEXT("\"vertical_datum\": 3855")));
	ExpectRejected(TEXT("A declared vertical accuracy must be numeric."),
		Valid.Replace(TEXT("\"vertical_accuracy_m\": 0.5"),
			TEXT("\"vertical_accuracy_m\": \"bad\"")));

	const FString PartialFailure = Valid.Replace(TEXT("  ]\n}"), TEXT(R"(  ,
    {
      "overlay_id": "bad_path",
      "authored_package": "/ProjectWorldTestData/Generated/Bad",
      "anchor": {
        "kind": "coordinate",
        "placement_class": "precision",
        "provenance_ref": "fixture_control",
        "horizontal_tolerance_m": 1.0,
        "canonical_crs": "EPSG:32639",
        "easting_m": 0.0,
        "northing_m": 0.0,
        "vertical_mode": "absolute",
        "vertical_provenance_ref": "fixture_height_control",
        "vertical_datum": "EPSG:3855",
        "height_m": 0.0,
        "vertical_tolerance_m": 1.0
      }
    }
  ]
})"));
	TestTrue(TEXT("Partial-failure fixture is writable."),
		FFileHelper::SaveStringToFile(PartialFailure, *Path));
	TestFalse(TEXT("A late validation failure rejects the whole document."),
		ProjectWorldAuthoredOverlay::Load(Path, Set, ErrorCode, Error));
	TestEqual(TEXT("Rejected loads leave the prior output unchanged."), Set.Overlays.Num(), 1);

	// A future resolver must re-accept authored placement explicitly.
	const FString FutureResolver = Valid.Replace(TEXT("\"resolver_version\": 3"), TEXT("\"resolver_version\": 4"));
	TestTrue(TEXT("Future-resolver fixture is writable."), FFileHelper::SaveStringToFile(FutureResolver, *Path));
	TestFalse(TEXT("An unsupported resolver version fails closed."),
		ProjectWorldAuthoredOverlay::Load(Path, Rejected, ErrorCode, Error));
	TestEqual(TEXT("Resolver rejection is structured."), ErrorCode, FString(TEXT("authored-overlay-resolver")));

	IFileManager::Get().Delete(*Path, false, true, true);
	IFileManager::Get().Delete(*ProductionPath, false, true, true);
	return true;
}

REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FProjectWorldAuthoredOverlayCoordinateTest,
	"Project.World.Authored.Anchor.Coordinate",
	"[Fast][Unit][World]")
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FProjectWorldAuthoredOverlayFeatureTest,
	"Project.World.Authored.Anchor.Feature",
	"[Fast][Unit][World]")
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FProjectWorldAuthoredOverlayContractTest,
	"Project.World.Authored.Anchor.Contract",
	"[Fast][Integration][World]")
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FProjectWorldAuthoredOverlayActorNoOpTest,
	"Project.World.Authored.Anchor.ActorNoOp",
	"[Fast][Integration][World]")
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FProjectWorldAuthoredOverlayKazanSurvivalTest,
	"Project.World.Authored.Anchor.KazanSurvival",
	"[Fast][Integration][World]")

#endif
