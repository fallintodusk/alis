// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldVegetationRealization.h"

#include "ProjectWorldAuthoredOverlay.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldRealizationProfile.h"
#include "ProjectWorldRealizationService.h"

#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ProjectWorldVegetationRealizationTests
{
	FProjectWorldRealizationLayer MakeLayer(int32 MaximumInstances = 100, double Jitter = 0.0)
	{
		FProjectWorldRealizationLayer Layer;
		Layer.LayerId = TEXT("vegetation");
		Layer.LayerKind = EProjectWorldLayerKind::GeneratedGeography;
		Layer.GeneratorId = TEXT("project_vegetation_instances");
		Layer.GeneratorVersion = 1;
		Layer.DependsOn = {TEXT("terrain"), TEXT("water"), TEXT("roads")};
		Layer.CanonicalSelectors = {TEXT("vegetation")};
		Layer.ArtifactRoot = TEXT("/ProjectWorldTestData/Generated/Test/Vegetation/");
		Layer.SpatialOwnership = TEXT("cell_local");
		Layer.DirtyGranularity = EProjectWorldDirtyGranularity::CanonicalCell;
		Layer.RuntimeMapping = TEXT("world_partition_spatial");
		Layer.ContractHash = FString::ChrN(64, TEXT('a'));
		Layer.NormalizedSettings = FString::Printf(
			TEXT("{\"area_jitter_fraction\":%.17g,\"area_spacing_m\":20,\"collision\":\"no_collision\","
				"\"deterministic_seed\":7,\"maximum_instances_per_cell\":%d,\"maximum_scale\":1,"
				"\"mesh_assets\":[\"/ProjectObject/Test.SM_Test\"],\"minimum_scale\":1,\"nanite\":true,"
				"\"placement_policy\":\"canonical_points_and_lattice_areas\",\"surface_offset_m\":0}"),
			Jitter,
			MaximumInstances);
		return Layer;
	}

	FProjectWorldRealizationProfile MakeProfile(FProjectWorldRealizationLayer Vegetation = MakeLayer())
	{
		FProjectWorldRealizationProfile Profile;
		FProjectWorldRealizationLayer Terrain;
		Terrain.LayerId = TEXT("terrain");
		Terrain.GeneratorId = TEXT("project_landscape");
		Terrain.GeneratorVersion = 1;
		FProjectWorldRealizationLayer Water;
		Water.LayerId = TEXT("water");
		Water.GeneratorId = TEXT("project_water_mesh");
		Water.GeneratorVersion = 1;
		FProjectWorldRealizationLayer Roads;
		Roads.LayerId = TEXT("roads");
		Roads.GeneratorId = TEXT("project_road_mesh");
		Roads.GeneratorVersion = 1;
		Roads.NormalizedSettings = TEXT("{\"selected_classes\":[\"primary\"]}");
		Profile.Layers = {MoveTemp(Terrain), MoveTemp(Water), MoveTemp(Roads), MoveTemp(Vegetation)};
		return Profile;
	}

	FProjectWorldCanonicalPolygon Square(double Minimum, double Maximum)
	{
		FProjectWorldCanonicalPolygon Polygon;
		Polygon.Outer = {
			FVector2D(Minimum, Minimum), FVector2D(Maximum, Minimum),
			FVector2D(Maximum, Maximum), FVector2D(Minimum, Maximum),
			FVector2D(Minimum, Minimum)};
		return Polygon;
	}

	FProjectWorldCanonicalBundle MakeBundle(bool bWithHole = false)
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
		Area.GeometryPolygons.Add(Square(0.0, 100.0));
		if (bWithHole)
		{
			Area.GeometryPolygons[0].Holes.Add(Square(30.0, 70.0).Outer);
		}

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

	FVector CanonicalLocation(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldCanonicalCell& Cell,
		const FProjectWorldVegetationInstance& Instance)
	{
		const FVector Origin = FProjectWorldCanonicalLoader::CanonicalToUnreal(
			Bundle, FVector(Cell.Bounds.X, Cell.Bounds.W, Bundle.HeightOriginMeters));
		return FProjectWorldCanonicalLoader::UnrealToCanonical(Bundle, Origin + Instance.Transform.GetLocation());
	}

	FProjectWorldRealizationResult MakeDirtyResult(const FProjectWorldRealizationLayer& Layer)
	{
		FProjectWorldRealizationResult Result;
		FProjectWorldLayerInventory& Inventory = Result.LayerInventories.AddDefaulted_GetRef();
		Inventory.LayerId = Layer.LayerId;
		Inventory.GeneratorId = Layer.GeneratorId;
		Inventory.FinalDirtyUnits.Add(TEXT("*"));
		return Result;
	}

	FProjectWorldAuthoredOverlaySet MakeMaskSet()
	{
		FProjectWorldAuthoredOverlaySet Set;
		FProjectWorldAuthoredOverlay& Mask = Set.Overlays.AddDefaulted_GetRef();
		Mask.OverlayId = TEXT("test/vegetation-mask");
		Mask.Anchor.Kind = EProjectWorldAnchorKind::Mask;
		Mask.Anchor.BoundsMeters = FVector4d(45.0, 0.0, 55.0, 100.0);
		Mask.Anchor.Excludes = {TEXT("vegetation")};
		return Set;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldVegetationPlacementContractTest,
	"Project.World.Realization.Vegetation.PlacementContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldVegetationPlacementContractTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldVegetationRealizationTests;
	FProjectWorldCanonicalBundle Bundle = MakeBundle();
	const FProjectWorldRealizationLayer Layer = MakeLayer();
	const FProjectWorldRealizationProfile Profile = MakeProfile(Layer);
	const FProjectWorldAuthoredOverlaySet OverlaySet;
	TArray<FProjectWorldVegetationInstance> First;
	TArray<FProjectWorldVegetationInstance> Second;
	FString FirstSemantic;
	FString SecondSemantic;
	FString Error;
	TestTrue(TEXT("The synthetic cell produces deterministic vegetation."),
		ProjectWorldVegetationRealization::BuildCellInstances(
			Bundle, Bundle.Cells[0], Layer, Profile, OverlaySet, First, nullptr, FirstSemantic, Error));
	TestTrue(TEXT("The identical synthetic cell can be rebuilt."),
		ProjectWorldVegetationRealization::BuildCellInstances(
			Bundle, Bundle.Cells[0], Layer, Profile, OverlaySet, Second, nullptr, SecondSemantic, Error));
	TestEqual(TEXT("The lattice plus explicit point has a bounded population."), First.Num(), 26);
	TestEqual(TEXT("The semantic hash is deterministic."), SecondSemantic, FirstSemantic);
	TestEqual(TEXT("The stable instance count is deterministic."), Second.Num(), First.Num());
	for (int32 Index = 0; Index < First.Num(); ++Index)
	{
		TestEqual(TEXT("Stable identities retain their order."), Second[Index].StableId, First[Index].StableId);
		const FVector Canonical = CanonicalLocation(Bundle, Bundle.Cells[0], First[Index]);
		TestTrue(TEXT("Every instance remains inside its owning cell."),
			Canonical.X >= 0.0 && Canonical.X < 100.0 && Canonical.Y >= 0.0 && Canonical.Y < 100.0);
		TestEqual(TEXT("Every instance is terrain-draped."), Canonical.Z, 2.0);
	}

	FProjectWorldCanonicalBundle WithHole = MakeBundle(true);
	TArray<FProjectWorldVegetationInstance> HoleInstances;
	FString HoleSemantic;
	TestTrue(TEXT("A polygon hole remains executable."),
		ProjectWorldVegetationRealization::BuildCellInstances(
			WithHole, WithHole.Cells[0], Layer, Profile, OverlaySet,
			HoleInstances, nullptr, HoleSemantic, Error));
	for (const FProjectWorldVegetationInstance& Instance : HoleInstances)
	{
		const FVector Canonical = CanonicalLocation(WithHole, WithHole.Cells[0], Instance);
		TestFalse(TEXT("Area placement does not fill a canonical hole."),
			Canonical.X > 30.0 && Canonical.X < 70.0 && Canonical.Y > 30.0 && Canonical.Y < 70.0);
	}

	TArray<FProjectWorldVegetationInstance> Capped;
	FString CappedSemantic;
	TestTrue(TEXT("The per-cell population ceiling remains executable."),
		ProjectWorldVegetationRealization::BuildCellInstances(
			Bundle, Bundle.Cells[0], MakeLayer(3), Profile, OverlaySet,
			Capped, nullptr, CappedSemantic, Error));
	TestEqual(TEXT("The per-cell ceiling is exact."), Capped.Num(), 3);

	FString BroadleafHash;
	FString NeedleleafHash;
	TestTrue(TEXT("Broadleaf input hashes."), ProjectWorldVegetationRealization::HashCellInput(
		Bundle, Bundle.Cells[0], Layer, Profile, OverlaySet, BroadleafHash, Error));
	Bundle.Features.FindChecked(TEXT("alis:test:vegetation:area")).LeafType = TEXT("needleleaved");
	TestTrue(TEXT("Needleleaf input hashes."), ProjectWorldVegetationRealization::HashCellInput(
		Bundle, Bundle.Cells[0], Layer, Profile, OverlaySet, NeedleleafHash, Error));
	TestNotEqual(TEXT("Canonical vegetation semantics invalidate only their layer input."), BroadleafHash, NeedleleafHash);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldVegetationExclusionContractTest,
	"Project.World.Realization.Vegetation.ExclusionContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldVegetationExclusionContractTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldVegetationRealizationTests;
	FProjectWorldCanonicalBundle Bundle = MakeBundle();
	FProjectWorldCanonicalCell& Cell = Bundle.Cells[0];
	FProjectWorldCanonicalFeature Road;
	Road.FeatureId = TEXT("test/road/primary");
	Road.FeatureClass = TEXT("road");
	Road.RoadClass = TEXT("primary");
	Road.WidthMeters = 8.0;
	FProjectWorldCanonicalRepresentation& RoadFragment = Road.Representations.AddDefaulted_GetRef();
	RoadFragment.CellId = Cell.CellId;
	RoadFragment.Kind = TEXT("road_fragment");
	RoadFragment.Parts.Add({FVector2D(10.0, 0.0), FVector2D(10.0, 100.0)});
	FProjectWorldCanonicalFeature Water;
	Water.FeatureId = TEXT("test/water/lake");
	Water.FeatureClass = TEXT("water");
	Water.WaterSurface.bValid = true;
	Water.WaterSurface.SurfaceGroupId = Water.FeatureId;
	Water.WaterSurface.Geometry = TEXT("polygon");
	Water.WaterSurface.Behavior = TEXT("standing");
	Water.WaterSurface.FunctionId = TEXT("standing_polygon_quantile");
	Water.WaterSurface.FunctionVersion = 1;
	FProjectWorldCanonicalPolygon& WaterPolygon = Water.GeometryPolygons.AddDefaulted_GetRef();
	WaterPolygon.Outer = {
		FVector2D(25.0, 0.0), FVector2D(35.0, 0.0), FVector2D(35.0, 100.0),
		FVector2D(25.0, 100.0), FVector2D(25.0, 0.0)};
	Cell.OwnedFeatureIds.Append({Road.FeatureId, Water.FeatureId});
	Bundle.Features.Add(Road.FeatureId, MoveTemp(Road));
	Bundle.Features.Add(Water.FeatureId, MoveTemp(Water));
	const FProjectWorldRealizationLayer Layer = MakeLayer();
	const FProjectWorldRealizationProfile Profile = MakeProfile(Layer);
	FProjectWorldAuthoredOverlaySet OverlaySet = MakeMaskSet();
	TArray<FProjectWorldVegetationInstance> Instances;
	FProjectWorldVegetationPlacementStats Stats;
	FString Semantic;
	FString Error;
	if (!ProjectWorldVegetationRealization::BuildCellInstances(
		Bundle, Cell, Layer, Profile, OverlaySet, Instances, &Stats, Semantic, Error))
	{
		AddError(Error);
		return false;
	}
	TestTrue(TEXT("The road exclusion removes candidates."), Stats.RoadExcludedCount > 0);
	TestTrue(TEXT("The water exclusion removes candidates."), Stats.WaterExcludedCount > 0);
	TestTrue(TEXT("The authored mask removes candidates."), Stats.AuthoredMaskExcludedCount > 0);
	for (const FProjectWorldVegetationInstance& Instance : Instances)
	{
		const FVector Point = CanonicalLocation(Bundle, Cell, Instance);
		TestTrue(TEXT("Every retained instance remains outside all protected footprints."),
			Point.X > 14.0 && (Point.X < 25.0 || Point.X > 35.0) && (Point.X < 45.0 || Point.X > 55.0));
	}
	FString InitialHash;
	FString UnrelatedMaskHash;
	FString ChangedRoadHash;
	TestTrue(TEXT("The protected input hashes."), ProjectWorldVegetationRealization::HashCellInput(
		Bundle, Cell, Layer, Profile, OverlaySet, InitialHash, Error));
	FProjectWorldAuthoredOverlay& OutsideMask = OverlaySet.Overlays.AddDefaulted_GetRef();
	OutsideMask.OverlayId = TEXT("test/outside-mask");
	OutsideMask.Anchor.Kind = EProjectWorldAnchorKind::Mask;
	OutsideMask.Anchor.BoundsMeters = FVector4d(200.0, 200.0, 250.0, 250.0);
	OutsideMask.Anchor.Excludes = {TEXT("vegetation")};
	TestTrue(TEXT("An unrelated authored mask hashes."), ProjectWorldVegetationRealization::HashCellInput(
		Bundle, Cell, Layer, Profile, OverlaySet, UnrelatedMaskHash, Error));
	TestEqual(TEXT("An unrelated authored mask does not dirty this cell."), UnrelatedMaskHash, InitialHash);
	Bundle.Features.FindChecked(TEXT("test/road/primary")).WidthMeters = 12.0;
	TestTrue(TEXT("A changed road footprint hashes."), ProjectWorldVegetationRealization::HashCellInput(
		Bundle, Cell, Layer, Profile, OverlaySet, ChangedRoadHash, Error));
	TestNotEqual(TEXT("A changed road footprint dirties vegetation input."), ChangedRoadHash, InitialHash);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldVegetationAssetContractTest,
	"Project.World.Realization.Vegetation.AssetContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldVegetationAssetContractTest::RunTest(const FString& Parameters)
{
	const TCHAR* Paths[] = {
		TEXT("/ProjectObject/Nature/ExteriorPlant/Tree/Hornbeam/SM_Tree_Hornbeam_Medium.SM_Tree_Hornbeam_Medium"),
		TEXT("/ProjectObject/Nature/ExteriorPlant/Tree/AmurCork/SM_Tree_AmurCork_Big.SM_Tree_AmurCork_Big")};
	for (const TCHAR* Path : Paths)
	{
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, Path);
		TestNotNull(TEXT("The admitted vegetation mesh is loadable."), Mesh);
		if (Mesh != nullptr)
		{
			TestTrue(TEXT("The admitted vegetation mesh is Nanite-enabled."), Mesh->GetNaniteSettings().bEnabled);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldVegetationRetirementPersistenceTest,
	"Project.World.Realization.Vegetation.RetirementPersistence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldVegetationRetirementPersistenceTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldVegetationRealizationTests;
	UWorld* World = GEditor->NewMap(false);
	FProjectWorldCanonicalBundle Bundle = MakeBundle();
	FProjectWorldRealizationProfile Profile = MakeProfile();
	FProjectWorldRealizationLayer& Vegetation = Profile.Layers.Last();
	Vegetation.NormalizedSettings.ReplaceInline(
		TEXT("/ProjectObject/Test.SM_Test"),
		TEXT("/ProjectObject/Nature/ExteriorPlant/Tree/Hornbeam/SM_Tree_Hornbeam_Medium.SM_Tree_Hornbeam_Medium"));
	const FProjectWorldAuthoredOverlaySet OverlaySet;
	FProjectWorldRealizationResult First = MakeDirtyResult(Vegetation);
	FString Error;
	TestTrue(TEXT("The occupied cell is realized."),
		ProjectWorldVegetationRealization::Apply(World, Bundle, Profile, OverlaySet, First, Error));
	AActor* VegetationActor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		FString CellId;
		FString Semantic;
		if (ProjectWorldVegetationRealization::ReadActorIdentity(*It, CellId, Semantic))
		{
			VegetationActor = *It;
			break;
		}
	}
	TestNotNull(TEXT("The occupied cell owns one vegetation actor."), VegetationActor);
	if (VegetationActor == nullptr || VegetationActor->GetExternalPackage() == nullptr)
	{
		return false;
	}
	const FVector ExpectedOrigin = FProjectWorldCanonicalLoader::CanonicalToUnreal(
		Bundle,
		FVector(Bundle.Cells[0].Bounds.X, Bundle.Cells[0].Bounds.W, Bundle.HeightOriginMeters));
	TestTrue(TEXT("The realized actor retains its canonical cell origin after root replacement."),
		VegetationActor->GetActorLocation().Equals(ExpectedOrigin, UE_KINDA_SMALL_NUMBER));
	TestTrue(TEXT("The OFPA-serialized root owns the canonical cell origin."),
		VegetationActor->GetRootComponent() != nullptr &&
		VegetationActor->GetRootComponent()->GetRelativeLocation().Equals(ExpectedOrigin, UE_KINDA_SMALL_NUMBER));
	FProjectWorldRealizationResult ProducerRefresh = MakeDirtyResult(Vegetation);
	TestTrue(TEXT("A whole-layer producer refresh reapplies unchanged cell semantics."),
		ProjectWorldVegetationRealization::Apply(
			World, Bundle, Profile, OverlaySet, ProducerRefresh, Error));
	TestEqual(TEXT("The producer refresh updates the existing cell actor."),
		ProducerRefresh.UpdatedActorCount, 1);
	TestTrue(TEXT("The producer refresh rewrites the cell instances."),
		ProducerRefresh.VegetationInstanceRewriteCount > 0);
	const FString Filename = FPackageName::LongPackageNameToFilename(
		VegetationActor->GetExternalPackage()->GetName(), FPackageName::GetAssetPackageExtension());
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
	TestTrue(TEXT("The retirement fixture creates an external actor artifact."),
		FFileHelper::SaveStringToFile(TEXT("occupied"), *Filename));
	Bundle.Cells[0].OwnedFeatureIds.Reset();
	Bundle.Features.Reset();
	FProjectWorldRealizationResult Second = MakeDirtyResult(Vegetation);
	TestTrue(TEXT("Occupied-to-empty regeneration succeeds."),
		ProjectWorldVegetationRealization::Apply(World, Bundle, Profile, OverlaySet, Second, Error));
	TestFalse(TEXT("Occupied-to-empty regeneration retires the OFPA artifact."),
		IFileManager::Get().FileExists(*Filename));
	IFileManager::Get().Delete(*Filename, false, true);
	return true;
}

#endif
