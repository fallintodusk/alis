// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldAuthoredOverlay.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldGeneratedGeometry.h"
#include "ProjectWorldLayerProducerRegistry.h"
#include "ProjectWorldRealizationProfile.h"
#include "ProjectWorldStaticPartitionAudit.h"
#include "ProjectWorldVerify.h"

#include "Editor.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ProjectWorldBuildingRecoveryTests
{
	const FName CurrentTag(TEXT("ProjectWorld.BuildingMassing.v2"));
	const FName StaleTag(TEXT("ProjectWorld.BuildingMassing.v1"));
	const FString CellId(TEXT("building_recovery_test:a"));

	FProjectWorldCanonicalBundle Bundle()
	{
		FProjectWorldCanonicalBundle Result;
		Result.GridId = TEXT("building_recovery_test");
		Result.CoordinateQuantizationMeters = 0.01;
		Result.HeightQuantizationMeters = 0.1;
		FProjectWorldCanonicalCell& Cell = Result.Cells.AddDefaulted_GetRef();
		Cell.CellId = CellId;
		Cell.Bounds = FVector4d(0.0, 0.0, 100.0, 100.0);
		Cell.Terrain.ArtifactHash = FString::ChrN(64, TEXT('a'));
		Cell.Terrain.Bounds = Cell.Bounds;
		Cell.Terrain.SampleSpacing = FVector2D(100.0, 100.0);
		Cell.Terrain.SamplesX = 2;
		Cell.Terrain.SamplesY = 2;
		Cell.Terrain.HeightsMeters = {2.0, 2.0, 2.0, 2.0};
		FProjectWorldCanonicalPolygon Polygon;
		Polygon.Outer = {
			FVector2D(10.0, 10.0), FVector2D(40.0, 10.0), FVector2D(40.0, 40.0),
			FVector2D(10.0, 40.0), FVector2D(10.0, 10.0)};
		FProjectWorldCanonicalFeature Feature;
		Feature.FeatureId = TEXT("building/recovery");
		Feature.FeatureClass = TEXT("building");
		Feature.OwnerCellId = CellId;
		Feature.GeometryType = TEXT("Polygon");
		Feature.GeometryPolygons.Add(Polygon);
		Feature.HeightMeters = 12.0;
		FProjectWorldCanonicalBuildingVolume& Volume = Feature.BuildingVolumes.AddDefaulted_GetRef();
		Volume.VolumeId = TEXT("building/recovery/volume");
		Volume.SourceFeatureId = Feature.FeatureId;
		Volume.GeometryType = TEXT("Polygon");
		Volume.GeometryPolygons.Add(Polygon);
		Volume.MinHeightMeters = 0.0;
		Volume.HeightMeters = 12.0;
		Cell.OwnedFeatureIds.Add(Feature.FeatureId);
		Result.Features.Add(Feature.FeatureId, MoveTemp(Feature));
		return Result;
	}

	FProjectWorldRealizationProfile Profile(const FString& Root)
	{
		FProjectWorldRealizationProfile Result;
		FProjectWorldRealizationLayer& Layer = Result.Layers.AddDefaulted_GetRef();
		Layer.LayerId = TEXT("buildings");
		Layer.LayerKind = EProjectWorldLayerKind::GeneratedGeography;
		Layer.GeneratorId = TEXT("project_building_massing");
		Layer.GeneratorVersion = 2;
		Layer.CanonicalSelectors = {TEXT("buildings")};
		Layer.ArtifactRoot = Root;
		Layer.SpatialOwnership = TEXT("cell_local");
		Layer.DirtyGranularity = EProjectWorldDirtyGranularity::CanonicalCell;
		Layer.RuntimeMapping = TEXT("world_partition_spatial");
		Layer.ContractHash = FString::ChrN(64, TEXT('c'));
		Layer.NormalizedSettings = TEXT(
			"{\"collision\":\"complex_as_simple\",\"conflict_policy\":\"reject_affected_fragments\","
			"\"contained_policy\":\"associate_with_container\",\"duplicate_policy\":\"stable_feature_id\","
			"\"maximum_height_m\":300,\"nanite\":true,\"navigation\":\"no_navigation\","
			"\"terrain_anchor_policy\":\"owner_cell_clamped_bounds_center\","
			"\"topology_policy\":\"logical_building_classify_v2\"}");
		return Result;
	}

	AStaticMeshActor* Find(UWorld* World, const FName Tag, int32& OutCount)
	{
		AStaticMeshActor* Found = nullptr;
		OutCount = 0;
		for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
		{
			if (It->Tags.Contains(Tag))
			{
				Found = *It;
				++OutCount;
			}
		}
		return Found;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldBuildingRecoveryTest,
	"Project.World.Realization.Buildings.SavedStaleRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldBuildingRecoveryTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldBuildingRecoveryTests;
	FProjectWorldVerifyWorld Verify(TEXT("BuildingRecovery"));
	FString Error;
	if (!TestTrue(TEXT("The persistent test map exists."), Verify.IsReady(Error)))
	{
		AddError(Error);
		return false;
	}
	const FProjectWorldCanonicalBundle Canonical = Bundle();
	const FProjectWorldRealizationProfile Realization = Profile(Verify.GetRoot());
	const FProjectWorldAuthoredOverlaySet Overlays;
	const IProjectWorldLayerProducer* Producer = ProjectWorldLayerProducerRegistry::Find(
		TEXT("project_building_massing"), 2, Error);
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (!TestNotNull(TEXT("The building producer is registered."), Producer) ||
		!TestNotNull(TEXT("The building test material exists."), Material))
	{
		AddError(Error);
		return false;
	}
	TMap<FName, UMaterialInterface*> Materials;
	Materials.Add(TEXT("building"), Material);
	const FProjectWorldLayerInputs Inputs{Canonical, Realization, Realization.Layers[0], Overlays};
	auto Apply = [&](const TArray<FString>& Dirty, FProjectWorldRealizationResult& Result) -> bool
	{
		FProjectWorldLayerInventory& Inventory = Result.LayerInventories.AddDefaulted_GetRef();
		Inventory.LayerId = TEXT("buildings");
		Inventory.GeneratorId = TEXT("project_building_massing");
		Inventory.FinalDirtyUnits = Dirty;
		const FProjectWorldLayerApplyContext Context{*Verify.GetWorld(), Inputs, Inventory.FinalDirtyUnits, Materials};
		return Producer->Apply(Context, Inventory, Result, Error);
	};
	FProjectWorldRealizationResult First;
	if (!TestTrue(TEXT("Initial building is saved and reloaded."),
		Apply({TEXT("*")}, First) && Verify.SaveOwned(Error) && Verify.Reload(Error)))
	{
		AddError(Error);
		return false;
	}
	int32 Count = 0;
	AStaticMeshActor* Actor = Find(Verify.GetWorld(), CurrentTag, Count);
	if (!TestEqual(TEXT("One current actor was loaded from disk."), Count, 1) || Actor == nullptr)
	{
		return false;
	}
	Actor->Modify();
	Actor->Tags.Remove(CurrentTag);
	Actor->Tags.Add(StaleTag);
	Actor->MarkPackageDirty();
	if (!TestTrue(TEXT("Stale identity is persisted and reloaded."),
		Verify.SaveOwned(Error) && Verify.Reload(Error)))
	{
		AddError(Error);
		return false;
	}
	Actor = Find(Verify.GetWorld(), StaleTag, Count);
	if (!TestEqual(TEXT("One stale actor was loaded from disk."), Count, 1) || Actor == nullptr)
	{
		return false;
	}
	const IProjectWorldLayerProducer* Owner = nullptr;
	TestFalse(TEXT("The static audit rejects stale building ownership."),
		ProjectWorldStaticPartitionAudit::ValidateGeneratedActorOwnership(*Actor, Owner, Error));
	Error.Reset();
	FProjectWorldRealizationResult Cleanup;
	if (!TestTrue(TEXT("The actual lifecycle retires the saved stale actor."),
		ProjectWorldGeneratedGeometry::RemoveStaleOwnedActorsForApply(
			Verify.GetWorld(), Canonical, FString(), Cleanup, &Error)))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("Exactly one stale actor was retired."), Cleanup.RemovedActorCount, 1);
	FProjectWorldRealizationResult Replacement;
	if (!TestTrue(TEXT("Producer replaces stale output, then saves and reloads it."),
		Apply({CellId}, Replacement) && Verify.SaveOwned(Error) && Verify.Reload(Error)))
	{
		AddError(Error);
		return false;
	}
	Actor = Find(Verify.GetWorld(), CurrentTag, Count);
	TestEqual(TEXT("Exactly one current actor remains after reload."), Count, 1);
	TestNotNull(TEXT("The current actor survived reload."), Actor);
	FProjectWorldRealizationResult Unchanged;
	if (!TestTrue(TEXT("Unchanged Apply succeeds."), Apply({}, Unchanged)))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("Unchanged Apply updates no actor."), Unchanged.UpdatedActorCount, 0);
	TestEqual(TEXT("Unchanged Apply creates no actor."), Unchanged.CreatedActorCount, 0);
	TestEqual(TEXT("Unchanged Apply writes no triangles."),
		Unchanged.GetProducerMetric(TEXT("project_building_massing"), TEXT("triangles_written")), int64(0));
	return true;
}

REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FProjectWorldBuildingRecoveryTest,
	"Project.World.Realization.Buildings.SavedStaleRecovery", "[Slow][Integration][World]")

#endif
