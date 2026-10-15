// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectTexturePatternBuilder.h"
#include "ProjectTexturePatternRecipe.h"
#include "ProjectTextureRuntimeCatalog.h"
#include "ProjectTextureRuntimeSubsystem.h"
#include "Utilities/ProjectSha256.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectHash.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectTextureGradedSourceTest,"Project.Texture.GradedParticles.FrozenSource",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FProjectTextureGradedSourceTest,"Project.Texture.GradedParticles.FrozenSource",
    "[Slow][Integration][ProjectTexture]")
bool FProjectTextureGradedSourceTest::RunTest(const FString&)
{
    const FString Root=FPaths::Combine(FPaths::ProjectPluginsDir(),TEXT("Resources/ProjectTexture/Data/TestFixtures"));
    const FString Path=FPaths::Combine(Root,TEXT("GradedParticles/project_graded_fixture.pattern.json"));
    FString Json,Error; FProjectTexturePatternRecipe Recipe;
    if (!FFileHelper::LoadFileToString(Json,*Path) ||
        !TestTrue(TEXT("Frozen graded profile is admitted"),FProjectTexturePatternRecipeContract::Parse(Json,Path,Root,Recipe,Error)))
    { AddError(Error); return false; }
    UObject* Asset=nullptr;
    const FString Package=TEXT("/ProjectTextureTest/Verification/Graded_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
    if (!TestTrue(TEXT("Build the owner-generated four-stage source"),ProjectTexturePatternBuilder::Build(Recipe,Package,Asset,Error)))
    { AddError(Error); return false; }
    auto* Slot=CastChecked<UProjectTextureOutputSlot>(CastChecked<UTextureRenderTarget2D>(Asset)->GetAssetUserDataOfClass(UProjectTextureOutputSlot::StaticClass()));
    TStrongObjectPtr<UProjectTextureRuntimeCatalog> Catalog(Slot->Catalog);
    TArray<UObject*> Owned; GetObjectsWithOuter(Asset->GetOutermost(),Owned);
    int32 Targets=0; for (const auto* O : Owned) Targets+=O->IsA<UTextureRenderTarget2D>() ? 1 : 0;
    TestEqual(TEXT("Only the three exported render targets persist in the generated package"),Targets,3);
    if (!TestTrue(TEXT("Generated source authenticates"),ProjectTexturePatternBuilder::Verify(Recipe,Asset,Error)))
    { AddError(Error); return false; }
    auto* NormalMaterial=Catalog->Nodes[2].GeneratorMaterial.Get();
    auto* MasksMaterial=Catalog->Nodes[3].GeneratorMaterial.Get();
    Catalog->Nodes[2].GeneratorMaterial=MasksMaterial;
    Catalog->Nodes[3].GeneratorMaterial=NormalMaterial;
    TestFalse(TEXT("Swapped derived kernels must not authenticate their semantic exports"),ProjectTexturePatternBuilder::Verify(Recipe,Asset,Error));
    Catalog->Nodes[2].GeneratorMaterial=NormalMaterial;
    Catalog->Nodes[3].GeneratorMaterial=MasksMaterial;
    auto* Structure=Catalog->OutputSlots[0]->GetRenderTarget();
    Structure->MipsAddressU=TA_Clamp;
    TestFalse(TEXT("Different mip addressing must not authenticate"),ProjectTexturePatternBuilder::Verify(Recipe,Asset,Error));
    Structure->MipsAddressU=TA_Wrap;
    auto* Runtime=GEngine->GetEngineSubsystem<UProjectTextureRuntimeSubsystem>();
    const auto Before=Runtime->GetTelemetry();
    if (!TestTrue(TEXT("Existing runtime consumes the complete DAG"),Runtime->RequestOutput(GEditor->GetEditorWorldContext().World(),Slot,Error)))
    { AddError(Error); return false; }
    TestEqual(TEXT("Raw and derived stages execute once"),Runtime->GetTelemetry().Generations-Before.Generations,4);
    FlushRenderingCommands();
    const TCHAR* Hashes[]={
        TEXT("231608ca7bbaa40401de4c33417c9b6aeb4b3fc0ab67d65de0c2b971aab6b6bc"),
        TEXT("2df07279e7c9e698878287307d4d632cab64c9797d5ea851fdbde30b1f3f8e49"),
        TEXT("7eeaa5f404d5548602767b90dc9a75f43b78f5dc05940f3c3fea920cfef10e52")};
    const FString Evidence=FPaths::Combine(FPaths::ProjectDir(),TEXT("tmp/texture/verification/graded_particles"));
    IFileManager::Get().MakeDirectory(*Evidence,true);
    for (int32 I=0;I<3;++I)
    {
        TArray<FColor> Pixels; FReadSurfaceDataFlags Flags(RCM_UNorm); Flags.SetLinearToGamma(false);
        if (!TestTrue(TEXT("Read actual linear output pixels"),Catalog->OutputSlots[I]->GetRenderTarget()->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags))) return false;
        TestEqual(TEXT("Default residency stays 512 square"),Pixels.Num(),512*512);
        TArray<uint8> RGB; RGB.Reserve(Pixels.Num()*3);
        for (const FColor& P : Pixels) RGB.Append({P.R,P.G,P.B});
        FString Hash; FProjectSha256::HashBuffer(RGB,Hash);
        TestEqual(TEXT("Generated pixels preserve the accepted compacted-earth source"),Hash,FString(Hashes[I]));
        FFileHelper::SaveArrayToFile(RGB,*(Evidence/Catalog->OutputSlots[I]->SlotName.ToString()+TEXT(".rgb")));
    }
    const auto Cold=Runtime->GetTelemetry();
    for (int32 I=0;I<9;++I) TestTrue(TEXT("Consumers share stable exports"),Runtime->RequestOutput(GEditor->GetEditorWorldContext().World(),Catalog->OutputSlots[I%3],Error));
    TestEqual(TEXT("Warm requests do not redraw"),Runtime->GetTelemetry().Generations,Cold.Generations);
    return true;
}
#endif
