// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldDataRoots.h"
#include "ProjectWorldGeneratedGeometry.h"
#include "ProjectWorldRealizationService.h"
#include "ProjectWorldRealizeCommandlet.h"
#include "ProjectWorldSavePolicy.h"

#include "Editor.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldSavePolicyTest,
	"Project.World.Realization.SavePolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldSavePolicyTest::RunTest(const FString& Parameters)
{
	FProjectWorldRealizationResult Result;
	TestFalse(TEXT("A no-op needs no broad save."),
		ProjectWorldSavePolicy::RequiresBroadWorldSave(Result, true, false));
	Result.CreatedActorCount = 1;
	Result.SelfSavedActorMutationCount = 1;
	TestFalse(TEXT("A self-saved road-only change on an existing map needs no broad save."),
		ProjectWorldSavePolicy::RequiresBroadWorldSave(Result, true, false));
	TestTrue(TEXT("A new map still needs a broad save."),
		ProjectWorldSavePolicy::RequiresBroadWorldSave(Result, false, false));
	Result.UpdatedActorCount = 1;
	TestTrue(TEXT("A mixed actor change needs a broad save."),
		ProjectWorldSavePolicy::RequiresBroadWorldSave(Result, true, false));
	Result.UpdatedActorCount = 0;
	TestTrue(TEXT("A partition policy change needs a broad save."),
		ProjectWorldSavePolicy::RequiresBroadWorldSave(Result, true, true));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldDataRootsTest,
	"Project.World.Realization.WorldDataRoots",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldDataRootsTest::RunTest(const FString& Parameters)
{
	FProjectWorldDataRoots FixtureRoots;
	FProjectWorldDataRoots ProductionRoots;
	FString Error;
	TestTrue(TEXT("Fixture roots resolve from the ProjectWorldTestData plugin descriptor."),
		FProjectWorldDataRoots::Resolve(TEXT("ProjectWorldTestData"), FixtureRoots, Error));
	TestTrue(TEXT("Production roots resolve from the ProjectWorldData plugin descriptor."),
		FProjectWorldDataRoots::Resolve(TEXT("ProjectWorldData"), ProductionRoots, Error));
	TestEqual(TEXT("Fixture generated mount is derived from its data owner."),
		FixtureRoots.GeneratedPackageRoot, FString(TEXT("/ProjectWorldTestData/Generated/")));
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
	const FVector East = FProjectWorldCanonicalLoader::CanonicalToUnreal(
		Bundle, Canonical + FVector(1.0, 0.0, 0.0));
	const FVector North = FProjectWorldCanonicalLoader::CanonicalToUnreal(
		Bundle, Canonical + FVector(0.0, 1.0, 0.0));
	const FVector Up = FProjectWorldCanonicalLoader::CanonicalToUnreal(
		Bundle, Canonical + FVector(0.0, 0.0, 1.0));
	TestTrue(TEXT("One canonical metre east is exactly +100 UE centimetres."),
		(East - Unreal).Equals(FVector(100.0, 0.0, 0.0), 0.000001));
	TestTrue(TEXT("One canonical metre north is exactly -100 UE centimetres."),
		(North - Unreal).Equals(FVector(0.0, -100.0, 0.0), 0.000001));
	TestTrue(TEXT("One canonical vertical metre is exactly +100 UE centimetres."),
		(Up - Unreal).Equals(FVector(0.0, 0.0, 100.0), 0.000001));
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
	FProjectWorldGeoReferencingPlacementTest,
	"Project.World.Realization.GeoReferencingMatchesPlacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldGeoReferencingPlacementTest::RunTest(const FString& Parameters)
{
	FProjectWorldCanonicalBundle Bundle;
	Bundle.GridId = TEXT("georeferencing_noop_test");
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

	UWorld* World = GEditor->NewMap(false);
	FProjectWorldRealizationResult Result;
	FString Error;
	const double ErrorMeters = ProjectWorldGeneratedGeometry::MeasureCoordinateRoundTrip(
		World, Bundle, true, Result, Error);
	TestTrue(TEXT("GeoReferencing reverse round trip stays inside tolerance."), ErrorMeters <= 0.01);
	TestTrue(
		TEXT("GeoReferencing placement equals the canonical actor transform."),
		Result.GeoReferencingPlacementErrorMeters <= 0.01);
	TestTrue(TEXT("Both cell corners and shared edge are probed."), Result.GeoReferencingProbePointCount >= 8);
	TestEqual(TEXT("The first persistent probe creates one GeoReferencing actor."), Result.CreatedActorCount, 1);

	FProjectWorldRealizationResult UnchangedResult;
	ProjectWorldGeneratedGeometry::MeasureCoordinateRoundTrip(
		World, Bundle, true, UnchangedResult, Error);
	TestEqual(TEXT("An unchanged probe updates no GeoReferencing actor."), UnchangedResult.UpdatedActorCount, 0);
	TestEqual(TEXT("An unchanged probe preserves the GeoReferencing actor."), UnchangedResult.PreservedActorCount, 1);
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
		ProjectWorldGeneratedGeometry::RemoveOwnedActors(World, Result));
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
	const FString MapPackage = TEXT("/ProjectWorldTestData/Generated/P0/L_ProjectWorldSynthetic");
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
			TEXT("-CompileResult=\"%s\" -Result=\"%s\" -Map=\"%s\" -Mode=delete"),
			*MissingCompile,
			*UnsafeResult,
			*MapPackage)),
		2);
	TestFalse(TEXT("Unsafe commandlet evidence is not emitted."), IFileManager::Get().FileExists(*UnsafeResult));
	TestEqual(
		TEXT("Delete reaches canonical validation without a presentation profile."),
		Commandlet->Main(FString::Printf(
			TEXT("-CompileResult=\"%s\" -Result=\"%s\" -Map=\"%s\" -Mode=delete"),
			*MissingCompile,
			*SafeResult,
			*MapPackage)),
		4);
	TestTrue(TEXT("Safe commandlet evidence is emitted."), IFileManager::Get().FileExists(*SafeResult));
	IFileManager::Get().Delete(*SafeResult, false, true, true);
	return true;
}

#endif
