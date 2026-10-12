// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldRoadRealization.h"
#include "ProjectWorldVerifyTestFlow.h"

#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "AssetCompilingManager.h"
#include "Interfaces/Interface_CollisionDataProvider.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshResources.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"

#if WITH_DEV_AUTOMATION_TESTS

// Fixture: a cross-cell and a single-cell primary road over a peaked cell and a flat cell, so the
// drape, the cell clipping, and the bound material are all observed.
namespace ProjectWorldRoadVerifyTest
{
	const ProjectWorldVerifyTestFlow::FSpec Spec{
		TEXT("Project.World.Realization.Verify.Road"), TEXT("Road"), TEXT("project_road_mesh"), 1};
	// Not the engine default surface material, so binding the default is an observable change.
	const TCHAR* RoadMaterial = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");

	FProjectWorldCanonicalCell MakeCell(int32 CellX, const FVector2D& Spacing, int32 SamplesX, const TArray<double>& Heights, TCHAR Hash)
	{
		FProjectWorldCanonicalCell Cell;
		Cell.CellId = FString::Printf(TEXT("cell_%d"), CellX);
		Cell.CellX = CellX;
		Cell.Bounds = FVector4d(CellX * 10.0, 0.0, (CellX + 1) * 10.0, 10.0);
		Cell.Terrain.ArtifactHash = FString::ChrN(64, Hash);
		Cell.Terrain.Bounds = Cell.Bounds;
		Cell.Terrain.SampleSpacing = Spacing;
		Cell.Terrain.SamplesX = SamplesX;
		Cell.Terrain.SamplesY = 2;
		Cell.Terrain.HeightsMeters = Heights;
		return Cell;
	}

	FProjectWorldCanonicalFeature MakeRoad(const FString& Id, double Width)
	{
		FProjectWorldCanonicalFeature Road;
		Road.FeatureId = Id;
		Road.FeatureClass = TEXT("road");
		Road.RoadClass = TEXT("primary");
		Road.WidthMeters = Width;
		Road.OwnerCellId = TEXT("cell_0");
		return Road;
	}

	void AddFragment(FProjectWorldCanonicalFeature& Road, const FString& CellId, const FVector2D& Start, const FVector2D& End)
	{
		FProjectWorldCanonicalRepresentation& Representation = Road.Representations.AddDefaulted_GetRef();
		Representation.CellId = CellId;
		Representation.Kind = TEXT("road_fragment");
		Representation.Parts.Add({Start, End});
		Road.GeometryParts.Add({Start, End});
	}

	FProjectWorldCanonicalBundle MakeBundle()
	{
		FProjectWorldCanonicalBundle Bundle;
		Bundle.GridId = TEXT("road_grid");
		Bundle.InputsHash = TEXT("road_input");
		Bundle.CoordinateQuantizationMeters = 0.01;
		Bundle.HeightQuantizationMeters = 0.1;
		Bundle.Cells.Add(MakeCell(0, FVector2D(5.0, 10.0), 3, {0.0, 10.0, 0.0, 0.0, 10.0, 0.0}, TEXT('a')));
		Bundle.Cells.Add(MakeCell(1, FVector2D(10.0, 10.0), 2, {0.0, 0.0, 0.0, 0.0}, TEXT('b')));
		FProjectWorldCanonicalFeature CrossCell = MakeRoad(TEXT("alis:test:road:cross-cell"), 4.0);
		AddFragment(CrossCell, TEXT("cell_0"), FVector2D(5.0, 5.0), FVector2D(10.0, 5.0));
		AddFragment(CrossCell, TEXT("cell_1"), FVector2D(10.0, 5.0), FVector2D(15.0, 5.0));
		FProjectWorldCanonicalFeature SingleCell = MakeRoad(TEXT("alis:test:road:single-cell"), 3.0);
		AddFragment(SingleCell, TEXT("cell_0"), FVector2D(1.0, 2.0), FVector2D(4.0, 2.0));
		Bundle.Cells[0].OwnedFeatureIds = {CrossCell.FeatureId, SingleCell.FeatureId};
		Bundle.Cells[1].ReferencedFeatureIds = {CrossCell.FeatureId};
		Bundle.Features.Add(CrossCell.FeatureId, MoveTemp(CrossCell));
		Bundle.Features.Add(SingleCell.FeatureId, MoveTemp(SingleCell));
		return Bundle;
	}

