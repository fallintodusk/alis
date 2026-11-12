// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldAuthoredOverlay.h"
#include "ProjectWorldLayerProducerRegistry.h"
#include "ProjectWorldVerifyTestFlow.h"
#include "ProjectWorldConstructedPlan.h"
#include "AssetCompilingManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/Material.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshResources.h"
#include "Misc/PackageName.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "HAL/FileManager.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldConstructedSavedOutputTest,
	"Project.World.Realization.Constructed.RegisteredSourceOutput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldConstructedSavedOutputTest::RunTest(const FString&)
{
	FString Input,Hash,Actual,Error;
	if (!FParse::Value(FCommandLine::Get(),TEXT("ProjectWorldConstructedImport="),Input) ||
		!FParse::Value(FCommandLine::Get(),TEXT("ProjectWorldConstructedImportHash="),Hash) ||
		!FProjectWorldCanonicalLoader::ComputeFileSha256(Input,Actual) || Actual!=Hash)
	{AddError(TEXT("Supply a freshly generated source-faithful canonical import fixture and receipt hash.")); return false;}
	FProjectWorldCanonicalBundle Bundle; FProjectWorldCanonicalValidation Validation;
	if (!FProjectWorldCanonicalLoader::Load(Input,Bundle,Validation)) {AddError(Validation.Message); return false;}
	const auto* Producer=ProjectWorldLayerProducerRegistry::Find(TEXT("project_constructed_surface"),1,Error);
	if (!Producer) {AddError(Error); return false;}
	FProjectWorldVerifyWorld Fixture(TEXT("Constructed"));
	if (!Fixture.IsReady(Error)) {AddError(Error); return false;}
	FProjectWorldRealizationLayer Layer; Layer.LayerId=TEXT("structures"); Layer.GeneratorId=TEXT("project_constructed_surface"); Layer.GeneratorVersion=1;
	Layer.DirtyGranularity=EProjectWorldDirtyGranularity::CanonicalCell; Layer.CanonicalSelectors={TEXT("constructed_surfaces")};
	Layer.SpatialOwnership=TEXT("cell_local"); Layer.RuntimeMapping=TEXT("world_partition_spatial"); Layer.ArtifactRoot=Fixture.GetRoot();
	Layer.NormalizedSettings=TEXT("{\"maximum_join_step_m\":0.001,\"nanite\":true,\"collision\":\"complex_as_simple\",\"navigation\":\"physical_support\"}");
	Layer=ProjectWorldVerifyTestFlow::Finalize(MoveTemp(Layer));
	if (!TestTrue(TEXT("The actual registry validates the structural producer tuple."),ProjectWorldLayerProducerRegistry::ValidateLayer(Layer,Error)))
	{AddError(Error); return false;}
	FProjectWorldRealizationProfile Profile; Profile.ProfileId=TEXT("verify_constructed"); Profile.Layers={Layer};
	FProjectWorldAuthoredOverlaySet Overlays; FProjectWorldLayerInputs Inputs{Bundle,Profile,Layer,Overlays};
	FProjectWorldLayerUnitInputs Units;
	if (!TestTrue(TEXT("The registered owner hashes the actual imported cell domain."),Producer->HashUnitInputs(Inputs,Units,Error)))
	{AddError(Error); return false;}
	TestEqual(TEXT("Every canonical cell, including empty removal scope, participates in dirty dependency identity."),Units.HashByUnit.Num(),Bundle.Cells.Num());
	{
		auto Broken=Bundle; FProjectWorldLayerUnitInputs Refused;
		auto* Referenced=Broken.Cells.FindByPredicate([](const auto& Cell)
			{return Cell.ReferencedFeatureIds.Contains(TEXT("alis:osm:relation:20784500"));});
		if (!TestNotNull(TEXT("The imported source has an actual referenced membership cell."),Referenced)) return false;
		Referenced->ReferencedFeatureIds.Reset();
		const FProjectWorldLayerInputs BadInputs{Broken,Profile,Layer,Overlays};
		TestFalse(TEXT("A missing referenced-cell declaration cannot silently remove half a physical source."),Producer->HashUnitInputs(BadInputs,Refused,Error));
		TestTrue(TEXT("Incomplete membership publishes no partial dirty input census."),Refused.HashByUnit.IsEmpty());
		auto Contact=Bundle; auto& Feature=Contact.Features.FindChecked(TEXT("alis:osm:relation:20784500")); FBox2D Bounds(ForceInit);
		for (const auto& Face : Feature.ConstructedSurface->Triangles) for (const auto& Point : Face.XYZ) Bounds+=FVector2D(Point);
		FProjectWorldCanonicalCell Boundary; Boundary.CellId=TEXT("boundary_contact");
		Boundary.Bounds=FVector4d(Bounds.Max.X,Bounds.Min.Y,Bounds.Max.X+1,Bounds.Max.Y);
		Boundary.ReferencedFeatureIds={Feature.FeatureId}; Contact.Cells.Add(Boundary);
		FProjectWorldCanonicalRepresentation Representation; Representation.CellId=Boundary.CellId; Representation.Kind=TEXT("constructed_surface_reference");
		Feature.Representations.Add(Representation); ProjectWorldConstructedRealization::FPlan BoundaryPlan;
		if (!TestTrue(TEXT("A declared closed-boundary contact is valid without a physical fragment."),
			ProjectWorldConstructedRealization::Plan(Contact,Layer,BoundaryPlan,Error))) {AddError(Error); return false;}
		TestEqual(TEXT("Boundary-only contact creates no extra body mesh."),BoundaryPlan.Units.Num(),2);
		TestEqual(TEXT("The nonphysical boundary contribution remains explicit in the census."),BoundaryPlan.BoundaryContacts,1);
		TestEqual(TEXT("Boundary declarations remain dependency-visible alongside empty canonical cells."),BoundaryPlan.CellHashes.Num(),Bundle.Cells.Num()+1);
	}
	TArray<FString> Dirty{TEXT("*")}; TMap<FName,UMaterialInterface*> Materials;
	auto* Bound=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (!TestNotNull(TEXT("A distinct existing material is resolved through the public binding."),Bound)) return false;
	Materials.Add(TEXT("road"),Bound);
	FProjectWorldLayerInventory Inventory=ProjectWorldVerifyTestFlow::WholeLayer(Layer); FProjectWorldRealizationResult Result;
	{
		const FProjectWorldLayerApplyContext Context{*Fixture.GetWorld(),Inputs,Dirty,Materials};
		if (!TestTrue(TEXT("The registered owner realizes source-bound structural units."),Producer->Apply(Context,Inventory,Result,Error)))
		{AddError(Error); return false;}
		if (!TestTrue(TEXT("Actual output census and semantics authenticate before save/reload."),Producer->CaptureArtifacts(Context,Inventory,Result,Error)))
		{AddError(Error); return false;}
	}
	TestEqual(TEXT("One source parent crossing two cells creates exactly two physical units."),Inventory.Metrics.FindRef(TEXT("physical_units")),int64(2));
	if (!Fixture.SaveOwned(Error) || !Fixture.Reload(Error)) {AddError(Error); return false;}
	TArray<AStaticMeshActor*> Actors;
	for (TActorIterator<AStaticMeshActor> It(Fixture.GetWorld()); It; ++It)
		if (It->Tags.Contains(FName(TEXT("ProjectWorld.ConstructedSurface.v1")))) Actors.Add(*It);
	if (!TestEqual(TEXT("Reloaded external actors retain the exact physical-unit census."),Actors.Num(),2)) return false;
	const FProjectWorldLayerApplyContext Reloaded{*Fixture.GetWorld(),Inputs,Dirty,Materials};
	Inventory.Artifacts.Reset();
	if (!TestTrue(TEXT("Reloaded saved geometry, policy, material, collision and Nav state authenticate."),
		Producer->CaptureArtifacts(Reloaded,Inventory,Result,Error))) {AddError(Error); return false;}
	for (auto* Actor : Actors)
	{
		auto* Component=Actor->GetStaticMeshComponent(); UStaticMesh* Mesh=Component->GetStaticMesh();
		const auto* Description=Mesh->GetMeshDescription(0); const auto* Render=Mesh->GetRenderData();
		if (!TestNotNull(TEXT("The reloaded mesh retains CPU fallback output."),Render) || !Description || Render->LODResources.IsEmpty()) return false;
		auto Key = [](const FVector P[3])
		{
			TArray<FString> Points;
			for (const auto& V : TConstArrayView<FVector>(P,3)) Points.Add(FString::Printf(TEXT("%.17g,%.17g,%.17g"),V.X,V.Y,V.Z));
			TArray<FString> Cycles;
			for (int32 I=0; I<3; ++I) Cycles.Add(Points[I]+TEXT(";")+Points[(I+1)%3]+TEXT(";")+Points[(I+2)%3]);
			Cycles.Sort(); return Cycles[0];
		};
		TArray<FString> Source,Fallback; const auto Positions=FStaticMeshConstAttributes(*Description).GetVertexPositions();
		TArray<FVector> Probes;
		for (const auto Triangle : Description->Triangles().GetElementIDs())
		{
			FVector P[3];
			for (int32 I=0; I<3; ++I) P[I]=FVector(Positions[Description->GetVertexInstanceVertex(Description->GetTriangleVertexInstance(Triangle,I))]);
			Source.Add(Key(P)); Probes.Add((P[0]+P[1]+P[2])/3+Actor->GetActorLocation());
		}
		const auto& Lod=Render->LODResources[0]; const auto Indices=Lod.IndexBuffer.GetArrayView();
		for (int32 I=0; I<Indices.Num(); I+=3)
		{
			FVector P[3]; for (int32 J=0; J<3; ++J) P[J]=FVector(Lod.VertexBuffers.PositionVertexBuffer.VertexPosition(Indices[I+J]));
			Fallback.Add(Key(P));
		}
		Source.Sort(); Fallback.Sort(); TestTrue(TEXT("Actual saved full fallback preserves every oriented emitted source triangle and XYZ."),!Source.IsEmpty() && Source==Fallback);
		TestFalse(TEXT("Physical inspection does not enable production runtime CPU retention."),Mesh->bAllowCPUAccess);
		if (!TestTrue(TEXT("Physical source-face probes are nonempty."),!Probes.IsEmpty())) return false;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(ProjectWorldConstructedSourceContact),true);
		double MaximumCollisionError=0;
		for (const FVector& Probe : Probes)
		{
			FHitResult Hit,PawnHit;
			TestTrue(TEXT("Every saved source face provides actual complex collision."),
				Component->LineTraceComponent(Hit,Probe+FVector(0,0,1000),Probe-FVector(0,0,1000),Query));
			MaximumCollisionError=FMath::Max(MaximumCollisionError,FMath::Abs(Hit.ImpactPoint.Z-Probe.Z));
			TestTrue(TEXT("Every saved source face blocks its own Pawn-channel world query."),
				Fixture.GetWorld()->LineTraceSingleByChannel(PawnHit,Probe+FVector(0,0,1000),Probe-FVector(0,0,1000),ECC_Pawn,Query) &&
				PawnHit.GetComponent()==Component && FMath::Abs(PawnHit.ImpactPoint.Z-Probe.Z)<=.1);
		}
		TestTrue(TEXT("All complex source-face collision follows the zero-lift surface."),MaximumCollisionError<=.1);
		AddInfo(FString::Printf(TEXT("Constructed source collision probes=%d maximum_error_cm=%.17g"),Probes.Num(),MaximumCollisionError));
		const FVector Probe=Probes[0];
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision); FHitResult Disabled;
		TestFalse(TEXT("Disabled structural collision cannot borrow another world's hit as physical support."),
			Fixture.GetWorld()->LineTraceSingleByChannel(Disabled,Probe+FVector(0,0,1000),Probe-FVector(0,0,1000),ECC_Pawn,Query) && Disabled.GetComponent()==Component);
		Component->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Component->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	}
	auto Refuse = [&](const TCHAR* Meaning)
	{
		const auto Before=Fixture.PackageDigests(); const int32 Artifacts=Inventory.Artifacts.Num(); Error.Reset();
		TestFalse(Meaning,Producer->CaptureArtifacts(Reloaded,Inventory,Result,Error));
		TestFalse(TEXT("A refused physical output provides a diagnostic."),Error.IsEmpty());
		TestEqual(TEXT("A failed census does not publish partial artifacts."),Inventory.Artifacts.Num(),Artifacts);
		TestTrue(TEXT("Read-only refusal preserves all saved packages."),Before.OrderIndependentCompareEqual(Fixture.PackageDigests()));
	};
	auto* Component=Actors[0]->GetStaticMeshComponent(); UStaticMesh* Mesh=Component->GetStaticMesh();
	Component->SetCanEverAffectNavigation(false); Refuse(TEXT("Lost physical Nav support refuses.")); Component->SetCanEverAffectNavigation(true);
	Component->SetMaterial(0,UMaterial::GetDefaultMaterial(MD_Surface)); Refuse(TEXT("A substituted effective material refuses.")); Component->SetMaterial(0,Bound);
	const auto Collision=Mesh->GetBodySetup()->CollisionTraceFlag; Mesh->GetBodySetup()->CollisionTraceFlag=CTF_UseDefault;
	Refuse(TEXT("Lost complex physical collision refuses.")); Mesh->GetBodySetup()->CollisionTraceFlag=Collision;
	const auto Tags=Actors[0]->Tags;
	Actors[0]->Tags.Remove(FName(TEXT("ProjectWorld.ConstructedSurface.v1"))); Refuse(TEXT("A missing physical unit cannot satisfy the complete saved census.")); Actors[0]->Tags=Tags;
	Actors[0]->Tags.RemoveAll([](FName N){return N.ToString().StartsWith(TEXT("ProjectWorld.ConstructedModel="));});
	Actors[0]->Tags.Add(FName(*(TEXT("ProjectWorld.ConstructedModel=")+FString::ChrN(64,'f')))); Refuse(TEXT("A stale model binding refuses.")); Actors[0]->Tags=Tags;
	const FString ForeignName=Fixture.GetRoot()+TEXT("Foreign/SM_Duplicate"); auto* ForeignPackage=CreatePackage(*ForeignName);
	auto* Foreign=DuplicateObject<UStaticMesh>(Mesh,ForeignPackage,TEXT("SM_Duplicate"));
	FAssetCompilingManager::Get().FinishAllCompilation();
	if (!TestNotNull(TEXT("The ownership control supplies a distinct foreign mesh."),Foreign)) return false;
	FString OriginalGeometry,ForeignGeometry;
	if (!TestTrue(TEXT("The foreign control has the same native description and complete Nanite/physics state."),
		Foreign->GetMeshDescription(0) && ProjectWorldConstructedRealization::Describe(*Mesh->GetMeshDescription(0),OriginalGeometry) &&
		ProjectWorldConstructedRealization::Describe(*Foreign->GetMeshDescription(0),ForeignGeometry) && ForeignGeometry==OriginalGeometry &&
		Foreign->HasValidNaniteData() && Foreign->GetBodySetup() && Foreign->GetBodySetup()->bCreatedPhysicsMeshes)) return false;
	Foreign->SetMaterial(0,UMaterial::GetDefaultMaterial(MD_Surface));
	const FString ForeignFile=FPackageName::LongPackageNameToFilename(ForeignName,FPackageName::GetAssetPackageExtension());
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ForeignFile),true);
	FSavePackageArgs SaveArgs; SaveArgs.TopLevelFlags=RF_Public|RF_Standalone; SaveArgs.SaveFlags=SAVE_NoError;
	if (!UPackage::SavePackage(ForeignPackage,Foreign,*ForeignFile,SaveArgs)) return false;
	FString ForeignBefore,ForeignAfter; FProjectWorldCanonicalLoader::ComputeFileSha256(ForeignFile,ForeignBefore);
	Component->SetStaticMesh(Foreign); FProjectWorldRealizationResult RepairResult;
	if (!TestTrue(TEXT("Dirty Apply restores the owned structural mesh without borrowing a foreign duplicate."),
		Producer->Apply(Reloaded,Inventory,RepairResult,Error))) {AddError(Error); return false;}
	TestTrue(TEXT("A foreign mesh package remains byte-identical during owned output repair."),
		FProjectWorldCanonicalLoader::ComputeFileSha256(ForeignFile,ForeignAfter) && ForeignAfter==ForeignBefore);
	Inventory.Artifacts.Reset();
	TestTrue(TEXT("Repair leaves the unit bound to the actual owned package."),Producer->CaptureArtifacts(Reloaded,Inventory,Result,Error));
	TestEqual(TEXT("Repeated capture replaces rather than duplicates the complete artifact inventory."),Inventory.Artifacts.Num(),4);
	const auto ExpectedTags=Actors[1]->Tags;
	Actors[1]->Tags.RemoveAll([](FName N){return N.ToString().StartsWith(TEXT("ProjectWorld.ConstructedModel="));});
	Actors[1]->Tags.Add(FName(*(TEXT("ProjectWorld.ConstructedModel=")+FString::ChrN(64,'e')))); Actors[1]->MarkPackageDirty();
	if (!Fixture.SaveOwned(Error)) {AddError(Error); return false;}
	Actors[1]->Tags=ExpectedTags; Actors[1]->MarkPackageDirty();
	Refuse(TEXT("An unsaved live correction cannot authenticate stale saved actor bytes."));
	if (!Fixture.SaveOwned(Error)) {AddError(Error); return false;}
	const auto Saved=Fixture.PackageDigests();
	TArray<FString> Clean; const FProjectWorldLayerApplyContext NoOp{*Fixture.GetWorld(),Inputs,Clean,Materials};
	FProjectWorldRealizationResult NoOpResult;
	if (!TestTrue(TEXT("A clean native re-apply preserves the realized units."),Producer->Apply(NoOp,Inventory,NoOpResult,Error))) {AddError(Error); return false;}
	TestTrue(TEXT("Cold unchanged inputs leave every saved package byte-identical."),Saved.OrderIndependentCompareEqual(Fixture.PackageDigests()));
	return !HasAnyErrors();
}
#endif
