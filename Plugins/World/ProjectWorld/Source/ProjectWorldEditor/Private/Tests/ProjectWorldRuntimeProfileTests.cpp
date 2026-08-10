// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldGeneratedGeometry.h"
#include "ProjectWorldRealizationService.h"
#include "ProjectWorldRuntimeProfile.h"
#include "ProjectWorldRuntimeRealization.h"
#include "ProjectWorldSemanticEvidence.h"
#include "Tests/ProjectWorldSchemaTestUtilities.h"

#include "Editor.h"
#include "Engine/TargetPoint.h"
#include "EngineUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ProjectWorldRuntimeTests
{
	FString ShippedProfilePath()
	{
		return FPaths::Combine(
			FPaths::ProjectPluginsDir(),
			TEXT("World/ProjectWorld/Data/Runtime/kazan_representative_playable_v1.json"));
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
	TestEqual(TEXT("Runtime profile identity."), Profile.ProfileId, FString(TEXT("kazan_representative_playable_v1")));
	TestEqual(TEXT("Runtime profile SHA-256 is complete."), Profile.ProfileHash.Len(), 64);
	TestEqual(TEXT("HLOD is explicitly disabled for the bounded procedural route."), Profile.HlodPolicy, FString(TEXT("disabled_for_bounded_route")));

	FProjectWorldCanonicalBundle Bundle = MakeBundle(Profile);
	TestTrue(TEXT("The pinned route is accepted by the matching canonical grid."), ProjectWorldRuntimeRealization::Validate(Bundle, Profile, Error));
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
	const FString Invalid = Source.Replace(TEXT("disabled_for_bounded_route"), TEXT("always_hlod"));
	TestTrue(TEXT("Invalid runtime profile fixture is writable."), FFileHelper::SaveStringToFile(Invalid, *InvalidPath));
	TestFalse(
		TEXT("Unmeasured HLOD policy cannot enter the bounded runtime profile."),
		ProjectWorldRuntimeProfile::Load(InvalidPath, Profile, ErrorCode, Error));
	TestEqual(TEXT("Optimization rejection is structured."), ErrorCode, FString(TEXT("runtime-profile-optimization")));

	const FString InvalidGrid = Source.Replace(TEXT("\"grid_id\": \"grid_"), TEXT("\"grid_id\": \"grid_Bad"));
	TestTrue(TEXT("Invalid-grid fixture is writable."), FFileHelper::SaveStringToFile(InvalidGrid, *InvalidPath));
	TestFalse(
		TEXT("The runtime loader enforces the schema grid identifier pattern."),
		ProjectWorldRuntimeProfile::Load(InvalidPath, Profile, ErrorCode, Error));
	TestEqual(TEXT("Grid rejection is structured."), ErrorCode, FString(TEXT("runtime-profile-contract")));

	const FString FractionalBudget = Source.Replace(
		TEXT("\"generated_actor_count\": 24"),
		TEXT("\"generated_actor_count\": 24.5"));
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
	FProjectWorldRuntimeRouteCollisionTest,
	"Project.World.Realization.Runtime.RouteCollision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldRuntimeRouteCollisionTest::RunTest(const FString& Parameters)
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
	FProjectWorldRealizationResult Result;
	TestTrue(
		TEXT("The pinned route realizes collision in both World Partition cells."),
		ProjectWorldGeneratedGeometry::CreateOwnedActors(
			World, Bundle, false, 1, 0, Result, Error, nullptr, Profile.RouteFeatureId));
	int32 RouteCellCount = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (!It->Tags.Contains(ProjectWorldGeneratedGeometry::GeneratedTag))
		{
			continue;
		}
		++RouteCellCount;
		const UProceduralMeshComponent* Mesh = It->FindComponentByClass<UProceduralMeshComponent>();
		TestNotNull(TEXT("Each route cell owns a procedural mesh."), Mesh);
		if (Mesh != nullptr)
		{
			TestEqual(TEXT("Route mesh blocks gameplay queries."), Mesh->GetCollisionEnabled(), ECollisionEnabled::QueryAndPhysics);
			TestTrue(TEXT("Route mesh contributes to navigation."), Mesh->CanEverAffectNavigation());
		}
		TestTrue(TEXT("Route cell is streamed by World Partition."), It->GetIsSpatiallyLoaded());
		TestFalse(TEXT("Procedural route actors do not claim unmeasured HLOD generation."), It->bEnableAutoLODGeneration);
	}
	TestEqual(TEXT("The route spans exactly two generated cells."), RouteCellCount, 2);
	const FProjectWorldCanonicalFeature& Route = Bundle.Features.FindChecked(Profile.RouteFeatureId);
	const double HalfWidth = FMath::Max(Route.WidthMeters * 0.5, 1.0);
	int32 CollisionProbeCount = 0;
	int32 OrientationProbeCount = 0;
	for (const FProjectWorldCanonicalRepresentation& Representation : Route.Representations)
	{
		const FVector2D Point = (Representation.Parts[0][0] + Representation.Parts[0].Last()) * 0.5;
		const FVector2D Tangent = (Representation.Parts[0].Last() - Representation.Parts[0][0]).GetSafeNormal();
		const FVector2D Perpendicular(-Tangent.Y, Tangent.X);
		const FName FeatureTag(*FString::Printf(TEXT("ProjectWorld.Feature=%s"), *Profile.RouteFeatureId));
		const FName CellTag(*FString::Printf(TEXT("ProjectWorld.Cell=%s"), *Representation.CellId));

		// Same discrimination as the runtime probe: the cell actor mixes terrain and road
		// collision, so only a hit inside the lifted road-surface band counts as the road.
		const auto TraceRoadBand = [&](const FVector2D& Canonical, FHitResult& OutHit) -> bool
		{
			const FVector Surface = FProjectWorldCanonicalLoader::CanonicalToUnreal(Bundle, FVector(Canonical, 0.65));
			const bool bHit = World->LineTraceSingleByChannel(
				OutHit,
				Surface + FVector(0.0, 0.0, 1000.0),
				Surface - FVector(0.0, 0.0, 1000.0),
				ECC_Visibility);
			return bHit && FMath::Abs(OutHit.ImpactPoint.Z - Surface.Z) <= 35.0;
		};

		FHitResult CenterHit;
		const bool bCenterOnRoad = TraceRoadBand(Point, CenterHit);
		TestTrue(TEXT("Each route representation has a collidable interior point."), bCenterOnRoad);
		TestTrue(
			TEXT("Each collision probe resolves to the expected route and cell."),
			bCenterOnRoad && CenterHit.GetActor() != nullptr &&
			CenterHit.GetActor()->Tags.Contains(FeatureTag) && CenterHit.GetActor()->Tags.Contains(CellTag));
		CollisionProbeCount += bCenterOnRoad ? 1 : 0;

		const double AlongOffset = 3.0 * HalfWidth;
		const double LateralInside = 0.5 * HalfWidth;
		const double LateralOutside = HalfWidth + FMath::Max(2.0, HalfWidth);
		bool bOrientationProven = bCenterOnRoad && CenterHit.ImpactNormal.Z >= 0.94;
		for (const FVector2D& OnRoad : {
			Point + Tangent * AlongOffset,
			Point - Tangent * AlongOffset,
			Point + Perpendicular * LateralInside,
			Point - Perpendicular * LateralInside})
		{
			FHitResult Hit;
			const bool bOnRoad = TraceRoadBand(OnRoad, Hit) && Hit.ImpactNormal.Z >= 0.94;
			TestTrue(TEXT("Road collision covers the declared ribbon along and across the tangent."), bOnRoad);
			bOrientationProven &= bOnRoad;
		}
		for (const FVector2D& OffRoad : {
			Point + Perpendicular * LateralOutside,
			Point - Perpendicular * LateralOutside})
		{
			FHitResult Hit;
			const bool bBeyondRoad = !TraceRoadBand(OffRoad, Hit);
			TestTrue(TEXT("Road collision does not extend beyond the declared half-width."), bBeyondRoad);
			bOrientationProven &= bBeyondRoad;
		}
		OrientationProbeCount += bOrientationProven ? 1 : 0;
	}
	TestEqual(TEXT("Collision is proven in both route fragment cells."), CollisionProbeCount, 2);
	TestEqual(TEXT("Collision orientation is proven in both route fragment cells."), OrientationProbeCount, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldRuntimeCleanupParityTest,
	"Project.World.Realization.Runtime.CleanupParity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldRuntimeCleanupParityTest::RunTest(const FString& Parameters)
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
	UWorld* TransitionedWorld = GEditor->NewMap(false);
	FProjectWorldRealizationResult InitialResult;
	TestTrue(
		TEXT("The transitioned map starts with runtime-route geometry."),
		ProjectWorldGeneratedGeometry::CreateOwnedActors(
			TransitionedWorld, Bundle, false, 1, 0, InitialResult, Error, nullptr, Profile.RouteFeatureId));
	ATargetPoint* RuntimeActor = TransitionedWorld->SpawnActor<ATargetPoint>();
	RuntimeActor->Tags.Add(ProjectWorldGeneratedGeometry::GeneratedTag);
	RuntimeActor->Tags.Add(TEXT("ProjectWorld.RuntimeRole=RouteStart"));
	RuntimeActor->Tags.Add(FName(*FString::Printf(TEXT("ProjectWorld.Runtime=%s"), *Profile.ProfileId)));
	RuntimeActor->Tags.Add(FName(*FString::Printf(TEXT("ProjectWorld.Grid=%s"), *Bundle.GridId)));
	FProjectWorldRealizationResult TransitionedResult;
	TestTrue(
		TEXT("Applying without a runtime profile removes prior runtime-role actors."),
		ProjectWorldGeneratedGeometry::RemoveStaleOwnedActorsForApply(
			TransitionedWorld, Bundle, FString(), false, TransitionedResult));
	TestEqual(TEXT("The stale runtime actor is removed."), TransitionedResult.RemovedActorCount, 1);
	TestTrue(
		TEXT("No-runtime Apply rebuilds the retained cell payload without route-only state."),
		ProjectWorldGeneratedGeometry::CreateOwnedActors(
			TransitionedWorld, Bundle, false, 1, 0, TransitionedResult, Error));
	TestTrue(
		TEXT("Transitioned no-runtime semantics are captured."),
		ProjectWorldSemanticEvidence::Capture(TransitionedWorld, TransitionedResult, Error));

	UWorld* CleanWorld = GEditor->NewMap(false);
	FProjectWorldRealizationResult CleanResult;
	TestTrue(
		TEXT("A clean no-runtime map creates the same canonical geometry."),
		ProjectWorldGeneratedGeometry::CreateOwnedActors(
			CleanWorld, Bundle, false, 1, 0, CleanResult, Error));
	TestTrue(
		TEXT("Clean no-runtime semantics are captured."),
		ProjectWorldSemanticEvidence::Capture(CleanWorld, CleanResult, Error));
	TestEqual(
		TEXT("Runtime-to-no-runtime Apply is semantically identical to a clean no-runtime Apply."),
		TransitionedResult.SemanticFingerprint,
		CleanResult.SemanticFingerprint);
	return true;
}

#endif
