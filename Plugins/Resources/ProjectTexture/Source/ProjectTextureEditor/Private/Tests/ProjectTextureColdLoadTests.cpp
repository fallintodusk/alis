// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectTextureRuntimeCatalog.h"
#include "ProjectTextureRuntimeSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "RenderingThread.h"
#include "TextureResource.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
class FColdAggregateProof final : public IAutomationLatentCommand
{
public:
    explicit FColdAggregateProof(FAutomationTestBase* InTest) : Test(InTest),Start(FPlatformTime::Seconds()) {}
    virtual ~FColdAggregateProof() override
    {
        if (World.IsValid()) {GEngine->DestroyWorldContext(World.Get()); World->DestroyWorld(false);}
    }
    virtual bool Update() override
    {
        auto* Owner=GEngine->GetEngineSubsystem<UProjectTextureRuntimeSubsystem>();
        if (!World.IsValid())
        {
            const TCHAR* Path=TEXT("/ProjectTexture/Patterns/Aggregate/DA_ProjectRoadAggregate.DA_ProjectRoadAggregate");
            if (!Test->TestNull(TEXT("The aggregate package is genuinely cold"),FindObject<UProjectTextureRuntimeCatalog>(nullptr,Path))) return true;
            World.Reset(UWorld::CreateWorld(EWorldType::Game,false));
            GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World.Get());
            Before=Owner->GetTelemetry();
            Catalog.Reset(LoadObject<UProjectTextureRuntimeCatalog>(nullptr,Path));
            if (!Test->TestNotNull(TEXT("Saved multi-output aggregate loads during a generation world"),Catalog.Get())) return true;
            if (!FPackageName::DoesPackageExist(Catalog->GetOutermost()->GetName(),&Filename) || !FFileHelper::LoadFileToArray(PackageBefore,*Filename)) return true;
            Test->TestEqual(TEXT("PostLoad notification must not draw inside the load lifecycle"),Owner->GetTelemetry().Generations,Before.Generations);
            FirstFrame=GFrameCounter;
            return false;
        }
        if (FPlatformTime::Seconds()-Start>20) {Test->AddError(TEXT("Cold aggregate did not become ready within 20 seconds")); return true;}
        if (GFrameCounter<FirstFrame+2) return false;
        if (!Owner->IsOutputReady(Catalog->OutputSlots[0]) || !Owner->IsOutputReady(Catalog->OutputSlots[1])) return false;
        auto* Target=Catalog->OutputSlots[1]->GetRenderTarget();
        TArray<FColor> Pixels;
        FReadSurfaceDataFlags Flags(RCM_MinMax,CubeFace_MAX); Flags.SetLinearToGamma(false);
        FlushRenderingCommands();
        if (!Test->TestTrue(TEXT("Read the actual cached linear RGBA8 bytes"),Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags,FIntRect(0,0,Target->SizeX,Target->SizeY)))) return true;
        Test->TestEqual(TEXT("Every generated Normal pixel exists"),Pixels.Num(),Target->SizeX*Target->SizeY);
        float MinimumZ=1,MaximumLengthError=0;
        for (const auto& P : Pixels)
        {
            const FVector N(P.R*(2.0/255)-1,P.G*(2.0/255)-1,P.B*(2.0/255)-1);
            MinimumZ=FMath::Min(MinimumZ,float(N.Z));
            MaximumLengthError=FMath::Max(MaximumLengthError,float(FMath::Abs(N.Size()-1)));
        }
        Test->AddInfo(FString::Printf(TEXT("cold_normal_pixels=%d minimum_z=%.6f maximum_length_error=%.6f"),Pixels.Num(),MinimumZ,MaximumLengthError));
        Test->TestTrue(TEXT("Normals face outward instead of a cleared black target"),MinimumZ>.7f);
        Test->TestTrue(TEXT("Encoded normals retain unit length"),MaximumLengthError<.03f);
        TStrongObjectPtr<UTextureRenderTarget2D> Literal(NewObject<UTextureRenderTarget2D>());
        Literal->RenderTargetFormat=RTF_RGBA8; Literal->SRGB=false; Literal->bForceLinearGamma=true;
        Literal->ClearColor=FLinearColor(.5f,.5f,1,1); Literal->InitAutoFormat(2,2); Literal->UpdateResourceImmediate(true);
        TArray<FColor> LiteralPixels;
        Test->TestTrue(TEXT("Literal encoded-flat control is readable"),Literal->GameThread_GetRenderTargetResource()->ReadPixels(LiteralPixels,Flags));
        Test->TestTrue(TEXT("Readback preserves linear encoded bytes without sRGB decoding"),LiteralPixels.Num()==4 &&
            FMath::Abs(int32(LiteralPixels[0].R)-128)<=1 && FMath::Abs(int32(LiteralPixels[0].G)-128)<=1 && LiteralPixels[0].B==255);
        const auto Telemetry=Owner->GetTelemetry();
        const auto* Rhi=Target->GetResource()->TextureRHI.GetReference();
        FString Error;
        Test->TestTrue(TEXT("Warm request uses the same completed catalog"),Owner->RequestOutput(World.Get(),Catalog->OutputSlots[1],Error));
        Test->TestEqual(TEXT("Warm request generates no nodes"),Owner->GetTelemetry().Generations,Telemetry.Generations);
        Test->TestTrue(TEXT("Warm request retains the same texture resource"),Target->GetResource()->TextureRHI.GetReference()==Rhi);
        Target->UpdateResource();
        FlushRenderingCommands();
        Test->TestFalse(TEXT("Native resource recreation invalidates readiness"),Owner->IsOutputReady(Catalog->OutputSlots[1]));
        Test->TestTrue(TEXT("A later request recovers the entire affected catalog"),Owner->RequestOutput(World.Get(),Catalog->OutputSlots[1],Error));
        Test->TestEqual(TEXT("Recovery generates exactly the two affected nodes"),Owner->GetTelemetry().Generations-Telemetry.Generations,2);
        Test->TestTrue(TEXT("Both outputs are ready after recovery"),Owner->IsOutputReady(Catalog->OutputSlots[0]) && Owner->IsOutputReady(Catalog->OutputSlots[1]));
        Pixels.Reset();
        Test->TestTrue(TEXT("Recovered Normal bytes are readable"),Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags,FIntRect(0,0,Target->SizeX,Target->SizeY)));
        Test->TestTrue(TEXT("Recovery replaces cleared pixels with encoded normals"),!Pixels.IsEmpty() && Pixels[0].B>230);
        TArray<uint8> After;
        Test->TestTrue(TEXT("Generation preserves saved source package bytes"),FFileHelper::LoadFileToArray(After,*Filename) && After==PackageBefore);
        return true;
    }
private:
    FAutomationTestBase* Test;
    double Start;
    uint64 FirstFrame=0;
    FProjectTextureRuntimeTelemetry Before;
    TStrongObjectPtr<UWorld> World;
    TStrongObjectPtr<UProjectTextureRuntimeCatalog> Catalog;
    FString Filename;
    TArray<uint8> PackageBefore;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectTextureColdLoadTest,
    "Project.Texture.Runtime.ColdLoadedAggregate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter | EAutomationTestFlags::NonNullRHI)
bool FProjectTextureColdLoadTest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(FColdAggregateProof(this));
    return true;
}
#endif
