// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldAuthoredOverlay.h"
#include "ProjectWorldBuildingMeshBuilder.h"
#include "ProjectWorldCanonicalBundle.h"
#include "Misc/AutomationTest.h"
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

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldBuildingExteriorFacingTest,
	"Project.World.Realization.Buildings.ExteriorFacing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace ProjectWorldBuildingFacingTests
{
bool BuildFixture(FProjectWorldBuildingMeshBuildResult& Result, FString& Error)
{
	FProjectWorldCanonicalBundle Bundle;
	Bundle.GridId = TEXT("building_facing_test");
	Bundle.CoordinateQuantizationMeters = 0.01;
	Bundle.HeightQuantizationMeters = 0.1;
	FProjectWorldCanonicalCell Cell;
	Cell.CellId = TEXT("building_facing_test:a");
	Cell.Bounds = FVector4d(0.0, 0.0, 100.0, 100.0);
	Cell.Terrain.ArtifactHash = FString::ChrN(64, TEXT('a'));
	Cell.Terrain.Bounds = Cell.Bounds;
	Cell.Terrain.SampleSpacing = FVector2D(100.0, 100.0);
	Cell.Terrain.SamplesX = 2;
	Cell.Terrain.SamplesY = 2;
	Cell.Terrain.HeightsMeters = {2.0, 2.0, 2.0, 2.0};
	FProjectWorldCanonicalFeature Feature;
	Feature.FeatureId = TEXT("building/facing");
	Feature.FeatureClass = TEXT("building");
	Feature.OwnerCellId = Cell.CellId;
	Feature.GeometryType = TEXT("Polygon");
	Feature.HeightMeters = 12.0;
	FProjectWorldCanonicalPolygon Polygon;
	Polygon.Outer = {FVector2D(10, 10), FVector2D(40, 10), FVector2D(40, 40), FVector2D(10, 40), FVector2D(10, 10)};
	Feature.GeometryPolygons.Add(Polygon);
	FProjectWorldCanonicalBuildingVolume& Volume = Feature.BuildingVolumes.AddDefaulted_GetRef();
	Volume.VolumeId = TEXT("building/facing/volume");
	Volume.SourceFeatureId = Feature.FeatureId;
	Volume.GeometryType = TEXT("Polygon");
	Volume.GeometryPolygons.Add(Polygon);
	Volume.HeightMeters = 12.0;
	Cell.OwnedFeatureIds.Add(Feature.FeatureId);
	Bundle.Cells.Add(Cell);
	Bundle.Features.Add(Feature.FeatureId, Feature);
	return ProjectWorldBuildingMeshBuilder::BuildCell(
		Bundle, Cell, FProjectWorldAuthoredOverlaySet(), FProjectWorldBuildingSettings(), Result, Error);
}
}

