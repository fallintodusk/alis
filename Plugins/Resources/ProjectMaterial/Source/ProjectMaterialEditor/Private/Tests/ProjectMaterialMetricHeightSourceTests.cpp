// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectMaterialMetricHeightSurface.h"
#include "ProjectMaterialSurfaceGenerationService.h"
#include "ProjectMaterialSurfaceRecipe.h"
#include "ProjectMaterialSurfaceBuilder.h"
#include "ProjectMaterialPatternAuthority.h"
#include "ProjectMaterialFixtureDomain.h"
#include "ProjectMaterialSurfacePreview.h"
#include "ProjectMaterialPreviewAuthority.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2DArray.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProjectTextureGenerationService.h"
#include "ProjectTextureFixtureDomain.h"
#include "AssetReferencingPolicySettings.h"
#include "MaterialEditingLibrary.h"
#include "MeshPartitionMaterialExpressionUtils.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionSubstrate.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "UObject/Package.h"
#include "PackageTools.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectMaterialMetricHeightSourceTest,
    "Project.Material.HeightMasks.AuthenticatedMetricSample",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FProjectMaterialMetricHeightSourceTest,
    "Project.Material.HeightMasks.AuthenticatedMetricSample","[Slow][Integration][ProjectMaterial]")

bool FProjectMaterialMetricHeightSourceTest::RunTest(const FString&)
{
    const FString Id=FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FString TR=FPaths::ProjectDir()/TEXT("tmp/texture/generation/metric_height_")+Id;
    const FString MR=FPaths::ProjectDir()/TEXT("tmp/material/generation/metric_height_")+Id;
    auto* Settings=GetMutableDefault<UAssetReferencingPolicySettings>(); const auto Policy=Settings->AdditionalDomains;
    const bool HadTexture=Policy.ContainsByPredicate([](const auto& D){return D.DomainName==TEXT("ProjectTextureFixture");});
    const bool HadMaterial=Policy.ContainsByPredicate([](const auto& D){return D.DomainName==TEXT("ProjectMaterialFixture");});
    TArray<FString> Packages;
    ON_SCOPE_EXIT
    {
        TArray<UPackage*> Loaded;
        for (const FString& Name : Packages) if (auto* Package=FindPackage(nullptr,*Name)) {Package->SetDirtyFlag(false); Loaded.AddUnique(Package);}
        if (!Loaded.IsEmpty()) UPackageTools::UnloadPackages(Loaded);
        FPackageName::UnRegisterMountPoint(TEXT("/ProjectMaterialTest/"),MR/TEXT("content/"));
        FPackageName::UnRegisterMountPoint(TEXT("/ProjectTextureTest/"),TR/TEXT("content/"));
        IFileManager::Get().DeleteDirectory(*MR,false,true); IFileManager::Get().DeleteDirectory(*TR,false,true);
        const uint64 AfterFrame=GFrameCounter+2;
        ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this,Settings,Policy,HadTexture,HadMaterial,AfterFrame]()
        {
            if (GFrameCounter<AfterFrame) return false;
            if (!HadMaterial) ProjectMaterialFixtureDomain::Shutdown(); if (!HadTexture) ProjectTextureFixtureDomain::Shutdown();
            bool Same=Policy.Num()==Settings->AdditionalDomains.Num();
            for (int32 I=0; Same && I<Policy.Num(); ++I) Same=FARPDomainDefinitionByContentRoot::StaticStruct()->CompareScriptStruct(&Policy[I],&Settings->AdditionalDomains[I],0);
            TestTrue(TEXT("Exact fixture reference policy restoration"),Same); return true;
        }));
    };
    auto Copy=[](const FString& From,const FString& To,bool Rewrite)
    {
        FString Json; if (!FFileHelper::LoadFileToString(Json,*From)) return false;
        if (Rewrite) Json.ReplaceInline(TEXT("/ProjectMaterial/Surfaces/"),TEXT("/ProjectMaterialTest/Surfaces/"));
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(To),true); return FFileHelper::SaveStringToFile(Json,*To);
    };
    FProjectTextureGenerationRequest T;
    T.RecipeRoot=TR/TEXT("recipes"); T.OutputPackageRoot=TEXT("/ProjectTextureTest/Patterns");
    T.OutputContentRoot=TR/TEXT("content/Patterns"); T.ManifestRoot=TR/TEXT("manifests");
    if (!Copy(FPaths::ProjectPluginsDir()/TEXT("Resources/ProjectTexture/Data/TestFixtures/GradedParticles/project_graded_fixture.pattern.json"),T.RecipeRoot/TEXT("GradedParticles/project_graded_fixture.pattern.json"),false)) return false;
    FPackageName::RegisterMountPoint(TEXT("/ProjectTextureTest/"),TR/TEXT("content/"));
    FProjectTextureGenerationResult Generated;
    if (!TestTrue(TEXT("Existing texture owner authenticates graded source"),FProjectTextureGenerationService::RegenerateTestMount(T,Generated))) {AddError(Generated.Error); return false;}
    for (const FString& P : Generated.OutputObjectPaths) Packages.AddUnique(FPackageName::ObjectPathToPackageName(P));
    FProjectMaterialSurfaceGenerationRequest M; FString Error;
    if (!FProjectMaterialSurfaceGenerationService::MakeTestRequest(MR,FString(),M,Error)) {AddError(Error); return false;}
    M.PatternPackageRoot=TEXT("/ProjectTextureTest"); M.PatternContentRoot=TR/TEXT("content");
    M.PatternManifestPath=T.ManifestRoot/TEXT("accepted.pattern-manifest.json");
    FPackageName::RegisterMountPoint(TEXT("/ProjectMaterialTest/"),MR/TEXT("content/"));
    for (const TCHAR* Name : {TEXT("M_ProjectMetricHeightFixture"),TEXT("MI_ProjectMetricHeightFixture")})
        if (!Copy(FPaths::ProjectPluginsDir()/TEXT("Resources/ProjectMaterial/Data/TestFixtures/MetricHeight")/(FString(Name)+TEXT(".surface.json")),M.RecipeRoot/TEXT("MetricHeight")/(FString(Name)+TEXT(".surface.json")),true)) return false;
    FProjectMaterialSurfaceGenerationResult Result;
    auto* SourceAsset=LoadObject<UObject>(nullptr,*Generated.OutputObjectPaths[0]);
    SourceAsset->GetOutermost()->SetDirtyFlag(true);
    TestFalse(TEXT("Dirty source refuses the complete material transaction"),FProjectMaterialSurfaceGenerationService::RegenerateTestMount(M,Result));
    TestEqual(TEXT("Refusal writes no earlier parent"),Result.Generated,0);
    TestFalse(TEXT("Refusal creates no material output tree"),FPaths::DirectoryExists(M.OutputContentRoot));
    SourceAsset->GetOutermost()->SetDirtyFlag(false);
    if (!TestTrue(TEXT("Metric parent and instance save/reload through their owner"),FProjectMaterialSurfaceGenerationService::RegenerateTestMount(M,Result))) {AddError(Result.Error); return false;}
    TestEqual(TEXT("Two generated material artifacts"),Result.Generated,2);
    for (const FString& P : Result.OutputObjectPaths) Packages.AddUnique(FPackageName::ObjectPathToPackageName(P));
    FProjectMaterialSurfaceGenerationResult Repeat;
    TestTrue(TEXT("Unchanged material transaction validates and skips"),FProjectMaterialSurfaceGenerationService::RegenerateTestMount(M,Repeat));
    TestEqual(TEXT("No repeat material writes"),Repeat.Generated,0);
    const FString File=M.RecipeRoot/TEXT("MetricHeight/M_ProjectMetricHeightFixture.surface.json"); FString Json;
    FProjectMaterialSurfaceRecipe R; FProjectMaterialSurfaceDependency P;
    if (!FFileHelper::LoadFileToString(Json,*File) || !FProjectMaterialSurfaceRecipeContract::Parse(Json,File,M.RecipeRoot,R,Error) ||
        !FProjectMaterialPatternAuthority::Resolve(R.PatternId,M.PatternManifestPath,M.PatternPackageRoot,M.PatternContentRoot,P,Error)) {AddError(Error); return false;}
    auto* Parent=LoadObject<UMaterial>(nullptr,TEXT("/ProjectMaterialTest/Surfaces/MetricHeight/M_ProjectMetricHeightFixture.M_ProjectMetricHeightFixture"));
    if (!TestNotNull(TEXT("Actual compiled metric parent"),Parent)) return false;
    TestTrue(TEXT("Saved terrain parent declares native Nanite usage"),Parent->GetUsageByFlag(MATUSAGE_Nanite));
    Parent->SetUsageByFlag(MATUSAGE_Nanite,false);
    TestFalse(TEXT("Missing cooked vertex-factory usage refuses authentication"),ProjectMaterialSurfaceBuilder::Verify(R,P,{},Parent,Error));
    Parent->SetUsageByFlag(MATUSAGE_Nanite,true);
    TestTrue(TEXT("Complete connected native graph authenticates"),ProjectMaterialMetricHeightSurface::Verify(R,P,Parent,Error));
    auto* Slab=CastChecked<UMaterialExpressionSubstrateSlabBSDF>(Parent->GetEditorOnlyData()->FrontMaterial.Expression);
    const auto Normal=Slab->Normal; Slab->Normal=Slab->Roughness;
    TestFalse(TEXT("Valid expressions cannot substitute an incorrect normal route"),ProjectMaterialMetricHeightSurface::Verify(R,P,Parent,Error)); Slab->Normal=Normal;
    Parent->bTangentSpaceNormal=true;
    TestFalse(TEXT("Wrong normal frame refuses"),ProjectMaterialMetricHeightSurface::Verify(R,P,Parent,Error)); Parent->bTangentSpaceNormal=false;
    UMaterialExpressionTextureSampleParameter2D* NormalSample=nullptr; UMaterialExpressionTextureSampleParameter2D* MasksSample=nullptr;
    UMaterialExpressionScalarParameter* Scale=nullptr;
    for (auto* E : UMaterialEditingLibrary::GetMaterialExpressions(Parent))
    {
        if (auto* S=Cast<UMaterialExpressionTextureSampleParameter2D>(E))
        {if (S->ParameterName==TEXT("SurfaceNormal")) NormalSample=S; if (S->ParameterName==TEXT("SurfaceMasks")) MasksSample=S;}
        if (auto* S=Cast<UMaterialExpressionScalarParameter>(E); S && S->ParameterName==TEXT("SecondaryScaleRatio")) Scale=S;
    }
    if (!TestNotNull(TEXT("Normal sample"),NormalSample) || !TestNotNull(TEXT("Mask sample"),MasksSample) || !TestNotNull(TEXT("Metric scale"),Scale)) return false;
    Swap(NormalSample->Texture,MasksSample->Texture);
    TestFalse(TEXT("Swapped map roles refuse"),ProjectMaterialMetricHeightSurface::Verify(R,P,Parent,Error)); Swap(NormalSample->Texture,MasksSample->Texture);
    const auto Sampler=NormalSample->SamplerType; NormalSample->SamplerType=SAMPLERTYPE_Normal;
    TestFalse(TEXT("Normal sampler cannot reinterpret linear encoded data"),ProjectMaterialMetricHeightSurface::Verify(R,P,Parent,Error)); NormalSample->SamplerType=Sampler;
    const float Before=Scale->DefaultValue; Scale->DefaultValue=2;
    TestFalse(TEXT("Altered consumed appearance refuses"),ProjectMaterialMetricHeightSurface::Verify(R,P,Parent,Error)); Scale->DefaultValue=Before;
    TestTrue(TEXT("Exact graph restoration authenticates"),ProjectMaterialMetricHeightSurface::Verify(R,P,Parent,Error));
    Parent->GetOutermost()->SetDirtyFlag(false);
    for (const TCHAR* Name : {TEXT("M_ProjectSemanticHeightFixture"),TEXT("MI_ProjectSemanticHeightFixture")})
        if (!Copy(FPaths::ProjectPluginsDir()/TEXT("Resources/ProjectMaterial/Data/TestFixtures/SemanticMetricHeight")/(FString(Name)+TEXT(".surface.json")),M.RecipeRoot/TEXT("SemanticMetricHeight")/(FString(Name)+TEXT(".surface.json")),true)) return false;
    const FString SemanticInstanceFile=M.RecipeRoot/TEXT("SemanticMetricHeight/MI_ProjectSemanticHeightFixture.surface.json");
    FString InstanceJson;
    if (!FFileHelper::LoadFileToString(InstanceJson,*SemanticInstanceFile)) return false;
    TestEqual(TEXT("One instance roughness control differs from its parent"),InstanceJson.ReplaceInline(TEXT("\"BaseRoughness\": 0.93"),TEXT("\"BaseRoughness\": 0.91")),1);
    if (!FFileHelper::SaveStringToFile(InstanceJson,*SemanticInstanceFile)) return false;
    FProjectMaterialSurfaceGenerationResult MissingLayout;
    TestFalse(TEXT("Semantic terrain refuses missing layout before mutation"),FProjectMaterialSurfaceGenerationService::RegenerateTestMount(M,MissingLayout));
    TestEqual(TEXT("Missing layout generates zero assets"),MissingLayout.Generated,0);
    M.TerrainLayoutReceiptPath=FPaths::ProjectPluginsDir()/TEXT("World/ProjectWorldMeshTerrain/Data/TestFixtures/mesh-terrain-layout-receipt.json");
    FProjectMaterialSurfaceGenerationResult SemanticGenerated;
    if (!TestTrue(TEXT("Same soil source compiles semantic terrain through its owner"),FProjectMaterialSurfaceGenerationService::RegenerateTestMount(M,SemanticGenerated)))
    {AddError(SemanticGenerated.Error); return false;}
    for (const FString& Name : SemanticGenerated.OutputObjectPaths) Packages.AddUnique(FPackageName::ObjectPathToPackageName(Name));
    TestEqual(TEXT("Only two semantic assets are new"),SemanticGenerated.Generated,2);
    TestEqual(TEXT("Original BrownEarth resources remain current"),SemanticGenerated.Skipped,2);
    const FString SemanticFile=M.RecipeRoot/TEXT("SemanticMetricHeight/M_ProjectSemanticHeightFixture.surface.json");
    FProjectMaterialSurfaceRecipe SemanticRecipe;
    if (!FFileHelper::LoadFileToString(Json,*SemanticFile) || !FProjectMaterialSurfaceRecipeContract::Parse(Json,SemanticFile,M.RecipeRoot,SemanticRecipe,Error)) {AddError(Error); return false;}
    auto* SemanticParent=LoadObject<UMaterial>(nullptr,TEXT("/ProjectMaterialTest/Surfaces/SemanticMetricHeight/M_ProjectSemanticHeightFixture.M_ProjectSemanticHeightFixture"));
    const TMap<FName,int32> Channels{{TEXT("ground"),0},{TEXT("hydro_transition"),1}};
    if (!TestTrue(TEXT("Saved semantic graph authenticates exact channels"),ProjectMaterialSurfaceBuilder::Verify(SemanticRecipe,P,Channels,SemanticParent,Error))) {AddError(Error); return false;}
    UE::MeshPartition::UMaterialExpressionMeshPartitionChannelSampleIndex* Hydro=nullptr;
    for (auto* E : UMaterialEditingLibrary::GetMaterialExpressions(SemanticParent))
        if (auto* Sample=Cast<UE::MeshPartition::UMaterialExpressionMeshPartitionChannelSampleIndex>(E); Sample && Sample->ConstChannelIndex==1) Hydro=Sample;
    if (!TestNotNull(TEXT("Native hydro receiver is connected"),Hydro)) return false;
    Hydro->ConstChannelIndex=0;
    TestFalse(TEXT("Ground duplicated as hydro refuses graph authentication"),ProjectMaterialSurfaceBuilder::Verify(SemanticRecipe,P,Channels,SemanticParent,Error));
    Hydro->ConstChannelIndex=1;
    TestTrue(TEXT("Exact native semantic restoration authenticates"),ProjectMaterialSurfaceBuilder::Verify(SemanticRecipe,P,Channels,SemanticParent,Error));
    FProjectMaterialSurfaceGenerationResult SemanticNoOp;
    TestTrue(TEXT("Repeated semantic generation skips unchanged outputs"),FProjectMaterialSurfaceGenerationService::RegenerateTestMount(M,SemanticNoOp));
    TestEqual(TEXT("Repeated semantic generation compiles no shaders"),SemanticNoOp.ShaderCompiles,0);
    FProjectMaterialSurfaceRecipe PreviewRecipe;
    UMaterialInstanceConstant* PreviewBaseline=nullptr;
    if (!TestTrue(TEXT("Semantic instance authenticates against its actual parent defaults"),ProjectMaterialPreviewAuthority::Load(
        SemanticInstanceFile,M.RecipeRoot,M.TerrainLayoutReceiptPath,PreviewRecipe,PreviewBaseline,Error,MR,TR)))
    {AddError(Error); return false;}
    auto* Component=NewObject<UStaticMeshComponent>(GetTransientPackage());
    Component->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
    auto* Original=UMaterialInstanceDynamic::Create(PreviewBaseline,GetTransientPackage());
    Original->SetScalarParameterValue(TEXT("NativePreviewControl"),3.25f);
    auto* ChannelTexture=NewObject<UTexture2DArray>(GetTransientPackage());
    Original->SetTextureParameterValue(TEXT("MeshPartitionChannelTexture"),ChannelTexture);
    Component->SetMaterial(0,Original);
    FProjectMaterialSurfacePreview Preview;
    TestTrue(TEXT("Semantic owner preview accepts distinct instance defaults"),Preview.ApplyFixtureRecipeFile(
        SemanticInstanceFile,M.RecipeRoot,MR,M.TerrainLayoutReceiptPath,Component,0,Error,TR));
    const FString CandidateRoot=MR/TEXT("preview");
    const FString Candidate=CandidateRoot/TEXT("SemanticMetricHeight/MI_ProjectSemanticHeightFixture.surface.json");
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Candidate),true);
    TestEqual(TEXT("Tune exactly one appearance control"),InstanceJson.ReplaceInline(TEXT("\"BaseRoughness\": 0.91"),TEXT("\"BaseRoughness\": 0.89")),1);
    if (!FFileHelper::SaveStringToFile(InstanceJson,*Candidate)) return false;
    TestTrue(TEXT("Semantic appearance tunes without changing the authenticated parent"),Preview.ApplyFixtureRecipeFile(
        Candidate,CandidateRoot,MR,M.TerrainLayoutReceiptPath,Component,0,Error,TR));
    float Value=0;
    TestTrue(TEXT("Tuned roughness is exposed"),Component->GetMaterial(0)->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("BaseRoughness")),Value));
    TestEqual(TEXT("Tuned roughness reaches the transient consumer"),Value,0.89f);
    TestTrue(TEXT("Native renderer override survives"),Component->GetMaterial(0)->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("NativePreviewControl")),Value));
    TestEqual(TEXT("Native renderer override is exact"),Value,3.25f);
    UTexture* CopiedChannel=nullptr;
    TestTrue(TEXT("Native semantic channel context remains available"),Component->GetMaterial(0)->GetTextureParameterValue(
        FHashedMaterialParameterInfo(TEXT("MeshPartitionChannelTexture")),CopiedChannel));
    TestEqual(TEXT("Appearance tuning preserves the exact native channel texture"),CopiedChannel,static_cast<UTexture*>(ChannelTexture));
    auto* LastGood=Component->GetMaterial(0);
    FFileHelper::SaveStringToFile(TEXT("{broken"),*Candidate);
    TestFalse(TEXT("Invalid semantic tuning refuses"),Preview.ApplyFixtureRecipeFile(Candidate,CandidateRoot,MR,M.TerrainLayoutReceiptPath,Component,0,Error,TR));
    TestEqual(TEXT("Refusal preserves the last good semantic consumer"),Component->GetMaterial(0),LastGood);
    TestTrue(TEXT("Semantic preview reverts"),Preview.Revert(Error));
    TestEqual(TEXT("Semantic preview restores the exact original renderer"),Component->GetMaterial(0),static_cast<UMaterialInterface*>(Original));
    FProjectMaterialSurfaceGenerationResult AfterPreview;
    TestTrue(TEXT("Preview preserves authenticated saved resources"),FProjectMaterialSurfaceGenerationService::Validate(M,AfterPreview));
    TestEqual(TEXT("Preview requires zero shader compilation"),AfterPreview.ShaderCompiles,0);
    return true;
}
#endif