	FProjectWorldRealizationProfile MakeProfile()
	{
		FProjectWorldRealizationLayer Layer;
		Layer.LayerId = TEXT("roads");
		Layer.LayerKind = EProjectWorldLayerKind::GeneratedGeography;
		Layer.GeneratorId = TEXT("project_road_mesh");
		Layer.GeneratorVersion = 1;
		Layer.DependsOn = {TEXT("terrain")};
		Layer.CanonicalSelectors = {TEXT("roads")};
		Layer.ArtifactRoot = Spec.ArtifactRoot();
		Layer.SpatialOwnership = TEXT("cell_local");
		Layer.DirtyGranularity = EProjectWorldDirtyGranularity::CanonicalCell;
		Layer.DependencyHaloCells = 1;
		Layer.RuntimeMapping = TEXT("world_partition_spatial");
		Layer.NormalizedSettings =
			TEXT("{\"collision\":\"complex_as_simple\",\"intersection_policy\":\"overlap_same_owner\",")
			TEXT("\"maximum_segment_length_m\":7.5,\"nanite\":true,\"selected_classes\":[\"primary\",\"primary_link\",")
			TEXT("\"secondary\",\"secondary_link\",\"tertiary\",\"tertiary_link\",\"residential\",\"unclassified\",")
			TEXT("\"living_street\"],\"structure_fallback\":\"terrain_drape\",\"surface_offset_m\":0.15}");
		FProjectWorldRealizationProfile Profile;
		Profile.ProfileId = TEXT("verify_road");
		Profile.Layers.Add(ProjectWorldVerifyTestFlow::Finalize(MoveTemp(Layer)));
		return Profile;
	}

	FString BoundMaterial(UWorld* World)
	{
		for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
		{
			FString CellId;
			FString Semantic;
			if (!ProjectWorldRoadRealization::ReadActorIdentity(*It, CellId, Semantic))
			{
				continue;
			}
			const UStaticMesh* Mesh = It->GetStaticMeshComponent()->GetStaticMesh();
			if (Mesh != nullptr && Mesh->GetStaticMaterials().Num() > 0 &&
				Mesh->GetStaticMaterials()[0].MaterialInterface != nullptr)
			{
				return Mesh->GetStaticMaterials()[0].MaterialInterface->GetPathName();
			}
		}
		return FString();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldRoadVerifyTest,
	"Project.World.Realization.Verify.Road",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldRoadVerifyTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldRoadVerifyTest;
	const FProjectWorldCanonicalBundle Bundle = MakeBundle();
	const FProjectWorldRealizationProfile Profile = MakeProfile();
	return ProjectWorldVerifyTestFlow::Run(*this, Spec,
		[&](FProjectWorldVerifyIdentity& Identity, FString& OutError)
		{
			Identity.AddFixtureBundle(TEXT("memory:bundle"), Bundle);
			Identity.AddFixtureText(TEXT("memory:profile"), ProjectWorldVerifyTestFlow::SerializeProfile(Profile));
			return Identity.AddFixtureObject(RoadMaterial, OutError);
		},
		[&](UWorld* World, FString& OutError)
		{
			UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, RoadMaterial);
			FProjectWorldRealizationResult Result;
			Result.LayerInventories.Add(ProjectWorldVerifyTestFlow::WholeLayer(Profile.Layers[0]));
			return ProjectWorldRoadRealization::Apply(World, Bundle, Profile, Material, Result, OutError);
		});
}

REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FProjectWorldRoadVerifyTest,
	"Project.World.Realization.Verify.Road",
	"[Slow][Integration][World]")

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldRoadMaterialIdentityTest,
	"Project.World.Realization.Identity.RoadMaterialChangeDirtiesRoads",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldRoadMaterialIdentityTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldRoadVerifyTest;
	const FProjectWorldCanonicalBundle Bundle = MakeBundle();
	const FProjectWorldRealizationProfile Profile = MakeProfile();
	const TCHAR* Replacement = TEXT("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial");
	UMaterialInterface* FirstMaterial = LoadObject<UMaterialInterface>(nullptr, RoadMaterial);
	UMaterialInterface* SecondMaterial = LoadObject<UMaterialInterface>(nullptr, Replacement);
	FProjectWorldVerifyWorld World(Spec.Folder);
	FString Error;
	if (!TestTrue(TEXT("Road identity world and materials are available."),
		World.IsReady(Error) && FirstMaterial != nullptr && SecondMaterial != nullptr))
	{
		return false;
	}
	FProjectWorldRealizationResult First;
	First.LayerInventories.Add(ProjectWorldVerifyTestFlow::WholeLayer(Profile.Layers[0]));
	if (!TestTrue(TEXT("Initial road material is applied and saved."),
		ProjectWorldRoadRealization::Apply(World.GetWorld(), Bundle, Profile, FirstMaterial, First, Error) &&
		World.SaveOwned(Error) && World.Reload(Error)))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("Initial road mesh binds the first material."), BoundMaterial(World.GetWorld()), FString(RoadMaterial));
	FProjectWorldRealizationResult Second;
	FProjectWorldLayerInventory& Inventory = Second.LayerInventories.Add_GetRef(
		ProjectWorldVerifyTestFlow::WholeLayer(Profile.Layers[0]));
	Inventory.FinalDirtyUnits.Reset();
	if (!TestTrue(TEXT("Presentation-only road update is applied and reloaded."),
		ProjectWorldRoadRealization::Apply(World.GetWorld(), Bundle, Profile, SecondMaterial, Second, Error) &&
		World.SaveOwned(Error) && World.Reload(Error)))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("Changed presentation material reaches the saved road mesh."),
		BoundMaterial(World.GetWorld()), FString(Replacement));
	return true;
}

REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FProjectWorldRoadMaterialIdentityTest,
	"Project.World.Realization.Identity.RoadMaterialChangeDirtiesRoads",
	"[Slow][Integration][World]")

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldRoadConformityTest,
	"Project.World.Realization.Road.FullFootprintConformity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldRoadConformityTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldRoadVerifyTest;
	FProjectWorldCanonicalBundle Bundle = MakeBundle();
	Bundle.Cells.SetNum(1);
	Bundle.Cells[0] = MakeCell(0, FVector2D(30, 30), 2, {0, 0, 0, 10}, TEXT('a'));
	Bundle.Cells[0].Bounds = Bundle.Cells[0].Terrain.Bounds = FVector4d(0, 0, 30, 30);
	FProjectWorldCanonicalFeature Road = MakeRoad(TEXT("fixture:diagonal"), 4);
	AddFragment(Road, TEXT("cell_0"), FVector2D(5, 8), FVector2D(25, 22));
	Bundle.Features.Reset();
	Bundle.Cells[0].OwnedFeatureIds = {Road.FeatureId};
	Bundle.Features.Add(Road.FeatureId, MoveTemp(Road));
	FMeshDescription Mesh;
	FVector Origin;
	FString Error;
	if (!TestTrue(TEXT("Production road description builds without saving."),
		ProjectWorldRoadRealization::BuildDescription(Bundle, Bundle.Cells[0],
			MakeProfile().Layers[0], Mesh, Origin, Error))) { AddError(Error); return false; }
	const auto Positions = FStaticMeshConstAttributes(Mesh).GetVertexPositions();
	double MaximumError = 0;
	auto Check = [&](const FVector& Point)
	{
		// This fixture's two planes are z=0 for x<=y, z=(x-y)/3 for x>=y.
		const double Expected = FMath::Max(0.0, (Point.X - Point.Y) / 3.0) + 0.15;
		MaximumError = FMath::Max(MaximumError, FMath::Abs(Point.Z - Expected));
	};
	for (const FTriangleID Triangle : Mesh.Triangles().GetElementIDs())
	{
		FVector Points[3];
		for (int32 I = 0; I < 3; ++I)
		{
			const auto Vertex = Mesh.GetVertexInstanceVertex(Mesh.GetTriangleVertexInstance(Triangle, I));
			Points[I] = FProjectWorldCanonicalLoader::UnrealToCanonical(Bundle, FVector(Positions[Vertex]) + Origin);
			Check(Points[I]);
		}
		const FVector Cross = FVector::CrossProduct(Points[1] - Points[0], Points[2] - Points[0]);
		TestTrue(TEXT("Canonical upward surface has the expected winding."), Cross.Z > 0);
		const auto Normals = FStaticMeshConstAttributes(Mesh).GetVertexInstanceNormals();
		for (int32 I = 0; I < 3; ++I)
			TestTrue(TEXT("Road shading normals point above the terrain."), Normals[Mesh.GetTriangleVertexInstance(Triangle, I)].Z > 0);
		for (int32 I = 0; I < 3; ++I)
		{
			const FVector& A = Points[I]; const FVector& B = Points[(I + 1) % 3];
			const double DA = A.X - A.Y, DB = B.X - B.Y;
			if (DA * DB < 0) Check(FMath::Lerp(A, B, DA / (DA - DB)));
		}
	}
	TestTrue(TEXT("Description has nonempty coverage."), Mesh.Triangles().Num() > 0);
	TestTrue(*FString::Printf(TEXT("Continuous deviation is %.9g m, below 0.0001 m."), MaximumError), MaximumError < 0.0001);
	FProjectWorldVerifyWorld World(TEXT("RoadConformity"));
	auto Profile = MakeProfile();
	Profile.Layers[0].ArtifactRoot = TEXT("/ProjectWorldVerify/RoadConformity/Generated/");
	Profile.Layers[0] = ProjectWorldVerifyTestFlow::Finalize(MoveTemp(Profile.Layers[0]));
	FProjectWorldRealizationResult Result;
	Result.LayerInventories.Add(ProjectWorldVerifyTestFlow::WholeLayer(Profile.Layers[0]));
	if (!TestTrue(TEXT("Disposable production road builds, saves and reloads."), World.IsReady(Error) &&
		ProjectWorldRoadRealization::Apply(World.GetWorld(), Bundle, Profile,
			LoadObject<UMaterialInterface>(nullptr, RoadMaterial), Result, Error) && World.SaveOwned(Error) && World.Reload(Error)))
	{ AddError(Error); return false; }
	int32 CheckedActors = 0;
	for (TActorIterator<AStaticMeshActor> It(World.GetWorld()); It; ++It)
	{
		FString CellId, Semantic;
		if (!ProjectWorldRoadRealization::ReadActorIdentity(*It, CellId, Semantic)) continue;
		++CheckedActors;
		UStaticMesh* Saved = It->GetStaticMeshComponent()->GetStaticMesh();
		FAssetCompilingManager::Get().FinishAllCompilation();
		if (!TestNotNull(TEXT("Saved mesh is loaded."), Saved)) return false;
		auto CheckTriangle = [&](const FVector Local[3]) -> double
		{
			FVector P[3];
			for (int32 I = 0; I < 3; ++I) { P[I] = FProjectWorldCanonicalLoader::UnrealToCanonical(Bundle, Local[I] + It->GetActorLocation()); Check(P[I]); }
			for (int32 I = 0; I < 3; ++I)
			{
				const auto& A = P[I]; const auto& B = P[(I + 1) % 3];
				const double DA = A.X - A.Y, DB = B.X - B.Y;
				if (DA * DB < 0) Check(FMath::Lerp(A, B, DA / (DA - DB)));
			}
			return FMath::Abs((P[1].X - P[0].X) * (P[2].Y - P[0].Y) -
				(P[1].Y - P[0].Y) * (P[2].X - P[0].X)) * 0.5;
		};
		const auto* Render = Saved->GetRenderData();
		if (!TestTrue(TEXT("Compiled fallback render surface exists."), Render && !Render->LODResources.IsEmpty())) return false;
		const auto& LOD = Render->LODResources[0];
		const auto Indices = LOD.IndexBuffer.GetArrayView();
		TestTrue(TEXT("Compiled render coverage is nonempty."), Indices.Num() >= 3);
		double RenderArea = 0;
		for (int32 I = 0; I + 2 < Indices.Num(); I += 3)
		{
			const FVector P[3] = {FVector(LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(Indices[I])),
				FVector(LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(Indices[I + 1])), FVector(LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(Indices[I + 2]))};
			RenderArea += CheckTriangle(P);
		}
		FTriMeshCollisionData Collision;
		if (!TestTrue(TEXT("Reloaded complex collision has triangle coverage."), Saved->GetPhysicsTriMeshData(&Collision, true) && !Collision.Indices.IsEmpty())) return false;
		double CollisionArea = 0;
		for (const auto& T : Collision.Indices)
		{
			const FVector P[3] = {FVector(Collision.Vertices[T.v0]), FVector(Collision.Vertices[T.v1]), FVector(Collision.Vertices[T.v2])};
			CollisionArea += CheckTriangle(P);
		}
		const double ExpectedArea = 4.0 * FMath::Sqrt(20.0 * 20.0 + 14.0 * 14.0);
		TestTrue(TEXT("Compiled render covers the complete analytic ribbon area."), FMath::Abs(RenderArea - ExpectedArea) < 1e-4);
		TestTrue(TEXT("Collision covers the complete analytic ribbon area."), FMath::Abs(CollisionArea - ExpectedArea) < 1e-4);
		Saved->GetBodySetup()->CreatePhysicsMeshes();
		for (const FVector2D XY : {FVector2D(10, 12), FVector2D(15, 15), FVector2D(20, 18)})
		{
			FHitResult Hit;
			FCollisionQueryParams Query; Query.bTraceComplex = true;
			const FVector Start = FProjectWorldCanonicalLoader::CanonicalToUnreal(Bundle, FVector(XY, 20));
			const FVector End = FProjectWorldCanonicalLoader::CanonicalToUnreal(Bundle, FVector(XY, -10));
			if (TestTrue(TEXT("Cooked disposable road collision hits the ribbon."), It->GetStaticMeshComponent()->LineTraceComponent(Hit, Start, End, Query)))
				Check(FProjectWorldCanonicalLoader::UnrealToCanonical(Bundle, Hit.ImpactPoint));
		}
	}
	TestEqual(TEXT("Only the fixture road actor is checked."), CheckedActors, 1);
	FProjectWorldRealizationResult NoOp;
	NoOp.LayerInventories.Add(ProjectWorldVerifyTestFlow::WholeLayer(Profile.Layers[0]));
	NoOp.LayerInventories[0].FinalDirtyUnits.Reset();
	TestTrue(TEXT("An unchanged disposable road is a no-op."), ProjectWorldRoadRealization::Apply(World.GetWorld(), Bundle, Profile,
		LoadObject<UMaterialInterface>(nullptr, RoadMaterial), NoOp, Error));
	TestEqual(TEXT("No-op creates no actors."), NoOp.CreatedActorCount, 0);
	TestEqual(TEXT("No-op updates no actors."), NoOp.UpdatedActorCount, 0);
	TestEqual(TEXT("No-op saves no actors."), NoOp.SelfSavedActorMutationCount, 0);
	TestTrue(*FString::Printf(TEXT("Source, compiled fallback and collision deviation %.9g m < 0.0001 m."), MaximumError), MaximumError < 0.0001);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldRoadContextIdentityTest,
	"Project.World.Realization.Road.ConsumedGeometryIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldRoadContextIdentityTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldRoadVerifyTest;
	FProjectWorldCanonicalBundle Bundle = MakeBundle();
	const auto Layer = MakeProfile().Layers[0];
	FString Initial, Changed, Error;
	TestTrue(TEXT("Initial unit hash succeeds."), ProjectWorldRoadRealization::HashCellInput(Bundle, Bundle.Cells[0], Layer, Initial, Error));
	Bundle.Features[TEXT("alis:test:road:cross-cell")].GeometryParts[0].Add(FVector2D(10, 10));
	TestTrue(TEXT("Changed local tangent context hashes."), ProjectWorldRoadRealization::HashCellInput(Bundle, Bundle.Cells[0], Layer, Changed, Error));
	TestNotEqual(TEXT("Consumed tangent continuation invalidates unit."), Initial, Changed);
	Bundle = MakeBundle();
	Bundle.Features[TEXT("alis:test:road:cross-cell")].GeometryParts.Add({FVector2D(1000, 1000), FVector2D(1010, 1010)});
	TestTrue(TEXT("Remote context hashes."), ProjectWorldRoadRealization::HashCellInput(Bundle, Bundle.Cells[0], Layer, Changed, Error));
	TestEqual(TEXT("Unconsumed remote geometry retains identity."), Initial, Changed);
	Bundle = MakeBundle();
	auto& Cross = Bundle.Features[TEXT("alis:test:road:cross-cell")];
	Cross.GeometryParts = {{FVector2D(5, 5), FVector2D(10, 8)}, {FVector2D(10, 8), FVector2D(15, 9)}};
	Cross.Representations[0].Parts = {Cross.GeometryParts[0]};
	Cross.Representations[1].Parts = {Cross.GeometryParts[1]};
	TestTrue(TEXT("Neighbor-width footprint hashes."), ProjectWorldRoadRealization::HashCellInput(Bundle, Bundle.Cells[0], Layer, Initial, Error));
	Bundle.Cells[1].Terrain.ArtifactHash = FString::ChrN(64, TEXT('c'));
	TestTrue(TEXT("Consumed neighbor terrain hashes."), ProjectWorldRoadRealization::HashCellInput(Bundle, Bundle.Cells[0], Layer, Changed, Error));
	TestNotEqual(TEXT("Width outside the cell authenticates neighbor terrain."), Initial, Changed);
	TMap<FString, FString> Units;
	TestTrue(TEXT("Indexed batch identity succeeds."), ProjectWorldRoadRealization::HashUnitInputs(Bundle, Layer, Units, Error));
	TestEqual(TEXT("Batch and single-unit context are identical."), Units.FindChecked(Bundle.Cells[0].CellId), Changed);
	const double Started = FPlatformTime::Seconds();
	for (int32 I = 0; I < 100; ++I)
		if (!ProjectWorldRoadRealization::HashUnitInputs(Bundle, Layer, Units, Error)) { AddError(Error); return false; }
	AddInfo(FString::Printf(TEXT("Indexed 200 fixture unit hashes: %.6f seconds; no mesh emission."), FPlatformTime::Seconds() - Started));
	Bundle.Cells.Add(MakeCell(2, FVector2D(10, 10), 2, {0, 0, 0, 0}, TEXT('d')));
	Bundle.Cells.RemoveAt(1);
	FMeshDescription Missing;
	FVector Origin;
	TestFalse(TEXT("Missing interior neighbor coverage refuses."), ProjectWorldRoadRealization::BuildDescription(Bundle, Bundle.Cells[0], Layer, Missing, Origin, Error));
	TestEqual(TEXT("Refusal emits no partial mesh."), Missing.Triangles().Num(), 0);
	Bundle = MakeBundle();
	Bundle.Cells.SetNum(1);
	auto Edge = MakeRoad(TEXT("fixture:outer-edge"), 4);
	AddFragment(Edge, TEXT("cell_0"), FVector2D(0, 5), FVector2D(10, 8));
	Bundle.Features.Reset(); Bundle.Cells[0].OwnedFeatureIds = {Edge.FeatureId};
	Bundle.Features.Add(Edge.FeatureId, MoveTemp(Edge));
	FMeshDescription Clipped;
	TestTrue(TEXT("Width at the admitted outer edge clips successfully."), ProjectWorldRoadRealization::BuildDescription(Bundle, Bundle.Cells[0], Layer, Clipped, Origin, Error));
	TestTrue(TEXT("Outer clipping retains coverage."), Clipped.Triangles().Num() > 0);
	for (const auto& Id : Clipped.Vertices().GetElementIDs())
	{
		const auto P = FProjectWorldCanonicalLoader::UnrealToCanonical(Bundle, FVector(FStaticMeshConstAttributes(Clipped).GetVertexPositions()[Id]) + Origin);
		TestTrue(TEXT("Outer-edge vertices stay in admitted bounds."), P.X >= -1e-6 && P.X <= 10 + 1e-6 && P.Y >= -1e-6 && P.Y <= 10 + 1e-6);
	}
	return true;
}

#endif
