#include "ProjectWorldMeshTerrainProducer.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldGeneratedActors.h"
#include "ProjectWorldTerrainSurface.h"
#include "ProjectWorldVerify.h"
#include "ProjectWorldMeshTerrainSurfaceFidelity.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "MeshPartitionMeshData.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Materials/Material.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshCompiler.h"
#include "StaticMeshResources.h"
#include "PrimitiveSceneProxy.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldRefinementTests
{
	bool Fixture(FAutomationTestBase& Test, FProjectWorldCanonicalBundle& Bundle, FString& Root)
	{
		Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("tmp/world/terrain_refinement_tests"), FGuid::NewGuid().ToString(EGuidFormats::Digits)));
		const FString Script = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("tools/World/CanonicalCompilation/tests/refinement_fixture.py")));
		int32 Code = -1; FString Out, Error;
		const bool bRan = FPlatformProcess::ExecProcess(TEXT("python.exe"), *FString::Printf(TEXT("-B \"%s\" \"%s\""), *Script, *Root), &Code, &Out, &Error);
		if (!Test.TestTrue(TEXT("Public canonical owner creates its isolated fixture."), bRan && Code == 0))
		{ Test.AddError(Out + Error); return false; }
		FProjectWorldCanonicalValidation Validation;
		const bool Loaded = FProjectWorldCanonicalLoader::Load(FPaths::Combine(Root, TEXT("candidate/compile_result.json")), Bundle, Validation);
		if (!Test.TestTrue(TEXT("The real typed loader authenticates the final surface."), Loaded))
		{ Test.AddError(Validation.ErrorCode + TEXT(": ") + Validation.Message + TEXT(" ") + Validation.Detail); return false; }
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldRefinedQueriesTest,
	"Project.World.Terrain.RefinedQueries", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldRefinedQueriesTest::RunTest(const FString&)
{
	FProjectWorldCanonicalBundle Bundle; FString Root, Error;
	if (!ProjectWorldRefinementTests::Fixture(*this, Bundle, Root)) return false;
	TestEqual(TEXT("Both selected cells load."), Bundle.Cells.Num(), 2);
	auto Nonplanar=Bundle.Cells[0];
	Nonplanar.Terrain.HeightsMeters={0,0,0,4};
	FProjectWorldCanonicalTerrainQuad Quad; Quad.Coordinate=FIntPoint(0,0);
	Quad.Vertices={FVector(0,30,0),FVector(30,30,0),FVector(0,0,0),FVector(30,0,4)};
	Quad.Triangles={FIntVector(0,2,1),FIntVector(1,2,3)};
	Quad.Weights.Init(FVector2f(1,0),4); Nonplanar.Terrain.RefinedQuads={Quad};
	double Center=0;
	TestTrue(TEXT("Nonplanar selected query is defined."),ProjectWorldGeneratedGeometry::SampleTerrain(Nonplanar,15,15,Center,Error));
	TestEqual(TEXT("Selected quad follows its final triangle diagonal."),Center,0.0);
	Nonplanar.Terrain.RefinedQuads.Reset();
	TestTrue(TEXT("Nonplanar unselected query preserves historical bilinear interpolation."),ProjectWorldGeneratedGeometry::SampleTerrain(Nonplanar,15,15,Center,Error));
	TestEqual(TEXT("Bilinear negative control differs from the final triangle."),Center,1.0);
	for (const auto& Cell : Bundle.Cells)
	{
		double Height = 0;
		const double X = Cell.Bounds.X + 15;
		TestTrue(TEXT("Placement query consumes final interior."), ProjectWorldGeneratedGeometry::SampleTerrain(Cell, X, 15, Height, Error));
		TestEqual(TEXT("Narrow Water no longer intersects terrain between samples."), Height, 10.0);
		TestTrue(TEXT("Outer domain preserves its original height."), ProjectWorldGeneratedGeometry::SampleTerrain(Cell, X, 30, Height, Error));
		TestEqual(TEXT("Unselected neighbor query boundary is continuous."), Height, 12.0);
		TArray<FProjectWorldSurfaceTriangle> Triangles;
		TestTrue(TEXT("Road sees the same final triangles."), ProjectWorldTerrainSurface::Triangles(Cell,
			FBox2D(FVector2D(X-0.1,14.9), FVector2D(X+0.1,15.1)), Triangles, Error));
		TestTrue(TEXT("Replacement triangle count exceeds the stale two-face grid."), Triangles.Num() > 2);
		auto Broken = Cell;
		Broken.Terrain.RefinedQuads[0].Triangles.Reset();
		TestFalse(TEXT("Missing replacement refuses without bilinear fallback."), ProjectWorldGeneratedGeometry::SampleTerrain(Broken, X, 15, Height, Error));
		Broken.Terrain.RefinedQuads.Reset();
		TestTrue(TEXT("Base-only query retains historical interpolation."), ProjectWorldGeneratedGeometry::SampleTerrain(Broken, X, 15, Height, Error));
		TestEqual(TEXT("Known-bad old surface retains the 2 m intrusion."), Height, 12.0);
	}
	for (const TCHAR* Case : {TEXT("bad_coverage"),TEXT("bad_weights"),TEXT("bad_duplicate"),TEXT("bad_crack")})
	{
		FProjectWorldCanonicalValidation Validation; FProjectWorldCanonicalBundle BrokenBundle;
		TestFalse(FString(Case)+TEXT(" refuses after receipt authentication."),FProjectWorldCanonicalLoader::Load(
			FPaths::Combine(Root,Case,TEXT("compile_result.json")),BrokenBundle,Validation));
		TestEqual(TEXT("The negative control fails geometry, not fixture creation or integrity."),Validation.ErrorCode,FString(TEXT("terrain-refinement")));
	}
	FProjectWorldVerifyIdentity Original; Original.AddFixtureBundle(TEXT("surface"),Bundle);
	for (int32 Field=0; Field<4; ++Field)
	{
		auto Changed=Bundle; auto& Q=Changed.Cells[0].Terrain.RefinedQuads[0];
		if (Field==0) ++Q.Coordinate.X;
		if (Field==1) Q.Vertices[0].Z+=0.1;
		if (Field==2) Q.Weights[0]=FVector2f(0.5f,0.5f);
		if (Field==3) Swap(Q.Triangles[0].X,Q.Triangles[0].Y);
		FProjectWorldVerifyIdentity Identity; Identity.AddFixtureBundle(TEXT("surface"),Changed);
		TestNotEqual(TEXT("Each refined typed field participates in fixture identity."),Original.FixtureInputs[0].Value,Identity.FixtureInputs[0].Value);
	}
	AddInfo(TEXT("RefinedQueryFixture=") + Root);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldMeshTerrainRefinedSurfaceTest,
	"Project.World.Realization.MeshTerrain.RefinedSurface", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldMeshTerrainRefinedSurfaceTest::RunTest(const FString&)
{
	using namespace UE::Geometry;
	FProjectWorldCanonicalBundle Bundle; FString Root, Error;
	if (!ProjectWorldRefinementTests::Fixture(*this, Bundle, Root)) return false;
	const auto& Cell = Bundle.Cells[0];
	FDynamicMesh3 Source;
	if (!TestTrue(TEXT("Adapter builds the canonical replacement."), ProjectWorldMeshTerrainProducer::BuildCellMesh(Bundle, Cell, Source, Error)))
	{ AddError(Error); return false; }
	TestTrue(TEXT("Source fidelity includes every final vertex and triangle."), ProjectWorldMeshTerrainProducer::MatchesCellMesh(Bundle, Cell, Source, Error));
	auto Stale = Cell; Stale.Terrain.RefinedQuads.Reset();
	FDynamicMesh3 Old;
	TestTrue(TEXT("Known-bad grid source builds independently."), ProjectWorldMeshTerrainProducer::BuildCellMesh(Bundle, Stale, Old, Error));
	TestFalse(TEXT("Old grid vertices cannot falsely certify refinement."), ProjectWorldMeshTerrainProducer::MatchesCellMesh(Bundle, Cell, Old, Error));
	auto Conflicting=Cell;
	Conflicting.Terrain.RefinedQuads[0].Weights[0]=FVector2f(0,1);
	FDynamicMesh3 Rejected;
	TestFalse(TEXT("Coincident grid/refined weights cannot be silently welded."),ProjectWorldMeshTerrainProducer::BuildCellMesh(Bundle,Conflicting,Rejected,Error));
	FProjectWorldCanonicalBundle OneCell=Bundle; OneCell.Cells={Cell};
	TArray<FProjectWorldSurfaceTriangle> FinalFaces, StaleFaces;
	const FBox2D Domain(FVector2D(Cell.Bounds.X,Cell.Bounds.Y),FVector2D(Cell.Bounds.Z,Cell.Bounds.W));
	if (!ProjectWorldTerrainSurface::Triangles(Cell,Domain,FinalFaces,Error) || !ProjectWorldTerrainSurface::Triangles(Stale,Domain,StaleFaces,Error))
	{ AddError(Error); return false; }
	FProjectWorldMeshTerrainSurfaceFidelity Fidelity;
	if (!TestTrue(TEXT("Complete final affine faces pass the independent fidelity check."),ProjectWorldMeshTerrainProducer::CompareRefinedSurface(OneCell,FinalFaces,1e-6,Fidelity,Error))) AddError(Error);
	TestFalse(TEXT("Stale original faces fail even with identical corner heights."),ProjectWorldMeshTerrainProducer::CompareRefinedSurface(OneCell,StaleFaces,1e-6,Fidelity,Error));
	auto Missing=FinalFaces; Missing.Pop();
	TestFalse(TEXT("Missing coverage fails independently of height tolerance."),ProjectWorldMeshTerrainProducer::CompareRefinedSurface(OneCell,Missing,1e-6,Fidelity,Error));
	const auto Duplicate=Missing[0]; Missing.Add(Duplicate);
	TestFalse(TEXT("Duplicate coverage cannot compensate for a missing face."),ProjectWorldMeshTerrainProducer::CompareRefinedSurface(OneCell,Missing,1e-6,Fidelity,Error));
	UE::MeshPartition::FMeshData Native;
	Native.AppendDynamicMesh(Source, FTransform::Identity);
	Native.RecomputeNormals();
	FMeshDescription Description;
	FStaticMeshAttributes(Description).Register();
	Native.ConvertToMeshDescription(Description);
	UStaticMesh* Mesh = NewObject<UStaticMesh>(GetTransientPackage());
	Mesh->GetStaticMaterials().Add(FStaticMaterial(UMaterial::GetDefaultMaterial(MD_Surface)));
	auto Nanite = Mesh->GetNaniteSettings(); Nanite.bEnabled = true;
	Nanite.FallbackPercentTriangles = 1.0f; Nanite.FallbackRelativeError = 0.0f;
	Mesh->SetNaniteSettings(Nanite);
	Mesh->bGenerateMeshDistanceField = false; Mesh->bHasNavigationData = false;
	Mesh->SetNumSourceModels(1); Mesh->CreateBodySetup();
	Mesh->GetBodySetup()->CollisionTraceFlag = CTF_UseComplexAsSimple;
	UStaticMesh::FBuildMeshDescriptionsParams Build;
	Build.bUseHashAsGuid = true;
	if (!TestTrue(TEXT("Native final static mesh compiles."), Mesh->BuildFromMeshDescriptions({&Description}, Build))) return false;
	FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
	TestTrue(TEXT("The rendered control has real compiled Nanite data."),Mesh->HasValidNaniteData());
	TArray<FProjectWorldSurfaceTriangle> Compiled;
	const auto& LOD=Mesh->GetRenderData()->LODResources[0];
	for (int32 I=0; I<LOD.IndexBuffer.GetNumIndices(); I+=3)
	{
		FProjectWorldSurfaceTriangle Face;
		for (int32 J=0; J<3; ++J) Face.Points[J]=FProjectWorldCanonicalLoader::UnrealToCanonical(Bundle,
			FVector(LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(LOD.IndexBuffer.GetIndex(I+J))));
		Compiled.Add(Face);
	}
	if (!TestTrue(TEXT("Actual compiled CPU fallback retains complete final coverage and heights."),
		ProjectWorldMeshTerrainProducer::CompareRefinedSurface(OneCell,Compiled,1e-4,Fidelity,Error))) AddError(Error);
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!TestNotNull(TEXT("Rendering fixture has a world."), World)) return false;
	FActorSpawnParameters Spawn; Spawn.ObjectFlags |= RF_Transient;
	const FVector Offset(1000000,-1000000,-10000);
	auto* Actor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Offset, FRotator::ZeroRotator, Spawn);
	auto* Component = Actor->GetStaticMeshComponent();
	Component->SetStaticMesh(Mesh); Component->SetCollisionProfileName(TEXT("BlockAll"));
	Component->RecreatePhysicsState(); Component->UpdateBounds();
	const FVector Camera = Offset + FProjectWorldCanonicalLoader::CanonicalToUnreal(Bundle, FVector(15,15,30));
	auto* Capture = World->SpawnActor<ASceneCapture2D>(ASceneCapture2D::StaticClass(), Camera, FRotator(-90,0,0), Spawn);
	auto* C = Capture->GetCaptureComponent2D();
	auto* Target = NewObject<UTextureRenderTarget2D>(Capture);
	Target->RenderTargetFormat = RTF_RGBA32f; Target->InitAutoFormat(64,64); Target->UpdateResourceImmediate(true);
	C->TextureTarget = Target; C->CaptureSource = SCS_SceneDepth;
	C->bCaptureEveryFrame = false; C->bCaptureOnMovement = false;
	C->ProjectionType = ECameraProjectionMode::Orthographic; C->OrthoWidth = 3000;
	C->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	C->ShowOnlyComponent(Component);
	const int32 VertexCount=Source.VertexCount(), TriangleCount=Source.TriangleCount();
	struct FRenderProof { int32 Frames=0; int32 Phase=0; double MaximumDepthError=0; };
	const auto Proof=MakeShared<FRenderProof>();
	// Proxy registration and native Nanite residency require actual engine frames.
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FFunctionLatentCommand>(
		[this,World,Actor,Capture,C,Target,Component,Camera,Offset,Root,VertexCount,TriangleCount,Proof]()
	{
	if (++Proof->Frames < 5) return false;
	const bool bNanite=Proof->Phase==0;
	const auto* Proxy=Component->GetSceneProxy();
	TestTrue(TEXT("Each rendered arm has a registered native proxy."),Proxy!=nullptr);
	if (Proxy) TestEqual(TEXT("Each rendered arm uses the actual Nanite/fallback proxy."),Proxy->IsNaniteMesh(),bNanite);
	{
		C->CaptureScene();
		TArray<FLinearColor> Pixels;
		const bool Read = Target->GameThread_GetRenderTargetResource()->ReadLinearColorPixels(Pixels);
		AddInfo(FString::Printf(TEXT("RefinedDepthControl nanite=%d pixels=%d center_depth_cm=%.9g"),bNanite,Pixels.Num(),Pixels.IsValidIndex(2080)?Pixels[2080].R:-1));
		TestTrue(TEXT("Rendered native/fallback readback is complete."), Read && Pixels.Num() == 4096);
		if (Read && Pixels.Num() == 4096) for (int32 Y=30; Y<34; ++Y) for (int32 X=30; X<34; ++X)
			Proof->MaximumDepthError = FMath::Max(Proof->MaximumDepthError, FMath::Abs(double(Pixels[Y*64+X].R)-2000));
	}
	if (bNanite)
	{
		Component->bDisallowNanite=true; Component->MarkRenderStateDirty();
		World->SendAllEndOfFrameUpdates(); Proof->Frames=0; ++Proof->Phase;
		return false;
	}
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(ProjectWorldRefinedTerrain), true);
	const bool HitSurface = Component->LineTraceComponent(Hit, Camera, Camera-FVector(0,0,4000), Query);
	TestTrue(TEXT("Compiled collision hits the final interior."), HitSurface);
	const double CollisionError = HitSurface ? FMath::Abs(Hit.ImpactPoint.Z - (Offset.Z + 1000)) : MAX_dbl;
	TestTrue(TEXT("Compiled Nanite and fallback retain the 2 m correction."), Proof->MaximumDepthError < 1);
	TestTrue(TEXT("Collision fidelity is measured separately."), CollisionError < 1);
	World->DestroyActor(Capture); World->DestroyActor(Actor);
	AddInfo(FString::Printf(TEXT("RefinedSurface vertices=%d triangles=%d depth_error_cm=%.9g collision_error_cm=%.9g fixture=%s"),
		VertexCount, TriangleCount, Proof->MaximumDepthError, CollisionError, *Root));
	return true;
	}));
	return true;
}
#endif
