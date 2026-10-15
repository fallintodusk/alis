// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldPresentationProfile.h"
#include "ProjectWorldPresentationMaterialBinding.h"
#include "Tests/ProjectWorldSchemaTestUtilities.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldMaterialBindingTest,
    "Project.World.Presentation.AuthenticatedReference",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldMaterialBindingTest::RunTest(const FString&)
{
    FString Error; FProjectWorldPresentationResources R;
    if (!TestTrue(TEXT("Accepted terrain reference resolves"),ProjectWorldPresentationMaterialBinding::ResolveTerrain(R,Error))) {AddError(Error); return false;}
    auto* Instance=CastChecked<UMaterialInstanceConstant>(R.TerrainMaterial);
    auto* Package=Instance->GetOutermost(); auto* Parent=Instance->Parent->GetOutermost();
    const bool Dirty=Package->IsDirty(),ParentDirty=Parent->IsDirty();
    ON_SCOPE_EXIT {Package->SetDirtyFlag(Dirty); Parent->SetDirtyFlag(ParentDirty);};
    Package->SetDirtyFlag(true);
    TestFalse(TEXT("Disk hashes cannot authenticate unsaved instance state"),ProjectWorldPresentationMaterialBinding::ResolveTerrain(R,Error));
    Package->SetDirtyFlag(false); Parent->SetDirtyFlag(true);
    TestFalse(TEXT("Disk hashes cannot authenticate unsaved parent state"),ProjectWorldPresentationMaterialBinding::ResolveTerrain(R,Error));
    Parent->SetDirtyFlag(false);
    TestTrue(TEXT("Exact restored reference resolves"),ProjectWorldPresentationMaterialBinding::ResolveTerrain(R,Error));
    FProjectWorldMaterialReference Object;
    TestTrue(TEXT("The same consumer resolves another accepted semantic binding"),ProjectWorldPresentationMaterialBinding::Resolve(TEXT("object.metric_cube"),Object,Error));
    TestEqual(TEXT("Generic binding returns authenticated package identity"),Object.PackageSha256.Len(),64);
    TestEqual(TEXT("Generic binding does not inherit the terrain object"),Object.ObjectPath,FString(TEXT("/ProjectMaterial/Surfaces/Object/MI_ProjectMetricCube.MI_ProjectMetricCube")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldMaterialBindingProfileTest,
    "Project.World.Presentation.SemanticResourceContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldMaterialBindingProfileTest::RunTest(const FString&)
{
    const FString Root=FPaths::ProjectDir()/TEXT("tmp/world/verification/material_bindings/")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
    IFileManager::Get().MakeDirectory(*Root,true);
    ON_SCOPE_EXIT {IFileManager::Get().DeleteDirectory(*Root,false,true);};
    const FString From=FPaths::ProjectPluginsDir()/TEXT("World/ProjectWorldTestData/Data/Presentation/synthetic_representative_v1.json");
    FString Original; if (!FFileHelper::LoadFileToString(Original,*From)) return false;
    const FString File=Root/TEXT("profile.json");
    FString Source=ProjectWorldSchemaTestUtilities::Rewrite(Original,File,TEXT("project_world_presentation_profile.schema.json"));
    if (!TestEqual(TEXT("Replace exactly one road declaration"),Source.ReplaceInline(
        TEXT("\"road\": \"/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial\""),
        TEXT("\"road\": \"road.unpublished_fixture\"")),1)) return false;
    if (!FFileHelper::SaveStringToFile(Source,*File)) return false;
    FProjectWorldPresentationProfile P; FString Code,Error;
    if (!TestTrue(TEXT("Logical road binding is a profile declaration"),ProjectWorldPresentationProfile::Load(File,P,Code,Error))) {AddError(Error); return false;}
    FProjectWorldPresentationResources R;
    TestFalse(TEXT("Missing requested binding refuses instead of Engine fallback"),ProjectWorldPresentationProfile::ResolveResources(P,R,Error));
    Source.ReplaceInline(TEXT("road.unpublished_fixture"),TEXT("water.default"));
    FFileHelper::SaveStringToFile(Source,*File);
    TestFalse(TEXT("Wrong semantic surface role refuses"),ProjectWorldPresentationProfile::Load(File,P,Code,Error));
    return true;
}
#endif