bool FProjectWorldBuildingExteriorFacingTest::RunTest(const FString& Parameters)
{
	FProjectWorldBuildingMeshBuildResult Result;
	FString Error;
	if (!TestTrue(TEXT("The closed cube fixture builds."), ProjectWorldBuildingFacingTests::BuildFixture(Result, Error)))
	{
		AddError(Error);
		return false;
	}
	FStaticMeshAttributes Attributes(Result.MeshDescription);
	TestEqual(TEXT("The fixture emits every wall and cap."), Result.TriangleCount, 12);
	TestEqual(TEXT("The realized description contains every triangle."), Result.MeshDescription.Triangles().Num(), 12);
	TestEqual(TEXT("The intended building is admitted."), Result.Stats.AcceptedFragmentCount, 1);
	const auto Positions = Attributes.GetVertexPositions();
	const auto Normals = Attributes.GetVertexInstanceNormals();
	FBox Bounds(ForceInit);
	for (const FVertexID Vertex : Result.MeshDescription.Vertices().GetElementIDs())
	{
		Bounds += FVector(Positions[Vertex]);
	}
	int32 InwardFaces = 0;
	int32 NormalMismatches = 0;
	for (const FTriangleID Triangle : Result.MeshDescription.Triangles().GetElementIDs())
	{
		const TArrayView<const FVertexInstanceID> Instances = Result.MeshDescription.GetTriangleVertexInstances(Triangle);
		const FVector A(Positions[Result.MeshDescription.GetVertexInstanceVertex(Instances[0])]);
		const FVector B(Positions[Result.MeshDescription.GetVertexInstanceVertex(Instances[1])]);
		const FVector C(Positions[Result.MeshDescription.GetVertexInstanceVertex(Instances[2])]);
		// UE static-mesh build derives normals from clockwise triangle winding.
		const FVector NativeNormal = FVector::CrossProduct(C - A, B - A).GetSafeNormal();
		InwardFaces += FVector::DotProduct(NativeNormal, (A + B + C) / 3.0 - Bounds.GetCenter()) <= 0.0;
		for (const FVertexInstanceID Instance : Instances)
		{
			NormalMismatches += FVector::DotProduct(NativeNormal, FVector(Normals[Instance])) < 0.99;
		}
	}
	TestEqual(TEXT("All walls and caps face outside under UE winding."), InwardFaces, 0);
	TestEqual(TEXT("Stored normals agree with native compiled normals."), NormalMismatches, 0);
	TestNotEqual(TEXT("Corrected emitted order invalidates the captured wrong-winding output identity."),
		Result.SemanticDigest, FString(TEXT("8bef2292f5da14830e9eb7ec3c100666b0606f2b182afa87a43907d4cd95815d")));
	AddInfo(FString::Printf(TEXT("BuildingFacingDigest=%s Triangles=%d Inward=%d NormalMismatches=%d"),
		*Result.SemanticDigest, Result.TriangleCount, InwardFaces, NormalMismatches));
	return true;
}

REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FProjectWorldBuildingExteriorFacingTest,
	"Project.World.Realization.Buildings.ExteriorFacing", "[Fast][Unit][World]")

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldBuildingExteriorRenderTest,
	"Project.World.Realization.Buildings.ExteriorRender",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldBuildingExteriorRenderTest::RunTest(const FString& Parameters)
{
	FProjectWorldBuildingMeshBuildResult Build;
	FString Error;
	if (!TestTrue(TEXT("The render fixture builds."), ProjectWorldBuildingFacingTests::BuildFixture(Build, Error)))
	{
		AddError(Error);
		return false;
	}
	UWorld* World = GEditor->GetEditorWorldContext().World();
	UMaterial* Material = UMaterial::GetDefaultMaterial(MD_Surface);
	if (!TestNotNull(TEXT("An Editor world is available."), World) ||
		!TestFalse(TEXT("The neutral native control is one-sided."), Material->IsTwoSided()))
	{
		return false;
	}
	UStaticMesh* Mesh = NewObject<UStaticMesh>(GetTransientPackage());
	Mesh->GetStaticMaterials().Add(FStaticMaterial(Material));
	FMeshNaniteSettings Nanite = Mesh->GetNaniteSettings();
	Nanite.bEnabled = true;
	Mesh->SetNaniteSettings(Nanite);
	Mesh->bGenerateMeshDistanceField = false;
	Mesh->bHasNavigationData = false;
	Mesh->SetNumSourceModels(1);
	UStaticMesh::FBuildMeshDescriptionsParams BuildParameters;
	BuildParameters.bUseHashAsGuid = true;
	if (!TestTrue(TEXT("Native compilation succeeds."), Mesh->BuildFromMeshDescriptions({&Build.MeshDescription}, BuildParameters)))
	{
		return false;
	}
	FActorSpawnParameters Spawn;
	Spawn.ObjectFlags |= RF_Transient;
	const FVector Offset(1000000.0, -1000000.0, -100000.0);
	AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Offset, FRotator::ZeroRotator, Spawn);
	Actor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
	Actor->GetStaticMeshComponent()->UpdateBounds();
	const FBox Bounds = Actor->GetStaticMeshComponent()->Bounds.GetBox();
	ASceneCapture2D* Capture = World->SpawnActor<ASceneCapture2D>(ASceneCapture2D::StaticClass(),
		FVector(Bounds.GetCenter().X, Bounds.GetCenter().Y, Bounds.Max.Z + 350.0), FRotator(-90, 0, 0), Spawn);
	UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>(Capture);
	Target->RenderTargetFormat = RTF_RGBA32f;
	Target->InitAutoFormat(64, 64);
	Target->UpdateResourceImmediate(true);
	USceneCaptureComponent2D* Component = Capture->GetCaptureComponent2D();
	Component->TextureTarget = Target;
	Component->CaptureSource = SCS_SceneDepth;
	Component->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	Component->ShowOnlyComponent(Actor->GetStaticMeshComponent());
	Component->CaptureScene();
	TArray<FLinearColor> Pixels;
	const bool bRead = Target->GameThread_GetRenderTargetResource()->ReadLinearColorPixels(Pixels);
	double MaximumDepthError = 0.0;
	if (bRead && Pixels.Num() == 4096)
	{
		for (int32 Y = 30; Y < 34; ++Y)
		{
			for (int32 X = 30; X < 34; ++X)
			{
				MaximumDepthError = FMath::Max(MaximumDepthError, FMath::Abs(double(Pixels[Y * 64 + X].R) - 350.0));
			}
		}
	}
	Component->CaptureSource = SCS_Normal;
	Component->CaptureScene();
	TArray<FLinearColor> RoofNormals;
	const bool bRoofNormalsRead = Target->GameThread_GetRenderTargetResource()->ReadLinearColorPixels(RoofNormals);
	const bool bRoofNormalCorrect = bRoofNormalsRead && RoofNormals.Num() == 4096 && RoofNormals[32 * 64 + 32].B > 0.99f;
	Capture->SetActorLocation(FVector(Bounds.Max.X + 350.0, Bounds.GetCenter().Y, Bounds.GetCenter().Z));
	Capture->SetActorRotation(FRotator(0, 180, 0));
	Component->CaptureSource = SCS_SceneDepth;
	Component->CaptureScene();
	TArray<FLinearColor> WallDepth;
	const bool bWallRead = Target->GameThread_GetRenderTargetResource()->ReadLinearColorPixels(WallDepth);
	const double WallDepthError = bWallRead && WallDepth.Num() == 4096
		? FMath::Abs(double(WallDepth[32 * 64 + 32].R) - 350.0) : MAX_dbl;
	Component->CaptureSource = SCS_Normal;
	Component->CaptureScene();
	TArray<FLinearColor> WallNormals;
	const bool bWallNormalsRead = Target->GameThread_GetRenderTargetResource()->ReadLinearColorPixels(WallNormals);
	const bool bWallNormalCorrect = bWallNormalsRead && WallNormals.Num() == 4096 && WallNormals[32 * 64 + 32].R > 0.99f;
	Capture->SetActorRotation(FRotator(0, 0, 0));
	Component->CaptureSource = SCS_SceneDepth;
	Component->CaptureScene();
	TArray<FLinearColor> Miss;
	const bool bMissRead = Target->GameThread_GetRenderTargetResource()->ReadLinearColorPixels(Miss);
	const bool bWrongCameraRejected = bMissRead && Miss.Num() == 4096 &&
		FMath::Abs(double(Miss[32 * 64 + 32].R) - 350.0) > 100.0;
	World->DestroyActor(Capture);
	World->DestroyActor(Actor);
	TestTrue(TEXT("Native depth readback returns every pixel."), bRead && Pixels.Num() == 4096);
	TestTrue(TEXT("A one-sided compiled roof is visible from outside."), MaximumDepthError < 1.0);
	TestTrue(TEXT("A one-sided compiled wall is visible from outside."), WallDepthError < 1.0);
	TestTrue(TEXT("The compiled roof shader normal faces up."), bRoofNormalCorrect);
	TestTrue(TEXT("The compiled wall shader normal faces outside."), bWallNormalCorrect);
	TestTrue(TEXT("An away-facing camera fails the target-depth check."), bWrongCameraRejected);
	AddInfo(FString::Printf(TEXT("BuildingExteriorRenderDepthErrorCm=%.9g WallErrorCm=%.9g Pixels=%d RoofNormal=%d WallNormal=%d WrongCameraRejected=%d"),
		MaximumDepthError, WallDepthError, Pixels.Num(), bRoofNormalCorrect, bWallNormalCorrect, bWrongCameraRejected));
	return true;
}

REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FProjectWorldBuildingExteriorRenderTest,
	"Project.World.Realization.Buildings.ExteriorRender", "[Integration][World]")

#endif
