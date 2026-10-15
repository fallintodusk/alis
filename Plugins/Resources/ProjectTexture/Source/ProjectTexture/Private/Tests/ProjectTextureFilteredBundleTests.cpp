// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectTextureBundleTestProducer.h"
#include "ProjectTextureRuntimeCatalog.h"
#include "ProjectTextureRuntimeSubsystem.h"
#include "Engine/Engine.h"
#include "Materials/Material.h"
#include "Misc/AutomationTest.h"
#include "TextureResource.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectTextureFilteredBundleTests
{
class FVerifyExports final : public IAutomationLatentCommand
{
public:
    explicit FVerifyExports(FAutomationTestBase* InTest):Test(InTest),Catalog(NewObject<UProjectTextureRuntimeCatalog>()),
        Producer(NewObject<UProjectTextureBundleTestProducer>()),Started(FPlatformTime::Seconds())
    {
        Catalog->SemanticIdentity=FString::ChrN(64,'7'); Catalog->OutputContract=TEXT("soil_structure_normal_masks_v1");
        Catalog->BudgetBytes=4*1024*1024;
        const TCHAR* Names[]={TEXT("Raw"),TEXT("Structure"),TEXT("Normal"),TEXT("Masks")};
        for (int32 I=0;I<4;++I)
        {
            FProjectTextureRuntimeNodeDescriptor N; N.NodeId=FName(Names[I]); N.StructuralIdentity=FString::ChrN(64,TCHAR('a'+I));
            N.Width=N.Height=I ? 16 : 8; N.GeneratorMaterial=UMaterial::GetDefaultMaterial(MD_Surface);
            if (I) { N.ParentNodeIds.Add(I==1 ? TEXT("Raw") : TEXT("Structure")); N.ParentTextureParameter=TEXT("ParentStructure"); }
            Catalog->Nodes.Add(N);
            if (I)
            {
                auto* Target=NewObject<UTextureRenderTarget2D>(Catalog->GetOuter());
                auto* Slot=NewObject<UProjectTextureOutputSlot>(Target); Slot->Catalog=Catalog.Get(); Slot->NodeId=N.NodeId; Slot->SlotName=N.NodeId;
                Target->AddAssetUserData(Slot); Catalog->OutputSlots.Add(Slot);
            }
        }
        Producer->Identity=Catalog->SemanticIdentity; Producer->OutputSize=16;
        FString Error;
        Test->TestEqual(TEXT("Private raw dimensions do not replace exported descriptors"),GEngine->GetEngineSubsystem<UProjectTextureRuntimeSubsystem>()->RequestBundle(Catalog->OutputSlots[0],Producer.Get(),true,Error),EProjectTextureBundleRequest::Pending);
    }
    bool Update() override
    {
        auto* Runtime=GEngine->GetEngineSubsystem<UProjectTextureRuntimeSubsystem>();
        if (!Runtime->IsBundleReady(Catalog->OutputSlots[0]))
        {
            if (FPlatformTime::Seconds()-Started<10) return false;
            Test->AddError(TEXT("Filtered exported slots did not become ready from their own descriptors.")); return true;
        }
        for (int32 I=0;I<3;++I)
        {
            auto* T=Catalog->OutputSlots[I]->GetRenderTarget();
            Test->TestEqual(TEXT("Every exported target uses the declared output size"),T->SizeX,16);
            Test->TestEqual(TEXT("Every exported target has a complete mip chain"),int32(T->GetResource()->TextureRHI->GetDesc().NumMips),5);
            TArray<FColor> Pixels; FReadSurfaceDataFlags Flags(RCM_UNorm); Flags.SetLinearToGamma(false);
            if (!Test->TestTrue(TEXT("Read actual copied output"),T->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags)) || Pixels.IsEmpty()) return true;
            Test->TestTrue(TEXT("Distinct semantic exports keep their producer pixels"),FMath::Abs(int32(Pixels[0].R)-FMath::RoundToInt((.2f+.1f*I)*255))<=1);
        }
        FString Error; Test->TestTrue(TEXT("Consumer releases the filtered bundle"),Runtime->ReleaseBundle(Catalog->SemanticIdentity,Error));
        Test->TestEqual(TEXT("Unpinned filtered outputs evict together"),Runtime->EvictReleasedBundles(),1);
        for (const auto& Slot : Catalog->OutputSlots) Test->TestNull(TEXT("Eviction releases every exported RHI resource"),Slot->GetRenderTarget()->GetResource());
        return true;
    }
private:
    FAutomationTestBase* Test;
    TStrongObjectPtr<UProjectTextureRuntimeCatalog> Catalog;
    TStrongObjectPtr<UProjectTextureBundleTestProducer> Producer;
    double Started;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectTextureFilteredBundleTest,"Project.Texture.Runtime.FilteredBundleExports",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FProjectTextureFilteredBundleTest,"Project.Texture.Runtime.FilteredBundleExports","[Slow][Integration][ProjectTexture]")
bool FProjectTextureFilteredBundleTest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(ProjectTextureFilteredBundleTests::FVerifyExports(this)); return true;
}
#endif
