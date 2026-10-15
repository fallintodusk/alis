// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectMaterialAggregateSurface.h"
#include "ProjectMaterialPatternAuthority.h"
#include "ProjectMaterialSurfaceGenerationService.h"
#include "ProjectMaterialSurfaceRecipe.h"
#include "ProjectMaterialFixtureDomain.h"
#include "ProjectTextureGenerationService.h"
#include "ProjectTextureFixtureDomain.h"
#include "AssetReferencingPolicySettings.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialExpressionSign.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "PackageTools.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectMaterialAggregateProjectionTest,
    "Project.Material.Aggregate.TriplanarContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectMaterialAggregateProjectionTest::RunTest(const FString&)
{
    const FString Id=FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FString TextureRoot=FPaths::ProjectDir()/TEXT("tmp/texture/generation")/(TEXT("aggregate_projection_")+Id);
    const FString MaterialRoot=FPaths::ProjectDir()/TEXT("tmp/material/generation")/(TEXT("aggregate_projection_")+Id);
    auto* Settings=GetMutableDefault<UAssetReferencingPolicySettings>(); const auto Policy=Settings->AdditionalDomains;
    const bool HadTexture=Policy.ContainsByPredicate([](const auto& R){return R.DomainName==TEXT("ProjectTextureFixture");});
    const bool HadMaterial=Policy.ContainsByPredicate([](const auto& R){return R.DomainName==TEXT("ProjectMaterialFixture");});
    TArray<FString> Packages;
    ON_SCOPE_EXIT
    {
        TArray<UPackage*> Loaded;
        for (const FString& Name : Packages) if (auto* P=FindPackage(nullptr,*Name)) {P->SetDirtyFlag(false); Loaded.AddUnique(P);}
        if (!Loaded.IsEmpty()) UPackageTools::UnloadPackages(Loaded);
        FPackageName::UnRegisterMountPoint(TEXT("/ProjectMaterialTest/"),MaterialRoot/TEXT("content/"));
        FPackageName::UnRegisterMountPoint(TEXT("/ProjectTextureTest/"),TextureRoot/TEXT("content/"));
        IFileManager::Get().DeleteDirectory(*MaterialRoot,false,true);
        IFileManager::Get().DeleteDirectory(*TextureRoot,false,true);
        const uint64 AfterFrame=GFrameCounter+2;
        ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this,Settings,Policy,HadTexture,HadMaterial,AfterFrame]()
        {
            if (GFrameCounter<AfterFrame) return false;
            if (!HadMaterial) ProjectMaterialFixtureDomain::Shutdown();
            if (!HadTexture) ProjectTextureFixtureDomain::Shutdown();
            bool Same=Policy.Num()==Settings->AdditionalDomains.Num();
            for (int32 I=0; Same && I<Policy.Num(); ++I) Same=FARPDomainDefinitionByContentRoot::StaticStruct()->CompareScriptStruct(&Policy[I],&Settings->AdditionalDomains[I],0);
            TestTrue(TEXT("Fixture domains restore exactly after native validators"),Same); return true;
        }));
    };
    FProjectTextureGenerationRequest Texture;
    Texture.RecipeRoot=TextureRoot/TEXT("recipes"); Texture.OutputPackageRoot=TEXT("/ProjectTextureTest/Patterns");
    Texture.OutputContentRoot=TextureRoot/TEXT("content/Patterns"); Texture.ManifestRoot=TextureRoot/TEXT("manifests");
    const FString Source=FPaths::ProjectPluginsDir()/TEXT("Resources/ProjectTexture/Data/TestFixtures/Aggregate/project_asphalt_fixture.pattern.json");
    const FString Dest=Texture.RecipeRoot/TEXT("Aggregate/project_asphalt_fixture.pattern.json");
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Dest),true);
    FString Json,Error;
    if (!FFileHelper::LoadFileToString(Json,*Source) || !FFileHelper::SaveStringToFile(Json,*Dest)) return false;
    FPackageName::RegisterMountPoint(TEXT("/ProjectTextureTest/"),TextureRoot/TEXT("content/"));
    FProjectTextureGenerationResult Textures;
    if (!TestTrue(TEXT("Existing owner produces authenticated two-output fixture"),FProjectTextureGenerationService::RegenerateTestMount(Texture,Textures))) {AddError(Textures.Error); return false;}
    for (const FString& Path : Textures.OutputObjectPaths) Packages.AddUnique(FPackageName::ObjectPathToPackageName(Path));
    FProjectMaterialSurfaceGenerationRequest Request;
    if (!FProjectMaterialSurfaceGenerationService::MakeTestRequest(MaterialRoot,FString(),Request,Error)) return false;
    Request.PatternPackageRoot=TEXT("/ProjectTextureTest"); Request.PatternContentRoot=TextureRoot/TEXT("content");
    Request.PatternManifestPath=Texture.ManifestRoot/TEXT("accepted.pattern-manifest.json");
    FPackageName::RegisterMountPoint(TEXT("/ProjectMaterialTest/"),MaterialRoot/TEXT("content/"));
    for (const TCHAR* Name : {TEXT("M_ProjectAsphaltFixture"),TEXT("MI_ProjectAsphaltFixture")})
    {
        const FString From=FPaths::ProjectPluginsDir()/TEXT("Resources/ProjectMaterial/Data/TestFixtures/Asphalt")/(FString(Name)+TEXT(".surface.json"));
        const FString To=Request.RecipeRoot/TEXT("Asphalt")/(FString(Name)+TEXT(".surface.json"));
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(To),true);
        if (!FFileHelper::LoadFileToString(Json,*From)) return false;
        Json.ReplaceInline(TEXT("world_metric_xy_2d"),TEXT("world_metric_triplanar_2d"));
        if (!FFileHelper::SaveStringToFile(Json,*To)) return false;
    }
    const FString ParentRecipe=Request.RecipeRoot/TEXT("Asphalt/M_ProjectAsphaltFixture.surface.json");
    FProjectMaterialSurfaceRecipe Recipe;
    if (!FFileHelper::LoadFileToString(Json,*ParentRecipe)) return false;
    if (!TestTrue(TEXT("Generic aggregate accepts world metric triplanar"),FProjectMaterialSurfaceRecipeContract::Parse(Json,ParentRecipe,Request.RecipeRoot,Recipe,Error))) {AddError(Error); return false;}
    TestEqual(TEXT("Projection identity is retained"),Recipe.ProjectionFrame,EProjectMaterialProjectionFrame::WorldMetricTriplanar2D);
    FProjectMaterialSurfaceRecipe Bad;
    TestFalse(TEXT("Unsupported object-relative projection refuses"),FProjectMaterialSurfaceRecipeContract::Parse(Json.Replace(TEXT("world_metric_triplanar_2d"),TEXT("object_metric_triplanar_2d")),ParentRecipe,Request.RecipeRoot,Bad,Error));
    FProjectMaterialSurfaceGenerationResult First;
    if (!TestTrue(TEXT("Save and reload triplanar parent and instance through existing compiler"),FProjectMaterialSurfaceGenerationService::RegenerateTestMount(Request,First))) {AddError(First.Error); return false;}
    for (const FString& Path : First.OutputObjectPaths) Packages.AddUnique(FPackageName::ObjectPathToPackageName(Path));
    TestEqual(TEXT("Two authenticated material artifacts"),First.Generated,2);
    FProjectMaterialSurfaceDependency Pattern;
    if (!FProjectMaterialPatternAuthority::Resolve(TEXT("project_asphalt_fixture"),Request.PatternManifestPath,Request.PatternPackageRoot,Request.PatternContentRoot,Pattern,Error)) return false;
    auto* Parent=LoadObject<UMaterial>(nullptr,TEXT("/ProjectMaterialTest/Surfaces/Asphalt/M_ProjectAsphaltFixture.M_ProjectAsphaltFixture"));
    if (!TestNotNull(TEXT("Cold-reloaded parent"),Parent)) return false;
    TestTrue(TEXT("Exact source and projection routes authenticate"),ProjectMaterialAggregateSurface::Verify(Recipe,Pattern,Parent,Error));
    bool SignControl=false;
    for (auto* N : UMaterialEditingLibrary::GetMaterialExpressions(Parent)) if (auto* S=Cast<UMaterialExpressionSign>(N))
    {
        const auto Input=S->Input; S->Input.Expression=nullptr;
        TestFalse(TEXT("Missing outward-axis sign refuses"),ProjectMaterialAggregateSurface::Verify(Recipe,Pattern,Parent,Error));
        S->Input=Input; SignControl=true; break;
    }
    TestTrue(TEXT("Negative control reaches a real connected sign"),SignControl);
    Parent->GetOutermost()->SetDirtyFlag(false);
    FProjectMaterialSurfaceGenerationResult Warm;
    TestTrue(TEXT("Unchanged projection reuses saved resources"),FProjectMaterialSurfaceGenerationService::RegenerateTestMount(Request,Warm));
    TestEqual(TEXT("No warm asset writes"),Warm.Generated,0);
    TestEqual(TEXT("Both material resources reused"),Warm.Skipped,2);
    return true;
}
#endif
