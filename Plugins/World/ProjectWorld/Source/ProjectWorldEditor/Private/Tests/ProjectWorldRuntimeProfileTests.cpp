// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldGeneratedGeometry.h"
#include "ProjectWorldPartitionPolicy.h"
#include "ProjectWorldRealizationService.h"
#include "ProjectWorldRuntimeProfile.h"
#include "ProjectWorldRuntimeRealization.h"
#include "ProjectWorldStaticPartitionAudit.h"
#include "Tests/ProjectWorldSchemaTestUtilities.h"

#include "Editor.h"
#include "Engine/TargetPoint.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavigationInvokerComponent.h"
#include "NavigationSystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ProjectWorldRuntimeTests
{
	FString ShippedProfilePath()
	{
		return FPaths::Combine(
			FPaths::ProjectPluginsDir(),
			TEXT("World/ProjectWorldTestData/Data/Runtime/synthetic_territory_twin_v1.json"));
	}

	FProjectWorldCanonicalBundle MakeBundle(const FProjectWorldRuntimeProfile& Profile)
	{
		FProjectWorldCanonicalBundle Bundle;
		Bundle.GridId = Profile.GridId;
		Bundle.InputsHash = TEXT("runtime_inputs");
		Bundle.LatticeOriginMeters = FVector2D::ZeroVector;
		Bundle.EngineGeoreferenceOriginMeters = FVector2D::ZeroVector;
		Bundle.HeightOriginMeters = 0.0;
		Bundle.CoordinateQuantizationMeters = 0.01;
		for (int32 CellX = 0; CellX < 2; ++CellX)
		{
			FProjectWorldCanonicalCell Cell;
			Cell.CellId = FString::Printf(TEXT("%s:x%d:y0"), *Profile.GridId, CellX);
			Cell.CellX = CellX;
			Cell.Bounds = FVector4d(CellX * 100.0, 0.0, (CellX + 1) * 100.0, 100.0);
			Cell.Terrain.Bounds = Cell.Bounds;
			Cell.Terrain.SampleSpacing = FVector2D(100.0, 100.0);
			Cell.Terrain.SamplesX = 2;
			Cell.Terrain.SamplesY = 2;
			Cell.Terrain.HeightsMeters = {0.0, 0.0, 0.0, 0.0};
			Bundle.Cells.Add(MoveTemp(Cell));
		}

		FProjectWorldCanonicalFeature Route;
		Route.FeatureId = Profile.RouteFeatureId;
		Route.FeatureClass = TEXT("road");
		Route.GeometryType = TEXT("LineString");
		Route.GeometryPoints = {FVector2D(20.0, 50.0), FVector2D(180.0, 50.0)};
		Route.WidthMeters = 2.0;
		for (int32 CellX = 0; CellX < 2; ++CellX)
		{
			FProjectWorldCanonicalRepresentation Representation;
			Representation.CellId = Bundle.Cells[CellX].CellId;
			Representation.Kind = TEXT("road_fragment");
			Representation.Parts.Add(CellX == 0
				? TArray<FVector2D>{FVector2D(20.0, 50.0), FVector2D(100.0, 50.0)}
				: TArray<FVector2D>{FVector2D(100.0, 50.0), FVector2D(180.0, 50.0)});
			Route.Representations.Add(MoveTemp(Representation));
		}
		Bundle.Features.Add(Route.FeatureId, MoveTemp(Route));
		return Bundle;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldRuntimeProfileContractTest,
	"Project.World.Realization.Runtime.ProfileContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldRuntimeProfileContractTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldRuntimeTests;
	using namespace ProjectWorldSchemaTestUtilities;
	FProjectWorldRuntimeProfile Profile;
	FString ErrorCode;
	FString Error;
	TestTrue(
		TEXT("The shipped runtime profile passes its executable contract."),
		ProjectWorldRuntimeProfile::Load(ShippedProfilePath(), Profile, ErrorCode, Error));
	TestEqual(TEXT("Runtime profile identity."), Profile.ProfileId, FString(TEXT("synthetic_territory_twin_v1")));
	TestEqual(TEXT("Runtime profile SHA-256 is complete."), Profile.ProfileHash.Len(), 64);
	TestEqual(TEXT("HLOD is explicitly disabled for the territory."), Profile.HlodPolicy, FString(TEXT("disabled_for_territory")));

	FProjectWorldCanonicalBundle Bundle = MakeBundle(Profile);
	TestTrue(TEXT("The pinned route is accepted by the matching canonical grid."), ProjectWorldRuntimeRealization::Validate(Bundle, Profile, Error));
	Profile.ProfileKind = TEXT("bounded_procedural_route");
	TestFalse(TEXT("A crafted retired profile cannot pass runtime validation."),
		ProjectWorldRuntimeRealization::Validate(Bundle, Profile, Error));
	Profile.ProfileKind = TEXT("territory_product");
	Bundle.GridId = TEXT("grid_different");
	TestFalse(TEXT("A runtime profile cannot drift onto another grid."), ProjectWorldRuntimeRealization::Validate(Bundle, Profile, Error));

	FString ShippedSource;
	TestTrue(TEXT("Runtime profile fixture is readable."), FFileHelper::LoadFileToString(ShippedSource, *ShippedProfilePath()));
	const FString InvalidPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/ProjectWorldRuntime/invalid.json"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(InvalidPath), true);
	const FString Source = Rewrite(
		ShippedSource,
		InvalidPath,
		TEXT("project_world_runtime_profile.schema.json"));
	const FString ProductionRoot = FPaths::Combine(
		FPaths::ProjectPluginsDir(),
		TEXT("World/ProjectWorldData/Data/Profiles"));
	const FString ProductionPath = FPaths::Combine(ProductionRoot, TEXT("runtime_loader_test.json"));
	const FString ProductionSchema = ReferenceFor(
		ProductionPath,
		TEXT("project_world_runtime_profile.schema.json"));
	TestEqual(
		TEXT("Production runtime schema climbs to the canonical logic plugin."),
		ProductionSchema,
		FString(TEXT("../../../ProjectWorld/Data/Schemas/project_world_runtime_profile.schema.json")));
	IFileManager::Get().MakeDirectory(*ProductionRoot, true);
	TestTrue(
		TEXT("Production-root runtime fixture is writable."),
		FFileHelper::SaveStringToFile(
			Rewrite(
				ShippedSource,
				ProductionPath,
				TEXT("project_world_runtime_profile.schema.json")),
			*ProductionPath));
	TestTrue(
		TEXT("The runtime loader accepts an owner-relative production schema."),
		ProjectWorldRuntimeProfile::Load(ProductionPath, Profile, ErrorCode, Error));
	const FString Invalid = Source.Replace(TEXT("disabled_for_territory"), TEXT("always_hlod"));
	TestTrue(TEXT("Invalid runtime profile fixture is writable."), FFileHelper::SaveStringToFile(Invalid, *InvalidPath));
	TestFalse(
		TEXT("Unmeasured HLOD policy cannot enter the territory runtime profile."),
		ProjectWorldRuntimeProfile::Load(InvalidPath, Profile, ErrorCode, Error));
	TestEqual(TEXT("Optimization rejection is structured."), ErrorCode, FString(TEXT("runtime-profile-optimization")));

	const FString InvalidGrid = Source.Replace(TEXT("\"grid_id\": \"grid_"), TEXT("\"grid_id\": \"grid_Bad"));
	TestTrue(TEXT("Invalid-grid fixture is writable."), FFileHelper::SaveStringToFile(InvalidGrid, *InvalidPath));
	TestFalse(
		TEXT("The runtime loader enforces the schema grid identifier pattern."),
		ProjectWorldRuntimeProfile::Load(InvalidPath, Profile, ErrorCode, Error));
	TestEqual(TEXT("Grid rejection is structured."), ErrorCode, FString(TEXT("runtime-profile-contract")));

	const FString FractionalBudget = Source.Replace(
		TEXT("\"generated_actor_count\": 5000"),
		TEXT("\"generated_actor_count\": 5000.5"));
	TestTrue(TEXT("Fractional-budget fixture is writable."), FFileHelper::SaveStringToFile(FractionalBudget, *InvalidPath));
	TestFalse(
		TEXT("Integer schema budgets reject fractional JSON numbers."),
		ProjectWorldRuntimeProfile::Load(InvalidPath, Profile, ErrorCode, Error));
	TestEqual(TEXT("Budget rejection is structured."), ErrorCode, FString(TEXT("runtime-profile-budgets")));
	IFileManager::Get().Delete(*InvalidPath, false, true, true);
	IFileManager::Get().Delete(*ProductionPath, false, true, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldRuntimeStaleIdentityReplacementTest,
	"Project.World.Realization.Runtime.StaleIdentityReplacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldRuntimeStaleIdentityReplacementTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldRuntimeTests;
	FProjectWorldRuntimeProfile Profile;
	FString ErrorCode;
	FString Error;
	if (!ProjectWorldRuntimeProfile::Load(ShippedProfilePath(), Profile, ErrorCode, Error))
	{
		AddError(Error);
		return false;
	}
	Profile.ProfileKind = TEXT("territory_product");
	Profile.ProductSpawnAnchor = TEXT("engine_georeference_origin");
	Profile.ProductSpawnHeightAboveTerrainMeters = 180.0;
	Profile.ProductSpawnYawDegrees = 45.0;
	Profile.ProductSpawnPitchDegrees = -20.0;
	const FProjectWorldCanonicalBundle Bundle = MakeBundle(Profile);
	UWorld* World = GEditor->NewMap(false);
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Name = TEXT("ProjectWorld_LegacyPlayerStart");
	SpawnParameters.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Required_ErrorAndReturnNull;
	APlayerStart* Stale = World->SpawnActor<APlayerStart>(
		APlayerStart::StaticClass(), FTransform::Identity, SpawnParameters);
	TestNotNull(TEXT("The stale persisted runtime actor is created."), Stale);
	if (Stale == nullptr)
	{
		return false;
	}
	Stale->Tags.Add(TEXT("ProjectWorld.RuntimeRole=PlayerStart"));
	SpawnParameters.Name = TEXT("ProjectWorld_PlayerStart");
	APlayerStart* NameRemnant = World->SpawnActor<APlayerStart>(
		APlayerStart::StaticClass(), FTransform::Identity, SpawnParameters);
	TestNotNull(TEXT("A second stale actor occupies the deterministic name."), NameRemnant);
	SpawnParameters.Name = TEXT("ProjectWorld_CurrentPlayerStart");
	SpawnParameters.OverrideActorGuid = ProjectWorldGeneratedGeometry::StableGuid(
		Bundle.GridId + TEXT("|runtime|PlayerStart"));
	APlayerStart* StableIdentity = World->SpawnActor<APlayerStart>(
		APlayerStart::StaticClass(), FTransform::Identity, SpawnParameters);
	TestNotNull(TEXT("The persisted stable GUID actor is available for reuse."), StableIdentity);

	FProjectWorldRealizationResult Result;
	TestTrue(
		TEXT("Runtime realization retires and replaces stale persisted identity."),
		ProjectWorldRuntimeRealization::Apply(World, Bundle, Profile, Result, Error));
	TestEqual(TEXT("All competing identity owners are retired."), Result.RemovedActorCount, 3);
	TestEqual(TEXT("The canonical product actors replace the competing owners."), Result.CreatedActorCount, 2);
	TestEqual(TEXT("No actor is reused from a competing identity set."), Result.UpdatedActorCount, 0);
	AActor* Replacement = FindObject<AActor>(World->PersistentLevel, TEXT("ProjectWorld_PlayerStart"));
	TestNotNull(TEXT("The replacement reclaims the deterministic object name."), Replacement);
	TestTrue(
		TEXT("The replacement owns the stable runtime identity."),
		Replacement != nullptr && Replacement->GetActorGuid() == ProjectWorldGeneratedGeometry::StableGuid(
			Bundle.GridId + TEXT("|runtime|PlayerStart")));
	TestTrue(
		TEXT("The replacement owns the generated runtime-role tags."),
		Replacement != nullptr && Replacement->Tags.Contains(ProjectWorldGeneratedGeometry::GeneratedTag) &&
		Replacement->Tags.Contains(TEXT("ProjectWorld.RuntimeRole=PlayerStart")));
	TestTrue(
		TEXT("The replacement is always loaded for product boot."),
		Replacement != nullptr && !Replacement->GetIsSpatiallyLoaded());
	TestTrue(
		TEXT("The replacement starts above the Spasskaya-derived engine origin."),
		Replacement != nullptr && Replacement->GetActorLocation().Equals(FVector(0.0, 0.0, 18000.0), 0.01));
	TestTrue(
		TEXT("The replacement starts with the data-owned overview orientation."),
		Replacement != nullptr && Replacement->GetActorRotation().Equals(FRotator(-20.0, 45.0, 0.0), 0.01));
	if (Replacement != nullptr)
	{
		Replacement->GetPackage()->SetDirtyFlag(false);
	}
	FProjectWorldRealizationResult NoOpResult;
	TestTrue(
		TEXT("An unchanged product runtime applies as a semantic no-op."),
		ProjectWorldRuntimeRealization::Apply(World, Bundle, Profile, NoOpResult, Error));
	TestEqual(TEXT("The no-op creates no actor."), NoOpResult.CreatedActorCount, 0);
	TestEqual(TEXT("The no-op updates no actor."), NoOpResult.UpdatedActorCount, 0);
	TestEqual(TEXT("The no-op removes no actor."), NoOpResult.RemovedActorCount, 0);
	TestEqual(TEXT("The no-op preserves the stable product actors."), NoOpResult.PreservedActorCount, 2);
	TestFalse(
		TEXT("The no-op leaves the PlayerStart package clean."),
		Replacement != nullptr && Replacement->GetPackage()->IsDirty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldProductNavigationDomainTest,
	"Project.World.Realization.Runtime.ProductNavigationDomain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldProductNavigationDomainTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldRuntimeTests;
	FProjectWorldRuntimeProfile Profile;
	FString ErrorCode;
	FString Error;
	if (!ProjectWorldRuntimeProfile::Load(ShippedProfilePath(), Profile, ErrorCode, Error))
	{
		AddError(Error);
		return false;
	}
	Profile.ProfileKind = TEXT("territory_product");
	Profile.ProductSpawnAnchor = TEXT("engine_georeference_origin");
	Profile.ProductSpawnHeightAboveTerrainMeters = 180.0;
	Profile.ProductSpawnYawDegrees = 45.0;
	Profile.ProductSpawnPitchDegrees = -20.0;
	const FProjectWorldCanonicalBundle Bundle = MakeBundle(Profile);
	UWorld* World = GEditor->NewMap(false);
	FProjectWorldRealizationResult Result;
	if (!TestTrue(
		TEXT("Product realization creates its neutral navigation domain."),
		ProjectWorldRuntimeRealization::Apply(World, Bundle, Profile, Result, Error)))
	{
		AddError(Error);
		return false;
	}

	ANavMeshBoundsVolume* NavigationBounds =
		FindObject<ANavMeshBoundsVolume>(World->PersistentLevel, TEXT("ProjectWorld_TerritoryNavigation"));
	TestNotNull(TEXT("The product map owns deterministic territory navigation bounds."), NavigationBounds);
	if (NavigationBounds != nullptr)
	{
		TestTrue(TEXT("Territory navigation bounds are always loaded."),
			!NavigationBounds->GetIsSpatiallyLoaded());
		TestTrue(TEXT("Territory navigation owns the generated runtime identity."),
			NavigationBounds->Tags.Contains(ProjectWorldGeneratedGeometry::GeneratedTag) &&
			NavigationBounds->Tags.Contains(TEXT("ProjectWorld.RuntimeRole=TerritoryNavigation")));
		const FBox Bounds = NavigationBounds->GetComponentsBoundingBox(true);
		TestTrue(TEXT("Territory navigation covers both canonical cells."),
			Bounds.IsValid && Bounds.Min.X <= 0.0 && Bounds.Max.X >= 20000.0 &&
			Bounds.Min.Y <= -10000.0 && Bounds.Max.Y >= 0.0);
		TestNull(TEXT("The map-owned domain is not a world-sized navigation invoker."),
			NavigationBounds->FindComponentByClass<UNavigationInvokerComponent>());
	}

	UNavigationSystemV1* Navigation = Cast<UNavigationSystemV1>(World->GetNavigationSystem());
	TestNotNull(TEXT("The product map has NavigationSystemV1."), Navigation);
	ARecastNavMesh* Recast = Navigation != nullptr
		? Cast<ARecastNavMesh>(Navigation->GetDefaultNavDataInstance(FNavigationSystem::DontCreate))
		: nullptr;
	TestNotNull(TEXT("The product map owns deterministic Recast data."), Recast);
	TestTrue(TEXT("Recast data remains in the map package."), Recast != nullptr && !Recast->IsPackageExternal());
	TestTrue(TEXT("Territory navigation generates only around runtime invokers."),
		Navigation != nullptr && Navigation->IsActiveTilesGenerationEnabled());
	TestTrue(TEXT("Territory Recast tiles fit the accepted large-world bounds."),
		Recast != nullptr && Recast->GetTileSizeUU() >= 4096.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldNoHLODPartitionPolicyTest,
	"Project.World.Realization.Runtime.NoHLODPartitionPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldNoHLODPartitionPolicyTest::RunTest(const FString& Parameters)
{
	UWorld* World = GEditor->NewMap(true);
	FString Error;
	AActor* Generated = World->SpawnActor<AActor>();
	Generated->Tags.Add(TEXT("ProjectWorld.Generated.v1"));
	Generated->bEnableAutoLODGeneration = true;
	TestTrue(
		TEXT("Production realization removes all default HLOD references."),
		ProjectWorldPartitionPolicy::DisableHLOD(World, Error));
	ProjectWorldPartitionPolicy::DisableGeneratedActorHLOD(World);
	TestEqual(
		TEXT("No World Partition HLOD layer remains."),
		ProjectWorldPartitionPolicy::CountHLODLayerReferences(World),
		0);
	TestFalse(TEXT("Generated actors cannot enter HLOD generation."), Generated->bEnableAutoLODGeneration);
	TestNull(TEXT("Generated actors retain no HLOD layer."), Generated->GetHLODLayer());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldRuntimeProfileSwitchLifecycleTest,
	"Project.World.Realization.Runtime.ProfileSwitchLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldRuntimeProfileSwitchLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldRuntimeTests;
	FProjectWorldRuntimeProfile Profile;
	FString ErrorCode;
	FString Error;
	if (!ProjectWorldRuntimeProfile::Load(ShippedProfilePath(), Profile, ErrorCode, Error))
	{
		AddError(Error);
		return false;
	}
	const FProjectWorldCanonicalBundle Bundle = MakeBundle(Profile);
	UWorld* World = GEditor->NewMap(false);
	ATargetPoint* RuntimeActor = World->SpawnActor<ATargetPoint>();
	RuntimeActor->Tags.Add(ProjectWorldGeneratedGeometry::GeneratedTag);
	RuntimeActor->Tags.Add(TEXT("ProjectWorld.RuntimeRole=RouteStart"));
	RuntimeActor->Tags.Add(TEXT("ProjectWorld.Runtime=baseline_profile"));
	RuntimeActor->Tags.Add(FName(*FString::Printf(TEXT("ProjectWorld.Grid=%s"), *Bundle.GridId)));
	FProjectWorldRealizationResult Result;
	TestTrue(
		TEXT("A runtime profile switch accepts the existing grid-owned actor for in-place update."),
		ProjectWorldGeneratedGeometry::RemoveStaleOwnedActorsForApply(
			World, Bundle, TEXT("candidate_profile"), Result));
	TestEqual(TEXT("A candidate profile does not delete stable runtime identity."), Result.RemovedActorCount, 0);
	TestTrue(TEXT("The runtime actor remains available to realization."), IsValid(RuntimeActor));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldStaticPartitionCellSpanTest,
	"Project.World.Realization.Runtime.StaticPartitionCellSpan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldStaticPartitionCellSpanTest::RunTest(const FString& Parameters)
{
	TestEqual(
		TEXT("Bounds wholly inside one runtime cell have one assignment."),
		ProjectWorldStaticPartitionAudit::CountIntersectedCells(
			FBox(FVector(100.0, 100.0, -100.0), FVector(1000.0, 1000.0, 100.0)), 128),
		1);
	TestEqual(
		TEXT("Bounds crossing one boundary on each axis have four assignments."),
		ProjectWorldStaticPartitionAudit::CountIntersectedCells(
			FBox(FVector(12000.0, 12000.0, -100.0), FVector(14000.0, 14000.0, 100.0)), 128),
		4);
	TestEqual(
		TEXT("A shifted 250 metre cell owner exposes nine assignments on a 128 metre grid."),
		ProjectWorldStaticPartitionAudit::CountIntersectedCells(
			FBox(FVector(6000.0, 6000.0, -100.0), FVector(31000.0, 31000.0, 100.0)), 128),
		9);
	TestEqual(
		TEXT("Invalid bounds cannot manufacture a passing cell assignment."),
		ProjectWorldStaticPartitionAudit::CountIntersectedCells(FBox(ForceInit), 128),
		0);
	return true;
}

#endif
