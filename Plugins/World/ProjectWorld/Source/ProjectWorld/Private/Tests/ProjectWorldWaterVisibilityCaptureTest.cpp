// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "Presentation/ProjectWorldWaterVisibilityCapture.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Materials/Material.h"
#include "MaterialShared.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
    struct FWaterVisibilityProof
    {
        UWorld* World=nullptr;
        AStaticMeshActor* Water=nullptr;
        AStaticMeshActor* Occluder=nullptr;
        UMaterialInterface* River=nullptr;
        FProjectWorldWaterVisibilityCapture Mask;
        ProjectWorldRuntimeScreenshotCapture::FCaptureSession Legacy;
        ProjectWorldRuntimeScreenshotCapture::FCaptureSpec Spec;
        FString Root,Session;
        int32 Phase=0,Full=0;
        ~FWaterVisibilityProof()
        {
            Legacy.Reset();Mask.Reset();
            if (World) {GEngine->DestroyWorldContext(World);World->DestroyWorld(false);}
        }
    };
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldWaterVisibilityCaptureTest,
    "Project.World.Presentation.WaterVisibilityCapture",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FProjectWorldWaterVisibilityCaptureTest::RunTest(const FString&)
{
    const auto S=MakeShared<FWaterVisibilityProof>();
    // Real engine frames separate proxy registration, captures and moving controls.
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FFunctionLatentCommand>([this,S]()
    {
        FString Error;
        const auto Capture=[&](const TCHAR* Name)
        {
            if (S->Mask.Capture(S->Root/Name,Error)) return true;
            AddError(Error);return false;
        };
        switch (S->Phase++)
        {
        case 0:
        {
            S->World=UWorld::CreateWorld(EWorldType::Game,false,FName(*FGuid::NewGuid().ToString()));
            if (!TestNotNull(TEXT("Disposable native rendering world"),S->World)) return true;
            GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(S->World);
            auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
            S->River=LoadObject<UMaterialInterface>(nullptr,
                TEXT("/ProjectMaterial/Surfaces/Water/MI_ProjectRiverCalm_Default.MI_ProjectRiverCalm_Default"));
            if (!TestNotNull(TEXT("Native fixture mesh"),Cube) || !TestNotNull(TEXT("Actual RiverCalm"),S->River)) return true;
            auto* Resource=S->River->GetMaterial()->GetMaterialResource(GMaxRHIShaderPlatform);
            if (!TestNotNull(TEXT("Native RiverCalm shader resource"),Resource)) return true;
            Resource->FinishCompilation();
            FActorSpawnParameters Spawn;Spawn.ObjectFlags=RF_Transient;
            S->Water=S->World->SpawnActor<AStaticMeshActor>(FVector(0,0,-.5),FRotator::ZeroRotator,Spawn);
            if (!TestNotNull(TEXT("Native Water fixture actor"),S->Water)) return true;
            auto* C=S->Water->GetStaticMeshComponent();C->bDisallowNanite=true;
            C->SetMobility(EComponentMobility::Movable);C->SetStaticMesh(Cube);C->SetMaterial(0,S->River);
            S->Water->SetActorScale3D(FVector(8,8,.01));S->Water->Tags.Add(TEXT("ProjectWorld.Water.v1"));
            S->Spec.CameraLocation=FVector(0,0,1000);S->Spec.CameraRotation=FRotator(-90,0,0);
            S->Spec.OrthographicWidthCentimeters=1000;S->Spec.SourceIdentity=TEXT("water_visibility_fixture");
            S->Root=FPaths::ProjectDir()/TEXT("tmp/world/water_visibility")/FGuid::NewGuid().ToString();
            S->World->SendAllEndOfFrameUpdates();
            break;
        }
        case 1:
            if (!S->Mask.Initialize(*S->World,S->Spec,Error) || !S->Mask.WarmUp(3,Error)) {AddError(Error);return true;}
            break;
        case 2:
            if (FParse::Param(FCommandLine::Get(),TEXT("ProjectWaterVisibilityDump")))
                for (TActorIterator<ASceneCapture2D> It(S->World);It;++It)
                    if (It->GetCaptureComponent2D()->PrimitiveRenderMode==ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList)
                    {
                        It->GetCaptureComponent2D()->ProfilingEventName=TEXT("WaterVisibilityFixture");
                        It->GetCaptureComponent2D()->bDumpGpuNextRender=true;
                    }
            if (!Capture(TEXT("visible.png"))) return true;
            S->Full=S->Mask.GetVisiblePixelCount();S->Session=S->Mask.GetSessionId();
            TestTrue(TEXT("Actual Single Layer Water produces substantial coverage"),S->Full>4096);
            TestTrue(TEXT("Empty background is excluded"),S->Full<1920*1080*.9);
            break;
        case 3:
        {
            if (!Capture(TEXT("repeat.png"))) return true;
            TestEqual(TEXT("Unmoved Water retains coverage across engine frames"),S->Mask.GetVisiblePixelCount(),S->Full);
            TestEqual(TEXT("Both masks retain one native capture pair"),S->Mask.GetSessionId(),S->Session);
            FProjectWorldWaterVisibilityCapture Immediate;
            if (!Immediate.Initialize(*S->World,S->Spec,Error) || !Immediate.WarmUp(3,Error) ||
                !Immediate.Capture(S->Root/TEXT("same-tick-session.png"),Error)) {AddError(Error);return true;}
            TestEqual(TEXT("Settled Water supports Shipping's same-tick initialize/warmup/capture"),
                Immediate.GetVisiblePixelCount(),S->Full);
            Immediate.Reset();
            auto Spec=S->Spec;Spec.CaptureSource=SCS_BaseColor;Spec.bIsolateBaseColor=true;
            if (!S->Legacy.Initialize(*S->World,Spec,Error) || !S->Legacy.WarmUp(3,Error)) {AddError(Error);return true;}
            TestFalse(TEXT("Black physical Water is not the old blue beauty oracle"),
                S->Legacy.Capture(S->Root/TEXT("legacy-base-color.png"),Error));
            S->Legacy.Reset();
            break;
        }
        case 4:
        {
            FActorSpawnParameters Spawn;Spawn.ObjectFlags=RF_Transient;
            S->Occluder=S->World->SpawnActor<AStaticMeshActor>(FVector(-200,0,100),FRotator::ZeroRotator,Spawn);
            if (!TestNotNull(TEXT("Native occluder fixture actor"),S->Occluder)) return true;
            auto* C=S->Occluder->GetStaticMeshComponent();C->SetMobility(EComponentMobility::Movable);
            C->SetStaticMesh(S->Water->GetStaticMeshComponent()->GetStaticMesh());
            S->Occluder->SetActorScale3D(FVector(4,8,.01));S->World->SendAllEndOfFrameUpdates();
            break;
        }
        case 5:
            if (!Capture(TEXT("occluded.png"))) return true;
            TestTrue(TEXT("Real opaque occlusion removes half the coverage"),
                S->Mask.GetVisiblePixelCount()>S->Full*.35 && S->Mask.GetVisiblePixelCount()<S->Full*.65);
            break;
        case 6:
            S->Occluder->SetActorScale3D(FVector(8,8,.01));S->Occluder->SetActorLocation(FVector(0,0,-.45));
            S->World->SendAllEndOfFrameUpdates();
            break;
        case 7:
            if (!Capture(TEXT("competing.png"))) return true;
            TestTrue(TEXT("A surface 0.05 cm above Water cannot masquerade as Water"),S->Mask.GetVisiblePixelCount()<S->Full*.01);
            TestEqual(TEXT("Accepted binding remains exact"),S->Water->GetStaticMeshComponent()->GetMaterial(0),S->River);
            break;
        case 8:
        {
            const int32 Written=S->Mask.GetWrittenCaptureCount();S->Occluder->Tags.Add(TEXT("ProjectWorld.Water.v1"));
            TestFalse(TEXT("Changed registered membership refuses"),S->Mask.Capture(S->Root/TEXT("membership-changed.png"),Error));
            TestEqual(TEXT("Membership refusal writes no mask"),S->Mask.GetWrittenCaptureCount(),Written);
            S->Occluder->Tags.Reset();
            if (!Capture(TEXT("membership-restored.png"))) return true;
            S->Mask.Reset();S->Water->Tags.Reset();
            TestFalse(TEXT("Missing canonical Water refuses"),S->Mask.Initialize(*S->World,S->Spec,Error));
            AddInfo(TEXT("Water visibility control artifacts: ")+FPaths::ConvertRelativePathToFull(S->Root));
            return true;
        }
        }
        return false;
    }));
    return true;
}
#endif
