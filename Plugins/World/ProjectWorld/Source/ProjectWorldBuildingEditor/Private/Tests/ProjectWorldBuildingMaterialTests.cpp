// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldBuildingLayerProducer.h"
#include "ProjectWorldAuthoredOverlay.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldRealizationProfile.h"
#include "ProjectWorldRealizationService.h"
#include "ProjectWorldVerify.h"
#include "Materials/Material.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldBuildingMaterialReferencesTest,
    "Project.World.Realization.Buildings.MaterialReferences",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldBuildingMaterialReferencesTest::RunTest(const FString&)
{
    FProjectWorldVerifyWorld Fixture(TEXT("BuildingMaterial")+FGuid::NewGuid().ToString(EGuidFormats::Digits));
    FString Error,Code;
    if (!Fixture.IsReady(Error)) {AddError(Error); return false;}
    FProjectWorldRealizationProfile Profile;
    if (!ProjectWorldRealizationProfile::Load(FPaths::ProjectPluginsDir()/
        TEXT("World/ProjectWorldTestData/Data/Profiles/Realization/synthetic_territory_twin.realization.json"),
        Profile,Code,Error)) {AddError(Error); return false;}
    auto* Layer=Profile.Layers.FindByPredicate([](const auto& L){return L.GeneratorId==TEXT("project_building_massing");});
    if (!TestNotNull(TEXT("Existing Building producer profile"),Layer)) return false;
    Layer->ArtifactRoot=Fixture.GetRoot()+TEXT("Buildings/");
    FProjectWorldCanonicalBundle Bundle;
    Bundle.GridId=TEXT("building_material"); Bundle.CoordinateQuantizationMeters=.01; Bundle.HeightQuantizationMeters=.1;
    FProjectWorldCanonicalCell Cell;
    Cell.CellId=TEXT("building_material:x0:y0"); Cell.Bounds=FVector4d(0,0,100,100);
    Cell.Terrain.Bounds=Cell.Bounds; Cell.Terrain.SampleSpacing=FVector2D(100,100);
    Cell.Terrain.SamplesX=Cell.Terrain.SamplesY=2; Cell.Terrain.HeightsMeters={2,2,2,2};
    Cell.Terrain.ArtifactHash=FString::ChrN(64,TEXT('a'));
    FProjectWorldCanonicalFeature Feature;
    Feature.FeatureId=TEXT("building/material"); Feature.FeatureClass=TEXT("building");
    Feature.OwnerCellId=Cell.CellId; Feature.GeometryType=TEXT("Polygon"); Feature.HeightMeters=12;
    FProjectWorldCanonicalPolygon Polygon;
    Polygon.Outer={FVector2D(10,10),FVector2D(30,10),FVector2D(30,30),FVector2D(10,30),FVector2D(10,10)};
    Feature.GeometryPolygons.Add(Polygon);
    auto& Volume=Feature.BuildingVolumes.AddDefaulted_GetRef();
    Volume.VolumeId=Feature.FeatureId+TEXT("/volume"); Volume.SourceFeatureId=Feature.FeatureId;
    Volume.GeometryType=TEXT("Polygon"); Volume.GeometryPolygons.Add(Polygon); Volume.HeightMeters=12;
    Cell.OwnedFeatureIds.Add(Feature.FeatureId); Bundle.Cells.Add(Cell); Bundle.Features.Add(Feature.FeatureId,Feature);
    const auto Material=[&](const TCHAR* Name)
    {
        auto* Package=CreatePackage(*(Fixture.GetRoot()+Name));
        auto* Asset=NewObject<UMaterial>(Package,Name,RF_Public|RF_Standalone);
        Asset->SetShadingModel(MSM_DefaultLit); Asset->SetUsageByFlag(MATUSAGE_Nanite,true);
        FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone;
        if (!UPackage::SavePackage(Package,Asset,*FPackageName::LongPackageNameToFilename(
            Package->GetName(),FPackageName::GetAssetPackageExtension()),Args)) return static_cast<UMaterial*>(nullptr);
        return Asset;
    };
    auto* Prior=Material(TEXT("M_Prior")); auto* Next=Material(TEXT("M_Next"));
    if (!TestNotNull(TEXT("Saved prior material"),Prior) || !TestNotNull(TEXT("Saved replacement material"),Next)) return false;
    const FProjectWorldAuthoredOverlaySet Overlays;
    const FProjectWorldLayerInputs Inputs{Bundle,Profile,*Layer,Overlays};
    const TArray<FString> Dirty{TEXT("*")};
    const TMap<FName,UMaterialInterface*> PriorMaterials{{TEXT("building"),Prior}};
    const FProjectWorldLayerApplyContext Initial{*Fixture.GetWorld(),Inputs,Dirty,PriorMaterials};
    auto& Producer=FProjectWorldBuildingLayerProducer::Get();
    FProjectWorldLayerInventory Inventory;
    Inventory.LayerId=Layer->LayerId; Inventory.GeneratorId=Layer->GeneratorId; Inventory.GeneratorVersion=Layer->GeneratorVersion;
    Inventory.ArtifactRoot=Layer->ArtifactRoot; Inventory.FinalDirtyUnits=Dirty;
    FProjectWorldRealizationResult Result; Result.LayerInventories.Add(Inventory);
    if (!Producer.Apply(Initial,Inventory,Result,Error) || !Fixture.SaveOwned(Error) ||
        !Producer.CaptureArtifacts(Initial,Inventory,Result,Error)) {AddError(Error); return false;}
    FProjectWorldLayerInventory Fresh;
    FProjectWorldRealizationResult ControlResult;
    if (!Producer.UpdateMaterialReferences(Initial,Inventory.Artifacts,Fresh,ControlResult,true,Error)) {AddError(Error); return false;}
    const auto BeforeCold=Fixture.PackageDigests();
    if (!Fixture.Reload(Error)) {AddError(Error); return false;}
    TestTrue(TEXT("Untouched cold baseline retains every saved package"),BeforeCold.OrderIndependentCompareEqual(Fixture.PackageDigests()));
    Prior=LoadObject<UMaterial>(nullptr,*(Fixture.GetRoot()+TEXT("M_Prior.M_Prior")));
    Next=LoadObject<UMaterial>(nullptr,*(Fixture.GetRoot()+TEXT("M_Next.M_Next")));
    if (!TestNotNull(TEXT("Cold prior material"),Prior) || !TestNotNull(TEXT("Cold replacement material"),Next)) return false;
    const TMap<FName,UMaterialInterface*> ColdMaterials{{TEXT("building"),Prior}};
    const FProjectWorldLayerApplyContext Cold{*Fixture.GetWorld(),Inputs,Dirty,ColdMaterials};
    FProjectWorldLayerInventory Control;
    if (!Producer.UpdateMaterialReferences(Cold,Inventory.Artifacts,Control,ControlResult,true,Error)) {AddError(Error); return false;}
    if (!TestEqual(TEXT("Fresh fixture exposes one geometry payload"),Fresh.MaterialGeometryInputs.Num(),1) ||
        !TestEqual(TEXT("Cold fixture exposes one geometry payload"),Control.MaterialGeometryInputs.Num(),1)) return false;
    AddInfo(TEXT("Untouched freshly-built -> cold geometry identities: ")+Fresh.MaterialGeometryInputs[0].Hash+TEXT(" -> ")+Control.MaterialGeometryInputs[0].Hash);
    TestEqual(TEXT("Untouched cold control saves no mesh"),Control.Metrics.FindRef(TEXT("material_mesh_writes")),int64(0));
    TestEqual(TEXT("Untouched cold control saves no actor"),Control.Metrics.FindRef(TEXT("material_actor_writes")),int64(0));
    const auto Accepted=Inventory.Artifacts;
    if (!TestEqual(TEXT("Native Building fixture owns one mesh/actor pair"),Accepted.Num(),2)) return false;
    const TMap<FName,UMaterialInterface*> NextMaterials{{TEXT("building"),Next}};
    const FProjectWorldLayerApplyContext Context{*Fixture.GetWorld(),Inputs,Dirty,NextMaterials};
    auto Bad=Accepted; Bad[0].Digest=FString::ChrN(64,TEXT('0'));
    const auto Before=Fixture.PackageDigests();
    FProjectWorldLayerInventory Updated; FProjectWorldRealizationResult UpdateResult;
    TestFalse(TEXT("Drifted Building membership refuses without writes"),Producer.UpdateMaterialReferences(Context,Bad,Updated,UpdateResult,true,Error));
    TestTrue(TEXT("Refusal preserves every fixture package"),Before.OrderIndependentCompareEqual(Fixture.PackageDigests()));
    Error.Reset();
    if (!Producer.UpdateMaterialReferences(Context,Accepted,Updated,UpdateResult,false,Error)) {AddError(Error); return false;}
    TestTrue(TEXT("Validation preserves every fixture package"),Before.OrderIndependentCompareEqual(Fixture.PackageDigests()));
    if (!Producer.UpdateMaterialReferences(Context,Accepted,Updated,UpdateResult,true,Error)) {AddError(Error); return false;}
    TestEqual(TEXT("Existing mesh material slot is saved"),Updated.Metrics.FindRef(TEXT("material_mesh_writes")),int64(1));
    TestEqual(TEXT("One Building geometry payload is checked"),Updated.MaterialGeometryInputs.Num(),1);
    TestEqual(TEXT("Material replacement invokes no native mesh build"),Updated.Metrics.FindRef(TEXT("material_mesh_build_events")),int64(0));
    TestEqual(TEXT("Material replacement creates no actors"),UpdateResult.CreatedActorCount,0);
    TestFalse(TEXT("Material replacement requests no geometry builder"),Updated.PostApplyBuilder.IsSet());
    if (!TestEqual(TEXT("Material replacement retains accepted membership"),Updated.Artifacts.Num(),Accepted.Num())) return false;
    for (int32 I=0; I<Accepted.Num(); ++I)
        TestEqual(TEXT("Material replacement retains geometry semantics"),Updated.Artifacts[I].SemanticHash,Accepted[I].SemanticHash);
    if (!Fixture.Reload(Error)) {AddError(Error); return false;}
    Next=LoadObject<UMaterial>(nullptr,*(Fixture.GetRoot()+TEXT("M_Next.M_Next")));
    if (!TestNotNull(TEXT("Saved replacement material reloads"),Next)) return false;
    const TMap<FName,UMaterialInterface*> ReloadedMaterials{{TEXT("building"),Next}};
    const FProjectWorldLayerApplyContext Reloaded{*Fixture.GetWorld(),Inputs,Dirty,ReloadedMaterials};
    FProjectWorldLayerInventory NoOp;
    if (!Producer.UpdateMaterialReferences(Reloaded,Updated.Artifacts,NoOp,UpdateResult,true,Error)) {AddError(Error); return false;}
    TestEqual(TEXT("Cold replay saves no mesh"),NoOp.Metrics.FindRef(TEXT("material_mesh_writes")),int64(0));
    TestEqual(TEXT("Cold replay saves no actor"),NoOp.Metrics.FindRef(TEXT("material_actor_writes")),int64(0));
    if (!TestEqual(TEXT("Reload retains geometry evidence count"),NoOp.MaterialGeometryInputs.Num(),Updated.MaterialGeometryInputs.Num())) return false;
    if (!TestEqual(TEXT("Cold replay retains accepted membership"),NoOp.Artifacts.Num(),Updated.Artifacts.Num())) return false;
    for (int32 I=0; I<Updated.MaterialGeometryInputs.Num(); ++I)
        TestEqual(TEXT("Saved source/render/collision identity is unchanged"),NoOp.MaterialGeometryInputs[I].Hash,Updated.MaterialGeometryInputs[I].Hash);
    for (int32 I=0; I<Updated.Artifacts.Num(); ++I)
        TestEqual(TEXT("Cold no-op preserves every accepted artifact digest"),NoOp.Artifacts[I].Digest,Updated.Artifacts[I].Digest);
    return true;
}
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FProjectWorldBuildingMaterialReferencesTest,
    "Project.World.Realization.Buildings.MaterialReferences","[Slow][Integration][World]")
#endif
