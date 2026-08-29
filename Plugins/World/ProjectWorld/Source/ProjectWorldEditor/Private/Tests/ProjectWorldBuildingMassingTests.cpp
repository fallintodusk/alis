// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldAuthoredOverlay.h"
#include "ProjectWorldBuildingMeshBuilder.h"
#include "ProjectWorldBuildingRealization.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldRealizationProfile.h"
#include "ProjectWorldRealizationService.h"

#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Materials/MaterialInterface.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PackageTools.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshAttributes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ProjectWorldBuildingMassingTests
{
	FProjectWorldCanonicalPolygon Square(double MinimumX, double MinimumY, double MaximumX, double MaximumY)
	{
		FProjectWorldCanonicalPolygon Polygon;
		Polygon.Outer = {
			FVector2D(MinimumX, MinimumY), FVector2D(MaximumX, MinimumY),
			FVector2D(MaximumX, MaximumY), FVector2D(MinimumX, MaximumY),
			FVector2D(MinimumX, MinimumY)};
		return Polygon;
	}

	FProjectWorldCanonicalFeature Building(
		const FString& Id,
		const FString& OwnerCellId,
		const FProjectWorldCanonicalPolygon& Polygon,
		double Height = 12.0)
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

	FProjectWorldCanonicalCell Cell(const FString& Id, double MinimumX, double MaximumX)
	{
		FProjectWorldCanonicalCell Result;
		Result.CellId = Id;
		Result.Bounds = FVector4d(MinimumX, 0.0, MaximumX, 100.0);
		Result.Terrain.ArtifactHash = FString::ChrN(64, Id.EndsWith(TEXT("a")) ? TEXT('a') : TEXT('b'));
		Result.Terrain.Bounds = Result.Bounds;
		Result.Terrain.SampleSpacing = FVector2D(100.0, 100.0);
		Result.Terrain.SamplesX = 2;
		Result.Terrain.SamplesY = 2;
		Result.Terrain.HeightsMeters = {2.0, 2.0, 2.0, 2.0};
		return Result;
	}

	FProjectWorldRealizationLayer Layer()
	{
		FProjectWorldRealizationLayer Result;
		Result.LayerId = TEXT("buildings");
		Result.LayerKind = EProjectWorldLayerKind::GeneratedGeography;
		Result.GeneratorId = TEXT("project_building_massing");
		Result.GeneratorVersion = 1;
	Result.CanonicalSelectors = {TEXT("buildings")};
	Result.ArtifactRoot = TEXT("/ProjectWorldTestData/Generated/BuildingMassingTest/");
	Result.SpatialOwnership = TEXT("cell_local");
	Result.DirtyGranularity = EProjectWorldDirtyGranularity::CanonicalCell;
	Result.RuntimeMapping = TEXT("world_partition_spatial");
		Result.ContractHash = FString::ChrN(64, TEXT('c'));
		Result.NormalizedSettings = TEXT(
			"{\"collision\":\"complex_as_simple\",\"conflict_policy\":\"reject_affected_fragments\"," 
			"\"contained_policy\":\"associate_with_container\",\"duplicate_policy\":\"stable_feature_id\"," 
			"\"maximum_height_m\":300,\"nanite\":true,\"navigation\":\"no_navigation\"," 
			"\"terrain_anchor_policy\":\"owner_cell_clamped_bounds_center\"," 
			"\"topology_policy\":\"cell_local_classify_v1\"}");
		return Result;
	}

	struct FWallQueryEvidence
	{
		bool bOutsideToInsideCapsuleBlocked = false;
		bool bInsideToOutsideCapsuleBlocked = false;
		bool bOutsideToInsideRayBlocked = false;
		bool bInsideToOutsideRayBlocked = false;
	};

	FWallQueryEvidence QueryMinimumXWall(UWorld& World, UStaticMeshComponent& Component)
	{
		constexpr float CharacterCapsuleRadius = 23.0f;
		constexpr float CharacterCapsuleHalfHeight = 88.0f;
		Component.UpdateBounds();
		const FBox Bounds = Component.Bounds.GetBox();
		const FVector Inside = Bounds.GetCenter();
		const FVector Outside(
			Bounds.Min.X - CharacterCapsuleRadius - 100.0,
			Inside.Y,
			Inside.Z);
		const FCollisionShape Capsule = FCollisionShape::MakeCapsule(
			CharacterCapsuleRadius,
			CharacterCapsuleHalfHeight);
		const FCollisionQueryParams CapsuleQueryParams(
			SCENE_QUERY_STAT(ProjectWorldBuildingPawnCapsuleCollision),
			false);
		const FCollisionQueryParams RayQueryParams(
			SCENE_QUERY_STAT(ProjectWorldBuildingDoubleSidedRayCollision),
			true);

		auto SweepHitsComponent = [&](const FVector& Start, const FVector& End)
		{
			FHitResult Hit;
			return World.SweepSingleByChannel(
				Hit,
				Start,
				End,
				FQuat::Identity,
				ECC_Pawn,
				Capsule,
				CapsuleQueryParams) && Hit.GetComponent() == &Component;
		};
		auto RayHitsComponent = [&](const FVector& Start, const FVector& End)
		{
			FHitResult Hit;
			return World.LineTraceSingleByChannel(
				Hit,
				Start,
				End,
				ECC_Pawn,
				RayQueryParams) && Hit.GetComponent() == &Component;
		};

		FWallQueryEvidence Evidence;
		Evidence.bOutsideToInsideCapsuleBlocked = SweepHitsComponent(Outside, Inside);
		Evidence.bInsideToOutsideCapsuleBlocked = SweepHitsComponent(Inside, Outside);
		Evidence.bOutsideToInsideRayBlocked = RayHitsComponent(Outside, Inside);
		Evidence.bInsideToOutsideRayBlocked = RayHitsComponent(Inside, Outside);
		return Evidence;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldBuildingTopologyAdmissionTest,
	"Project.World.Realization.Buildings.TopologyAdmission",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldBuildingTopologyAdmissionTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldBuildingMassingTests;
	FProjectWorldCanonicalBundle Bundle;
	Bundle.GridId = TEXT("building_test");
	Bundle.CoordinateQuantizationMeters = 0.01;
	Bundle.HeightQuantizationMeters = 0.1;
	FProjectWorldCanonicalCell Target = Cell(TEXT("building_test:a"), 0.0, 100.0);

	FProjectWorldCanonicalFeature Primary = Building(
		TEXT("building/primary"), Target.CellId, Square(0.0, 0.0, 20.0, 20.0));
	FProjectWorldCanonicalPolygon Reordered = Square(0.0, 0.0, 20.0, 20.0);
	Reordered.Outer = {
		FVector2D(20.0, 20.0), FVector2D(20.0, 0.0), FVector2D(0.0, 0.0),
		FVector2D(0.0, 20.0), FVector2D(20.0, 20.0)};
	FProjectWorldCanonicalFeature Duplicate = Building(
		TEXT("building/duplicate"), Target.CellId, Reordered);
	FProjectWorldCanonicalFeature Contained = Building(
		TEXT("building/contained"), Target.CellId, Square(2.0, 2.0, 5.0, 5.0));
	FProjectWorldCanonicalFeature Conflict = Building(
		TEXT("building/conflict"), Target.CellId, Square(15.0, 0.0, 25.0, 20.0));
	FProjectWorldCanonicalPolygon BowTie;
	BowTie.Outer = {
		FVector2D(30.0, 0.0), FVector2D(40.0, 10.0), FVector2D(30.0, 10.0),
		FVector2D(40.0, 0.0), FVector2D(30.0, 0.0)};
	FProjectWorldCanonicalFeature Malformed = Building(
		TEXT("building/malformed"), Target.CellId, BowTie);
	FProjectWorldCanonicalFeature Masked = Building(
		TEXT("building/masked"), Target.CellId, Square(42.0, 0.0, 48.0, 10.0));
	FProjectWorldCanonicalFeature Complex = Building(
		TEXT("building/complex"), Target.CellId, Square(60.0, 0.0, 90.0, 30.0));
	Complex.GeometryType = TEXT("MultiPolygon");
	Complex.GeometryPolygons[0].Holes.Add(Square(70.0, 10.0, 80.0, 20.0).Outer);
	Complex.GeometryPolygons.Add(Square(92.0, 0.0, 98.0, 6.0));
	for (FProjectWorldCanonicalFeature* Feature : {
		&Primary, &Duplicate, &Contained, &Conflict, &Malformed, &Masked, &Complex})
	{
		Target.OwnedFeatureIds.Add(Feature->FeatureId);
		Bundle.Features.Add(Feature->FeatureId, *Feature);
	}
	Bundle.Cells.Add(Target);

	FProjectWorldAuthoredOverlaySet OverlaySet;
	FProjectWorldAuthoredOverlay& Mask = OverlaySet.Overlays.AddDefaulted_GetRef();
	Mask.OverlayId = TEXT("test/building-mask");
	Mask.Anchor.Kind = EProjectWorldAnchorKind::Mask;
	Mask.Anchor.BoundsMeters = FVector4d(40.0, 0.0, 50.0, 20.0);
	Mask.Anchor.Excludes = {TEXT("buildings")};
	FProjectWorldBuildingMeshBuildResult First;
	FProjectWorldBuildingMeshBuildResult Second;
	FString Error;
	const FProjectWorldBuildingSettings Settings;
	TestTrue(TEXT("The topology fixture builds."),
		ProjectWorldBuildingMeshBuilder::BuildCell(Bundle, Bundle.Cells[0], OverlaySet, Settings, First, Error));
	TestTrue(TEXT("The topology fixture rebuilds deterministically."),
		ProjectWorldBuildingMeshBuilder::BuildCell(Bundle, Bundle.Cells[0], OverlaySet, Settings, Second, Error));
	TestEqual(TEXT("All intersecting fragments are classified."), First.Stats.CandidateFragmentCount, 7);
	TestEqual(TEXT("Only the independent complex footprint is admitted."), First.Stats.AcceptedFragmentCount, 1);
	TestEqual(TEXT("Equivalent reordered geometry is a duplicate."), First.Stats.DuplicateFragmentCount, 1);
	TestEqual(TEXT("Contained geometry is associated, not rendered twice."), First.Stats.ContainedFragmentCount, 1);
	TestEqual(TEXT("Both unresolved overlap participants are rejected."), First.Stats.ConflictFragmentCount, 2);
	TestEqual(TEXT("Self-intersecting geometry is rejected."), First.Stats.MalformedFragmentCount, 1);
	TestEqual(TEXT("The authored exclusion mask removes one fragment."), First.Stats.AuthoredMaskExcludedFragmentCount, 1);
	TestTrue(TEXT("Multipolygon massing with a hole produces triangles."), First.TriangleCount > 0);
	TestEqual(TEXT("Mesh semantics are deterministic."), Second.SemanticDigest, First.SemanticDigest);
	TestEqual(TEXT("Triangle count is deterministic."), Second.TriangleCount, First.TriangleCount);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldBuildingDimensionsTest,
	"Project.World.Realization.Buildings.Dimensions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldBuildingDimensionsTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldBuildingMassingTests;
	for (const double HeightMeters : {2.0, 9.0, 27.0})
	{
		FProjectWorldCanonicalBundle Bundle;
		Bundle.GridId = FString::Printf(TEXT("building_dimensions_%.0f"), HeightMeters);
		Bundle.CoordinateQuantizationMeters = 0.01;
		Bundle.HeightQuantizationMeters = 0.1;
		Bundle.Cells.Add(Cell(Bundle.GridId + TEXT(":a"), 0.0, 100.0));
		const FProjectWorldCanonicalFeature Feature = Building(
			TEXT("building/dimensions"), Bundle.Cells[0].CellId,
			Square(10.0, 10.0, 20.0, 20.0), HeightMeters);
		Bundle.Cells[0].OwnedFeatureIds = {Feature.FeatureId};
		Bundle.Features.Add(Feature.FeatureId, Feature);

		FProjectWorldBuildingMeshBuildResult Build;
		FString Error;
		TestTrue(TEXT("The dimensional fixture builds through the production mesh builder."),
			ProjectWorldBuildingMeshBuilder::BuildCell(
				Bundle, Bundle.Cells[0], FProjectWorldAuthoredOverlaySet(),
				FProjectWorldBuildingSettings(), Build, Error));
		FStaticMeshAttributes Attributes(Build.MeshDescription);
		TVertexAttributesRef<FVector3f> Positions = Attributes.GetVertexPositions();
		FBox Bounds(ForceInit);
		for (const FVertexID Vertex : Build.MeshDescription.Vertices().GetElementIDs())
		{
			Bounds += FVector(Positions[Vertex]);
		}
		TestTrue(TEXT("A ten metre footprint is 1000 UE centimetres on X."),
			FMath::IsNearlyEqual(Bounds.GetSize().X, 1000.0, 0.01));
		TestTrue(TEXT("A ten metre footprint is 1000 UE centimetres on Y."),
			FMath::IsNearlyEqual(Bounds.GetSize().Y, 1000.0, 0.01));
		TestTrue(TEXT("Building Z extent equals canonical height in centimetres."),
			FMath::IsNearlyEqual(Bounds.GetSize().Z, HeightMeters * 100.0, 0.01));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldPersistentBuildingLayerTest,
	"Project.World.Realization.Buildings.PersistentLayer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldPersistentBuildingLayerTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldBuildingMassingTests;
	FProjectWorldCanonicalBundle Bundle;
	Bundle.GridId = TEXT("building_persistence_test");
	Bundle.CoordinateQuantizationMeters = 0.01;
	Bundle.HeightQuantizationMeters = 0.1;
	Bundle.Cells.Add(Cell(TEXT("building_persistence_test:a"), 0.0, 100.0));
	FProjectWorldCanonicalFeature Feature = Building(
		TEXT("building/persistent"), Bundle.Cells[0].CellId, Square(10.0, 10.0, 40.0, 40.0));
	Bundle.Cells[0].OwnedFeatureIds = {Feature.FeatureId};
	Bundle.Features.Add(Feature.FeatureId, Feature);
	FProjectWorldRealizationProfile Profile;
	Profile.Layers.Add(Layer());
	FProjectWorldRealizationResult Result;
	FProjectWorldLayerInventory& Inventory = Result.LayerInventories.AddDefaulted_GetRef();
	Inventory.LayerId = Profile.Layers[0].LayerId;
	Inventory.GeneratorId = Profile.Layers[0].GeneratorId;
	Inventory.FinalDirtyUnits = {TEXT("*")};
	FProjectWorldAuthoredOverlaySet OverlaySet;
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(
		nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	TestNotNull(TEXT("The engine test material is available."), Material);
	UWorld* World = GEditor->NewMap(true);
	FString Error;
	TestTrue(TEXT("The production building layer creates persistent cell output."),
		ProjectWorldBuildingRealization::Apply(
			World, Bundle, Profile, OverlaySet, Material, Result, Error));

	AStaticMeshActor* Actor = nullptr;
	FString InitialSemantic;
	for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
	{
		FString CellId;
		FString Semantic;
		if (ProjectWorldBuildingRealization::ReadActorIdentity(*It, CellId, Semantic))
		{
			Actor = *It;
			InitialSemantic = Semantic;
			TestEqual(TEXT("Building actor identity is cell local."), CellId, Bundle.Cells[0].CellId);
		}
	}
	TestNotNull(TEXT("Exactly one building actor is discoverable."), Actor);
	if (Actor != nullptr)
	{
		UStaticMeshComponent* Component = Actor->GetStaticMeshComponent();
		UStaticMesh* Mesh = Component != nullptr ? Component->GetStaticMesh() : nullptr;
		TestNotNull(TEXT("The actor consumes a persistent StaticMesh."), Mesh);
		TestTrue(TEXT("Building massing is spatially loaded."), Actor->GetIsSpatiallyLoaded());
		TestFalse(TEXT("Building massing does not participate in HLOD."),
			Actor->bEnableAutoLODGeneration || Actor->GetHLODLayer() != nullptr);
		TestTrue(TEXT("Generated building actor keeps unit scale."),
			Actor->GetActorScale3D().Equals(FVector::OneVector));
		if (Mesh != nullptr && Component != nullptr)
		{
			UBodySetup* BodySetup = Mesh->GetBodySetup();
			TestTrue(TEXT("Generated building component keeps unit relative scale."),
				Component->GetRelativeScale3D().Equals(FVector::OneVector));
			TestTrue(TEXT("Building massing is Nanite enabled."), Mesh->GetNaniteSettings().bEnabled);
			TestTrue(TEXT("Building collision uses complex-as-simple."),
				BodySetup != nullptr && BodySetup->CollisionTraceFlag == CTF_UseComplexAsSimple);
			TestTrue(TEXT("Building collision is configured as double-sided."),
				BodySetup != nullptr && BodySetup->bDoubleSidedGeometry);
			TestEqual(TEXT("Building collision blocks queries and physics."),
				Component->GetCollisionEnabled(), ECollisionEnabled::QueryAndPhysics);
			TestFalse(TEXT("Blockout buildings do not affect navigation."), Component->CanEverAffectNavigation());
			if (BodySetup != nullptr)
			{
				BodySetup->bDoubleSidedGeometry = false;
				BodySetup->InvalidatePhysicsData();
				BodySetup->CreatePhysicsMeshes();
				Component->RecreatePhysicsState();
				const FWallQueryEvidence OneSided = QueryMinimumXWall(*World, *Component);
				TestTrue(
					TEXT("One-sided control culls exactly one wall-query direction."),
					OneSided.bOutsideToInsideRayBlocked != OneSided.bInsideToOutsideRayBlocked);

				BodySetup->bDoubleSidedGeometry = true;
				BodySetup->InvalidatePhysicsData();
				BodySetup->CreatePhysicsMeshes();
				Component->RecreatePhysicsState();
				const FWallQueryEvidence DoubleSided = QueryMinimumXWall(*World, *Component);
				TestTrue(TEXT("Double-sided collision blocks outside-to-inside pawn capsule travel."),
					DoubleSided.bOutsideToInsideCapsuleBlocked);
				TestTrue(TEXT("Double-sided collision blocks inside-to-outside pawn capsule travel."),
					DoubleSided.bInsideToOutsideCapsuleBlocked);
				TestTrue(TEXT("Double-sided collision blocks outside-to-inside ray queries."),
					DoubleSided.bOutsideToInsideRayBlocked);
				TestTrue(TEXT("Double-sided collision blocks inside-to-outside ray queries."),
					DoubleSided.bInsideToOutsideRayBlocked);
			}
		}
	}

	Inventory.FinalDirtyUnits.Reset();
	const int32 UpdatedBeforeNoOp = Result.UpdatedActorCount;
	const int32 RewritesBeforeNoOp = Result.BuildingTriangleRewriteCount;
	TestTrue(TEXT("An unchanged building layer applies as a no-op."),
		ProjectWorldBuildingRealization::Apply(
			World, Bundle, Profile, OverlaySet, Material, Result, Error));
	TestEqual(TEXT("A no-op does not replace the actor."), Result.UpdatedActorCount, UpdatedBeforeNoOp);
	TestEqual(TEXT("A no-op does not rewrite triangles."),
		Result.BuildingTriangleRewriteCount, RewritesBeforeNoOp);

	Bundle.Features.FindChecked(Feature.FeatureId).HeightMeters = 18.0;
	Inventory.FinalDirtyUnits = {Bundle.Cells[0].CellId};
	TestTrue(TEXT("Changed height replaces the owning building cell."),
		ProjectWorldBuildingRealization::Apply(
			World, Bundle, Profile, OverlaySet, Material, Result, Error));
	TestEqual(TEXT("Exactly one actor is updated."), Result.UpdatedActorCount, UpdatedBeforeNoOp + 1);
	FString UpdatedCell;
	FString UpdatedSemantic;
	TestTrue(TEXT("Updated building identity remains readable."),
		ProjectWorldBuildingRealization::ReadActorIdentity(Actor, UpdatedCell, UpdatedSemantic));
	TestNotEqual(TEXT("Changed massing changes semantic identity."), UpdatedSemantic, InitialSemantic);

	Bundle.Cells[0].OwnedFeatureIds.Reset();
	Bundle.Features.Reset();
	const int32 RemovedBefore = Result.RemovedActorCount;
	TestTrue(TEXT("Removing canonical building input retires its persistent output."),
		ProjectWorldBuildingRealization::Apply(
			World, Bundle, Profile, OverlaySet, Material, Result, Error));
	TestEqual(TEXT("Exactly one building actor is retired."), Result.RemovedActorCount, RemovedBefore + 1);
	const FString MeshFilename = FPackageName::LongPackageNameToFilename(
		Profile.Layers[0].ArtifactRoot + TEXT("Cells/SM_ProjectWorldBuildings_building_persistence_test_a"),
		FPackageName::GetAssetPackageExtension());
	TestFalse(TEXT("The retired building mesh file is deleted."), FPaths::FileExists(MeshFilename));

	TArray<UPackage*> Packages;
	for (TObjectIterator<UPackage> It; It; ++It)
	{
		if (It->GetName().StartsWith(Profile.Layers[0].ArtifactRoot))
		{
			Packages.Add(*It);
		}
	}
	if (!Packages.IsEmpty())
	{
		UPackageTools::UnloadPackages(Packages);
	}
	const FString RootFilename = FPackageName::LongPackageNameToFilename(Profile.Layers[0].ArtifactRoot);
	IFileManager::Get().DeleteDirectory(*RootFilename, false, true);
	TestFalse(TEXT("Disposable building packages are removed after the proof."),
		FPaths::DirectoryExists(RootFilename));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldBuildingInputLocalityTest,
	"Project.World.Realization.Buildings.InputLocality",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldBuildingInputLocalityTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldBuildingMassingTests;
	FProjectWorldCanonicalBundle Bundle;
	Bundle.CoordinateQuantizationMeters = 0.01;
	Bundle.HeightQuantizationMeters = 0.1;
	Bundle.Cells = {Cell(TEXT("building_test:a"), 0.0, 100.0), Cell(TEXT("building_test:b"), 100.0, 200.0)};
	FProjectWorldCanonicalFeature BuildingA = Building(
		TEXT("building/a"), Bundle.Cells[0].CellId, Square(10.0, 10.0, 20.0, 20.0));
	FProjectWorldCanonicalFeature BuildingB = Building(
		TEXT("building/b"), Bundle.Cells[1].CellId, Square(110.0, 10.0, 120.0, 20.0));
	Bundle.Cells[0].OwnedFeatureIds = {BuildingA.FeatureId};
	Bundle.Cells[1].OwnedFeatureIds = {BuildingB.FeatureId};
	Bundle.Features.Add(BuildingA.FeatureId, BuildingA);
	Bundle.Features.Add(BuildingB.FeatureId, BuildingB);
	const FProjectWorldRealizationLayer BuildingLayer = Layer();
	const FProjectWorldAuthoredOverlaySet OverlaySet;
	FString BeforeA;
	FString BeforeB;
	FString Error;
	TestTrue(TEXT("Cell A input hashes."), ProjectWorldBuildingRealization::HashCellInput(
		Bundle, Bundle.Cells[0], BuildingLayer, OverlaySet, BeforeA, Error));
	TestTrue(TEXT("Cell B input hashes."), ProjectWorldBuildingRealization::HashCellInput(
		Bundle, Bundle.Cells[1], BuildingLayer, OverlaySet, BeforeB, Error));

	Bundle.Features.FindChecked(BuildingA.FeatureId).HeightMeters += 3.0;
	FString AfterA;
	FString AfterB;
	TestTrue(TEXT("Changed cell A input hashes."), ProjectWorldBuildingRealization::HashCellInput(
		Bundle, Bundle.Cells[0], BuildingLayer, OverlaySet, AfterA, Error));
	TestTrue(TEXT("Unrelated cell B input hashes."), ProjectWorldBuildingRealization::HashCellInput(
		Bundle, Bundle.Cells[1], BuildingLayer, OverlaySet, AfterB, Error));
	TestNotEqual(TEXT("A building height change dirties its owning cell."), AfterA, BeforeA);
	TestEqual(TEXT("A building height change leaves unrelated cells clean."), AfterB, BeforeB);

	FProjectWorldCanonicalFeature Road;
	Road.FeatureId = TEXT("road/unrelated");
	Road.FeatureClass = TEXT("road");
	Bundle.Features.Add(Road.FeatureId, Road);
	Bundle.Cells[0].OwnedFeatureIds.Add(Road.FeatureId);
	FString AfterRoad;
	TestTrue(TEXT("The building input hashes after an unrelated road change."),
		ProjectWorldBuildingRealization::HashCellInput(
			Bundle, Bundle.Cells[0], BuildingLayer, OverlaySet, AfterRoad, Error));
	TestEqual(TEXT("Unrelated canonical classes do not dirty building massing."), AfterRoad, AfterA);

	FProjectWorldCanonicalBundle CrossCell;
	CrossCell.CoordinateQuantizationMeters = 0.01;
	CrossCell.Cells = {Cell(TEXT("building_test:a"), 0.0, 100.0), Cell(TEXT("building_test:b"), 100.0, 200.0)};
	FProjectWorldCanonicalFeature Crossing = Building(
		TEXT("building/crossing"), CrossCell.Cells[0].CellId, Square(90.0, 10.0, 110.0, 30.0));
	CrossCell.Cells[0].OwnedFeatureIds = {Crossing.FeatureId};
	CrossCell.Cells[1].ReferencedFeatureIds = {Crossing.FeatureId};
	CrossCell.Features.Add(Crossing.FeatureId, Crossing);
	FProjectWorldBuildingMeshBuildResult Left;
	FProjectWorldBuildingMeshBuildResult Right;
	const FProjectWorldBuildingSettings Settings;
	TestTrue(TEXT("The owner fragment builds."), ProjectWorldBuildingMeshBuilder::BuildCell(
		CrossCell, CrossCell.Cells[0], OverlaySet, Settings, Left, Error));
	TestTrue(TEXT("The referenced fragment builds."), ProjectWorldBuildingMeshBuilder::BuildCell(
		CrossCell, CrossCell.Cells[1], OverlaySet, Settings, Right, Error));
	TestEqual(TEXT("Cell clipping adds no duplicate internal seam walls."), Left.TriangleCount + Right.TriangleCount, 20);
	return true;
}

REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FProjectWorldBuildingTopologyAdmissionTest,
	"Project.World.Realization.Buildings.TopologyAdmission",
	"[Unit][World]")

REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FProjectWorldBuildingDimensionsTest,
	"Project.World.Realization.Buildings.Dimensions",
	"[Unit][World]")

REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FProjectWorldBuildingInputLocalityTest,
	"Project.World.Realization.Buildings.InputLocality",
	"[Unit][World]")

REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FProjectWorldPersistentBuildingLayerTest,
	"Project.World.Realization.Buildings.PersistentLayer",
	"[Slow][Integration][World]")

#endif
