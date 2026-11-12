// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldAuthoredOverlay.h"
#include "ProjectWorldConstructedPlan.h"
#include "ProjectWorldConstructedMesh.h"
#include "ProjectWorldStoredSurface.h"
#include "ProjectWorldLayerProducerRegistry.h"
#include "ProjectWorldVerifyTestFlow.h"
#include "AssetCompilingManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshResources.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldConstructedImportedNeighborhoodTest,
	"Project.World.Realization.Constructed.ImportedNeighborhood",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldConstructedImportedNeighborhoodTest::RunTest(const FString&)
{
	FString Input,Hash,Actual,Error;
	if (!FParse::Value(FCommandLine::Get(),TEXT("ProjectWorldConstructedImport="),Input) ||
		!FParse::Value(FCommandLine::Get(),TEXT("ProjectWorldConstructedImportHash="),Hash) ||
		!FProjectWorldCanonicalLoader::ComputeFileSha256(Input,Actual) || Actual!=Hash)
	{AddError(TEXT("Supply the freshly source-qualified full structural comparison child and exact receipt hash.")); return false;}
	FProjectWorldCanonicalBundle Bundle; FProjectWorldCanonicalValidation Validation;
	if (!FProjectWorldCanonicalLoader::Load(Input,Bundle,Validation)) {AddError(Validation.Message); return false;}
	int32 Records=0,SourceFaces=0;
	for (const auto& Pair : Bundle.Features) if (Pair.Value.FeatureClass==TEXT("constructed_surface"))
	{
		++Records;
		if (!Pair.Value.ConstructedSurface.IsSet()) return false;
		SourceFaces+=Pair.Value.ConstructedSurface->Triangles.Num();
	}
	if (!TestEqual(TEXT("All four declared source parents/stubs survive canonical loading."),Records,4) ||
		!TestEqual(TEXT("The complete retained structural comparison field survives without Land aliasing."),SourceFaces,3103)) return false;
	const auto* Producer=ProjectWorldLayerProducerRegistry::Find(TEXT("project_constructed_surface"),1,Error);
	if (!Producer) {AddError(Error); return false;}
	FProjectWorldVerifyWorld Fixture(TEXT("ConstructedNeighborhood"));
	if (!Fixture.IsReady(Error)) {AddError(Error); return false;}
	FProjectWorldRealizationLayer Layer; Layer.LayerId=TEXT("structures"); Layer.GeneratorId=TEXT("project_constructed_surface"); Layer.GeneratorVersion=1;
	Layer.DirtyGranularity=EProjectWorldDirtyGranularity::CanonicalCell; Layer.CanonicalSelectors={TEXT("constructed_surfaces")};
	Layer.SpatialOwnership=TEXT("cell_local"); Layer.RuntimeMapping=TEXT("world_partition_spatial"); Layer.ArtifactRoot=Fixture.GetRoot();
	Layer.NormalizedSettings=TEXT("{\"maximum_join_step_m\":0.001,\"nanite\":true,\"collision\":\"complex_as_simple\",\"navigation\":\"physical_support\"}");
	Layer=ProjectWorldVerifyTestFlow::Finalize(MoveTemp(Layer));
	ProjectWorldConstructedRealization::FPlan Planned;
	if (!TestTrue(TEXT("Actual imported role policies admit the native physical units."),
		ProjectWorldConstructedRealization::Plan(Bundle,Layer,Planned,Error))) {AddError(Error); return false;}
	if (!TestEqual(TEXT("Two P25 cells and three independent apron/stub units are explicit."),Planned.Units.Num(),5)) return false;
	FProjectWorldRealizationProfile Profile; Profile.ProfileId=TEXT("verify_imported_neighborhood"); Profile.Layers={Layer};
	FProjectWorldAuthoredOverlaySet Overlays; const FProjectWorldLayerInputs Inputs{Bundle,Profile,Layer,Overlays};
	TMap<FName,UMaterialInterface*> Materials;
	Materials.Add(TEXT("road"),LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")));
	const TArray<FString> Dirty{TEXT("*")}; FProjectWorldLayerInventory Inventory=ProjectWorldVerifyTestFlow::WholeLayer(Layer);
	FProjectWorldRealizationResult Result;
	{
		const FProjectWorldLayerApplyContext Context{*Fixture.GetWorld(),Inputs,Dirty,Materials};
		if (!Producer->Apply(Context,Inventory,Result,Error) || !Producer->CaptureArtifacts(Context,Inventory,Result,Error) ||
			!Fixture.SaveOwned(Error) || !Fixture.Reload(Error)) {AddError(Error); return false;}
	}
	const FProjectWorldLayerApplyContext Reloaded{*Fixture.GetWorld(),Inputs,Dirty,Materials};
	if (!Producer->CaptureArtifacts(Reloaded,Inventory,Result,Error)) {AddError(Error); return false;}
	TestEqual(TEXT("All five physical units have saved mesh and actor artifacts."),Inventory.Artifacts.Num(),10);
	int32 Actors=0,Faces=0,CollisionProbes=0; double MaximumCollisionError=0;
	FVector CommonOrigin=FVector::ZeroVector; bool Anchored=false; TArray<FProjectWorldSurfaceTriangle> Combined;
	auto FaceKey=[](const FVector P[3])
	{
		TArray<FString> Points,Cycles;
		for (int32 I=0; I<3; ++I) Points.Add(FString::Printf(TEXT("%.17g,%.17g,%.17g"),P[I].X,P[I].Y,P[I].Z));
		for (int32 I=0; I<3; ++I) Cycles.Add(Points[I]+TEXT(";")+Points[(I+1)%3]+TEXT(";")+Points[(I+2)%3]);
		Cycles.Sort(); return Cycles[0];
	};
	for (TActorIterator<AStaticMeshActor> It(Fixture.GetWorld()); It; ++It)
	{
		if (!It->Tags.Contains(FName(TEXT("ProjectWorld.ConstructedSurface.v1")))) continue;
		++Actors; auto* Component=It->GetStaticMeshComponent(); UStaticMesh* Mesh=Component->GetStaticMesh();
		const auto* Description=Mesh ? Mesh->GetMeshDescription(0) : nullptr; const auto* Render=Mesh ? Mesh->GetRenderData() : nullptr;
		if (!Description || !Render || Render->LODResources.IsEmpty()) {AddError(TEXT("Saved neighborhood lost source or fallback geometry.")); return false;}
		const auto Positions=FStaticMeshConstAttributes(*Description).GetVertexPositions(); FProjectWorldStoredSurface Stored;
		Stored.Origin=It->GetActorLocation(); if (!Anchored) {CommonOrigin=Stored.Origin; Anchored=true;}
		TArray<FString> Source,Fallback;
		for (const auto T : Description->Triangles().GetElementIDs())
		{
			auto& Face=Stored.Triangles.AddDefaulted_GetRef();
			for (int32 I=0; I<3; ++I) Face.Points[I]=FVector(Positions[Description->GetVertexInstanceVertex(Description->GetTriangleVertexInstance(T,I))]);
			Source.Add(FaceKey(Face.Points)); ++Faces;
			const FVector Point=Component->GetComponentTransform().TransformPosition((Face.Points[0]+Face.Points[1]+Face.Points[2])/3.);
			FHitResult Hit; FCollisionQueryParams Query(SCENE_QUERY_STAT(ProjectWorldImportedStructuralContact),true);
			if (!Component->LineTraceComponent(Hit,Point+FVector(0,0,1000),Point-FVector(0,0,1000),Query))
			{AddError(TEXT("Actual saved collision misses a structural source-face interior.")); return false;}
			MaximumCollisionError=FMath::Max(MaximumCollisionError,FMath::Abs(Hit.ImpactPoint.Z-Point.Z)); ++CollisionProbes;
		}
		const auto& Lod=Render->LODResources[0]; const auto Indices=Lod.IndexBuffer.GetArrayView();
		for (int32 I=0; I+2<Indices.Num(); I+=3)
		{
			FVector P[3]; for (int32 J=0; J<3; ++J) P[J]=FVector(Lod.VertexBuffers.PositionVertexBuffer.VertexPosition(Indices[I+J]));
			Fallback.Add(FaceKey(P));
		}
		Source.Sort(); Fallback.Sort(); TestTrue(TEXT("Every saved fallback preserves oriented actual source faces."),!Source.IsEmpty() && Source==Fallback);
		TArray<FProjectWorldSurfaceTriangle> Reframed;
		if (!ProjectWorldStoredSurface::Reframe(Stored,CommonOrigin,Reframed,Error)) {AddError(Error); return false;}
		Combined.Append(Reframed);
	}
	TestEqual(TEXT("Reloaded physical actor census is complete."),Actors,5);
	TestEqual(TEXT("Source clipping retains the complete expected native face census."),Faces,3169);
	TestTrue(TEXT("All source-face collision probes meet the existing one-millimeter bound."),MaximumCollisionError<=.1);
	TestTrue(TEXT("Complete stored structural groups have no positive overlap or excessive shared-height step."),
		ProjectWorldConstructedMesh::ValidateStored(Combined,.1,Error));
	if (!Error.IsEmpty()) AddError(Error);
	const auto Before=Fixture.PackageDigests(); FProjectWorldRealizationResult NoOp;
	const TArray<FString> None; const FProjectWorldLayerApplyContext Clean{*Fixture.GetWorld(),Inputs,None,Materials};
	if (!Producer->Apply(Clean,Inventory,NoOp,Error)) {AddError(Error); return false;}
	TestTrue(TEXT("Clean source-bound neighborhood leaves every saved package unchanged."),Before.OrderIndependentCompareEqual(Fixture.PackageDigests()));
	AddInfo(FString::Printf(TEXT("Imported four source records, five physical units, %d stored faces and %d collision probes; maximum vertical error %.17g cm. Ground, Road delegation and full gameplay remain separate."),Faces,CollisionProbes,MaximumCollisionError));
	return !HasAnyErrors();
}
#endif
