// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldWaterRealization.h"
#include "ProjectWorldWaterLayerProducer.h"
#include "ProjectWorldAuthoredOverlay.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldRealizationProfile.h"
#include "ProjectWorldRealizationService.h"
#include "ProjectWorldVerify.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Materials/Material.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "PackageTools.h"
#include "UObject/SavePackage.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldPreparedWaterPresentationTest,
    "Project.World.Water.PreparedPresentation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FProjectWorldPreparedWaterPresentationTest,
    "Project.World.Water.PreparedPresentation","[Slow][Integration][World]")
bool FProjectWorldPreparedWaterPresentationTest::RunTest(const FString&)
{
    const FString Id=FGuid::NewGuid().ToString(EGuidFormats::Digits);
    FProjectWorldRealizationProfile Profile; FString Code,Error;
    FProjectWorldVerifyWorld Fixture(TEXT("WaterPresentation")+Id);
    if (!Fixture.IsReady(Error)) {AddError(Error); return false;}
    const FString Prefix=Fixture.GetRoot();
    if (!ProjectWorldRealizationProfile::Load(FPaths::ProjectPluginsDir()/TEXT("World/ProjectWorldTestData/Data/Profiles/Realization/synthetic_territory_twin.realization.json"),Profile,Code,Error)) {AddError(Error); return false;}
    auto* Layer=Profile.Layers.FindByPredicate([](const auto& L){return L.GeneratorId==TEXT("project_water_mesh");});
    if (!TestNotNull(TEXT("Existing leaf profile"),Layer)) return false;
    Layer->ArtifactRoot=Prefix+TEXT("Water/");
    FProjectWorldCanonicalBundle Bundle;
    Bundle.GridId=TEXT("water_presentation"); Bundle.InputsHash=TEXT("fixture");
    Bundle.CoordinateQuantizationMeters=.01; Bundle.HeightQuantizationMeters=.1; Bundle.HeightOriginMeters=10;
    FProjectWorldCanonicalCell Cell; Cell.CellId=TEXT("water_presentation:x0:y0"); Cell.Bounds=FVector4d(0,0,63,63);
    Cell.OwnedFeatureIds={TEXT("water/fixture/lake")}; Bundle.Cells.Add(Cell);
    FProjectWorldCanonicalFeature Lake; Lake.FeatureId=Cell.OwnedFeatureIds[0]; Lake.FeatureClass=TEXT("water"); Lake.OwnerCellId=Cell.CellId;
    Lake.WaterSurface.bValid=true; Lake.WaterSurface.SurfaceGroupId=Lake.FeatureId;
    Lake.WaterSurface.Geometry=TEXT("polygon"); Lake.WaterSurface.Behavior=TEXT("standing");
    Lake.WaterSurface.FunctionId=TEXT("standing_polygon_quantile"); Lake.WaterSurface.FunctionVersion=1; Lake.WaterSurface.LevelMeters=12;
    FProjectWorldCanonicalPolygon Polygon; Polygon.Outer={FVector2D(5,5),FVector2D(58,5),FVector2D(58,58),FVector2D(5,58),FVector2D(5,5)};
    Lake.GeometryPolygons.Add(Polygon); Bundle.Features.Add(Lake.FeatureId,Lake);
    auto* Package=CreatePackage(*(Prefix+TEXT("M_Prepared")));
    auto* Material=NewObject<UMaterial>(Package,TEXT("M_Prepared"),RF_Public|RF_Standalone);
    Material->SetShadingModel(MSM_SingleLayerWater); Material->BlendMode=BLEND_Opaque;
    FSavePackageArgs Save; Save.TopLevelFlags=RF_Public|RF_Standalone;
    if (!TestTrue(TEXT("Save isolated prepared material"),UPackage::SavePackage(Package,Material,*FPackageName::LongPackageNameToFilename(Package->GetName(),FPackageName::GetAssetPackageExtension()),Save))) return false;
    FProjectWorldRealizationResult Result; FProjectWorldLayerInventory Inventory;
    Inventory.LayerId=Layer->LayerId; Inventory.GeneratorId=Layer->GeneratorId; Inventory.GeneratorVersion=Layer->GeneratorVersion;
    Inventory.ArtifactRoot=Layer->ArtifactRoot; Inventory.FinalDirtyUnits={TEXT("*")}; Result.LayerInventories.Add(Inventory);
    UWorld* World=Fixture.GetWorld(); Material->GetOutermost()->SetDirtyFlag(true);
    TestFalse(TEXT("Dirty prepared resource refuses before geometry mutation"),ProjectWorldWaterRealization::Apply(World,Bundle,Profile,Result,Error,Material));
    TestEqual(TEXT("Refusal writes no water actors"),Result.CreatedActorCount,0);
    Material->GetOutermost()->SetDirtyFlag(false); Error.Reset();
    if (!TestTrue(TEXT("Leaf uses prepared SingleLayerWater without compiling another graph"),ProjectWorldWaterRealization::Apply(World,Bundle,Profile,Result,Error,Material))) {AddError(Error); return false;}
    TestEqual(TEXT("One selected cell realized"),Result.CreatedActorCount,1);
    TestEqual(TEXT("Water owns persistence of its external actor"),Result.SelfSavedActorMutationCount,1);
    if (!Fixture.SaveOwned(Error) || !Fixture.Reload(Error)) {AddError(Error); return false;}
    World=Fixture.GetWorld();
    Material=LoadObject<UMaterial>(nullptr,*(Prefix+TEXT("M_Prepared.M_Prepared")));
    if (!TestNotNull(TEXT("Prepared presentation reloads from its saved package"),Material)) return false;
    bool Found=false;
    for (TActorIterator<AStaticMeshActor> I(World); I; ++I)
    {
        FString CellId,Semantic;
        if (!ProjectWorldWaterRealization::ReadActorIdentity(*I,CellId,Semantic)) continue;
        Found=true;
        TestTrue(TEXT("Effective component consumes prepared presentation"),I->GetStaticMeshComponent()->GetMaterial(0)==Material);
        TestTrue(TEXT("Saved mesh consumes the same prepared presentation"),I->GetStaticMeshComponent()->GetStaticMesh()->GetStaticMaterials()[0].MaterialInterface==Material);
    }
    TestTrue(TEXT("Realized native water actor exists"),Found);
    const FProjectWorldAuthoredOverlaySet Overlays;
    const FProjectWorldLayerInputs Inputs{Bundle,Profile,*Layer,Overlays};
    const TMap<FName,UMaterialInterface*> Materials{{TEXT("water"),Material}};
    const FProjectWorldLayerApplyContext Context{*World,Inputs,Inventory.FinalDirtyUnits,Materials};
    auto& Producer=FProjectWorldWaterLayerProducer::Get();
    FProjectWorldLayerInventory Captured=Inventory;
    if (!TestTrue(TEXT("Saved inventory accepts the exact prepared water reference"),Producer.CaptureArtifacts(Context,Captured,Result,Error))) {AddError(Error); return false;}
    for (TActorIterator<AStaticMeshActor> I(World); I; ++I)
    {
        FString CellId,Semantic; if (!ProjectWorldWaterRealization::ReadActorIdentity(*I,CellId,Semantic)) continue;
        I->GetStaticMeshComponent()->SetMaterial(0,UMaterial::GetDefaultMaterial(MD_Surface));
        TestFalse(TEXT("Effective override mismatch refuses saved inspection"),Producer.CaptureArtifacts(Context,Captured,Result,Error));
        I->GetStaticMeshComponent()->SetMaterial(0,Material);
    }
    TestFalse(TEXT("Prepared path creates no generator-local diagnostic shader"),FPaths::FileExists(FPackageName::LongPackageNameToFilename(Layer->ArtifactRoot+TEXT("M_ProjectWorldWater"),FPackageName::GetAssetPackageExtension())));
    auto* AlternatePackage=CreatePackage(*(Prefix+TEXT("M_Alternate")));
    auto* Alternate=NewObject<UMaterial>(AlternatePackage,TEXT("M_Alternate"),RF_Public|RF_Standalone);
    Alternate->SetShadingModel(MSM_SingleLayerWater); Alternate->BlendMode=BLEND_Opaque;
    if (!UPackage::SavePackage(AlternatePackage,Alternate,*FPackageName::LongPackageNameToFilename(AlternatePackage->GetName(),FPackageName::GetAssetPackageExtension()),Save)) return false;
    const TMap<FName,UMaterialInterface*> NewMaterials{{TEXT("water"),Alternate}};
    const FProjectWorldLayerApplyContext UpdateContext{*World,Inputs,Inventory.FinalDirtyUnits,NewMaterials};
    const auto Accepted=Captured.Artifacts;
    auto Bad=Accepted; Bad[0].Digest=FString::ChrN(64,TEXT('0'));
    FProjectWorldLayerInventory Updated=Inventory; FProjectWorldRealizationResult UpdateResult;
    TestFalse(TEXT("Drifted prior membership refuses material mutation"),Producer.UpdateMaterialReferences(UpdateContext,Bad,Updated,UpdateResult,true,Error));
    TestEqual(TEXT("Refusal writes no mesh material"),Updated.Metrics.FindRef(TEXT("material_mesh_writes")),int64(0));
    Bad=Accepted; Bad.RemoveAt(0);
    TestFalse(TEXT("Missing prior membership cannot be fabricated from current actors"),Producer.UpdateMaterialReferences(UpdateContext,Bad,Updated,UpdateResult,true,Error));
    if (!TestTrue(TEXT("Complete material-reference plan validates without mutation"),Producer.UpdateMaterialReferences(UpdateContext,Accepted,Updated,UpdateResult,false,Error))) {AddError(Error); return false;}
    if (!TestTrue(TEXT("Owner updates only saved/effective water material references"),Producer.UpdateMaterialReferences(UpdateContext,Accepted,Updated,UpdateResult,true,Error))) {AddError(Error); return false;}
    TestEqual(TEXT("One mesh reference persisted"),Updated.Metrics.FindRef(TEXT("material_mesh_writes")),int64(1));
    TestEqual(TEXT("No native mesh build event during reference update"),Updated.Metrics.FindRef(TEXT("material_mesh_build_events")),int64(0));
    TestEqual(TEXT("Created zero actors in material operation"),UpdateResult.CreatedActorCount,0);
    TestEqual(TEXT("Existing semantic identities retained"),Updated.Artifacts[0].SemanticHash,Accepted[0].SemanticHash);
    if (!Fixture.Reload(Error)) {AddError(Error); return false;}
    World=Fixture.GetWorld();
    Alternate=LoadObject<UMaterial>(nullptr,*(Prefix+TEXT("M_Alternate.M_Alternate")));
    const TMap<FName,UMaterialInterface*> ReloadedMaterials{{TEXT("water"),Alternate}};
    const FProjectWorldLayerApplyContext ReloadedContext{*World,Inputs,Inventory.FinalDirtyUnits,ReloadedMaterials};
    FProjectWorldLayerInventory NoOp=Inventory;
    if (!TestTrue(TEXT("Saved material reference survives native fixture reload"),Producer.UpdateMaterialReferences(ReloadedContext,Updated.Artifacts,NoOp,UpdateResult,true,Error))) {AddError(Error); return false;}
    TestEqual(TEXT("Repeated material application writes zero meshes"),NoOp.Metrics.FindRef(TEXT("material_mesh_writes")),int64(0));
    TestEqual(TEXT("Repeated material application writes zero actors"),NoOp.Metrics.FindRef(TEXT("material_actor_writes")),int64(0));
    TestEqual(TEXT("No post-apply geometry builder requested"),NoOp.PostApplyBuilder.IsSet(),false);
    TestEqual(TEXT("Reload retains source/render/collision geometry evidence count"),NoOp.MaterialGeometryInputs.Num(),Updated.MaterialGeometryInputs.Num());
    for (int32 I=0; I<Updated.MaterialGeometryInputs.Num(); ++I)
        TestEqual(TEXT("Exact geometry payload survives saved reference and native reload"),NoOp.MaterialGeometryInputs[I].Hash,Updated.MaterialGeometryInputs[I].Hash);
    for (int32 I=0; I<Updated.Artifacts.Num(); ++I)
        TestEqual(TEXT("No-op preserves accepted package digest"),NoOp.Artifacts[I].Digest,Updated.Artifacts[I].Digest);
    return true;
}
#endif
