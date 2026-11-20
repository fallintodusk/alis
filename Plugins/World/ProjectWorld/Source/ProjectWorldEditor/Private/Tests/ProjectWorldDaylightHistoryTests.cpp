#include "ProjectLightComponent.h"
#include "ProjectTimeSubsystem.h"
#include "GeoReferencingSystem.h"
#include "ChartCreation.h"
#include "ContentStreaming.h"
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#include "Editor.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/PostProcessVolume.h"
#include "EngineUtils.h"
#include "Components/DirectionalLightComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameMapsSettings.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Scalability.h"
#include "SceneView.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Slate/SceneViewport.h"
#include "Tests/AutomationEditorCommon.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
    class FStartHistoryPIE final : public FStartPIEForAutomationCommand
    {
        TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
    public:
        explicit FStartHistoryPIE(const FRequestPlaySessionParams& Request)
            : FStartPIEForAutomationCommand(Request), Settings(Request.EditorPlaySettings.Get()) {}
    };

    class FHistoryFrames final : public IPerformanceDataConsumer
    {
    public:
        FString Phase;
        double Origin = 0;
        FString Rows = TEXT("phase,seconds,frame,frame_ms,game_ms,render_ms,gpu_ms\n");
        virtual void StartCharting() override {}
        virtual void StopCharting() override {}
        virtual void ProcessFrame(const FFrameData& Frame) override
        {
            Rows += FString::Printf(TEXT("%s,%.6f,%llu,%.6f,%.6f,%.6f,%.6f\n"), *Phase,
                FPlatformTime::Seconds()-Origin, GFrameCounter, Frame.TrueDeltaSeconds*1000,
                Frame.GameThreadTimeSeconds*1000, Frame.RenderThreadTimeSeconds*1000, Frame.GPUTimeSeconds*1000);
        }
    };

    class FDaylightHistoryProof final : public IAutomationLatentCommand
    {
        struct FStep { FString Name; int32 Action; double Seconds; bool Capture; };
        FAutomationTestBase& Test;
        FString Root, Arm;
        TWeakObjectPtr<UProjectTimeSubsystem> Time;
        TWeakObjectPtr<UProjectLightComponent> Light;
        TSharedPtr<FHistoryFrames> Frames;
        TArray<FStep> Steps;
        TMap<FString,FString> OriginalCVars;
        TOptional<Scalability::FQualityLevels> OriginalQuality;
        float OriginalNightFloor=0, OriginalSceneSpeed=0;
        bool OriginalSceneOverride=false, SettingsSaved=false, Pending=false, Pausing=false;
        double Started=FPlatformTime::Seconds(), StepStarted=0, JumpStarted=0, ContinuousStarted=0;
        int32 Index=-1;
        uint64 PauseFrame=0;
        FDateTime Sunset;
        FString Captures=TEXT("name,elapsed_seconds,utc,rate,sun_lux,night_lux,width,height,scene_speed,final_gather_speed,camera_x,camera_y,camera_z\n");

        bool BindClock()
        {
            auto* Geo=Cast<AGeoReferencingSystem>(Light->GetOwner());
            if(!Test.TestNotNull(TEXT("Generated geographic Light owner"),Geo))return false;
            FGeographicCoordinates Coordinates;FVector East,North,Up;FString Error;
            Geo->EngineToGeographic(FVector::ZeroVector,Coordinates);Geo->GetENUVectorsAtEngineLocation(FVector::ZeroVector,East,North,Up);
            return Test.TestTrue(TEXT("Explicit clock binding"),Light->Start(*Time,Coordinates.Latitude,Coordinates.Longitude,East,North,Up,Error));
        }
        bool Seek(FDateTime Utc)
        {
            if(!Time->SeekUtc(Utc))return false;
            // Rebinding isolates native controls from automatic runtime seek recovery.
            return (Arm==TEXT("runtime") && Index>=6) || BindClock();
        }

        bool Set(const TCHAR* Name, const TCHAR* Value)
        {
            auto* CVar=IConsoleManager::Get().FindConsoleVariable(Name);
            if(!Test.TestNotNull(Name,CVar))return false;
            if(!OriginalCVars.Contains(Name))OriginalCVars.Add(Name,CVar->GetString());
            CVar->Set(Value,ECVF_SetByConsole);
            return Test.TestEqual(Name,CVar->GetString(),FString(Value));
        }
        void Visualize(bool Enabled)
        {
            if(UWorld* World=GEditor->PlayWorld)
                if(auto* Player=World->GetFirstPlayerController())
                    Player->ConsoleCommand(Enabled?TEXT("vis Lumen.ScreenProbeGather.DiffuseIndirect"):TEXT("vis 0"),true);
        }
        bool Finish()
        {
            Visualize(false);
            if(Frames)
            {
                GEngine->RemovePerformanceDataConsumer(Frames);
                Test.TestTrue(TEXT("Native frame metrics written"),FFileHelper::SaveStringToFile(Frames->Rows,*(Root/TEXT("native-timings.csv"))));
                Frames.Reset();
            }
            Test.TestTrue(TEXT("Capture metadata written"),FFileHelper::SaveStringToFile(Captures,*(Root/TEXT("captures.csv"))));
            if(Light.IsValid() && SettingsSaved)
            {
                Light->Settings.NightMinEv100=OriginalNightFloor;
                auto& Post=Light->Bindings.PostProcess->Settings;
                Post.bOverride_LumenSceneLightingUpdateSpeed=OriginalSceneOverride;
                Post.LumenSceneLightingUpdateSpeed=OriginalSceneSpeed;
            }
            for(const auto& Pair:OriginalCVars)
                if(auto* CVar=IConsoleManager::Get().FindConsoleVariable(*Pair.Key))
                {
                    CVar->Set(*Pair.Value,ECVF_SetByConsole);
                    Test.TestEqual(TEXT("Diagnostic CVar value restored"),CVar->GetString(),Pair.Value);
                }
            if(OriginalQuality.IsSet())Scalability::SetQualityLevels(OriginalQuality.GetValue());
            if(Time.IsValid())Test.TestTrue(TEXT("Real clock restored"),Time->ResetToPcTime());
            if(GEditor->PlayWorld)GEditor->EndPlayMap();
            return true;
        }
        bool BeginStep()
        {
            const auto& Step=Steps[Index];
            StepStarted=FPlatformTime::Seconds();
            Visualize(Step.Action==3);
            if(Step.Action==1)
            {
                if(!Test.TestTrue(TEXT("Admitted clock seeks noon"),Seek(FDateTime(2026,3,21,16))))return false;
            }
            if(Step.Action==2)
            {
                JumpStarted=StepStarted;
                if(!Test.TestTrue(TEXT("Admitted clock seeks midnight"),Seek(FDateTime(2026,3,21,4))))return false;
            }
            if(Step.Action==4)
            {
                if(Arm==TEXT("primary")) {if(!Set(TEXT("r.Lumen.ScreenProbeGather.RadianceCache"),TEXT("0")))return false;}
                if(Arm==TEXT("scene"))
                {
                    auto& Post=Light->Bindings.PostProcess->Settings;
                    Post.bOverride_LumenSceneLightingUpdateSpeed=true;Post.LumenSceneLightingUpdateSpeed=4;
                }
                if(Arm==TEXT("temporal") && !Set(TEXT("r.Lumen.ScreenProbeGather.Temporal.ClearHistoryEveryFrame"),TEXT("1")))return false;
                if(!Test.TestTrue(TEXT("Candidate configured before noon/jump"),Seek(FDateTime(2026,3,21,16))))return false;
            }
            if(Step.Action==5)
            {
                if(!Set(TEXT("r.Lumen.ScreenProbeGather.RadianceCache"),TEXT("1")))return false;
                if(!Test.TestTrue(TEXT("Continuous control seeks solar sunset"),Seek(Sunset)))return false;
            }
            if(Step.Action==6)
            {
                ContinuousStarted=StepStarted;
                if(!Test.TestTrue(TEXT("Actual clock runs at 60x"),Time->SetRate(60)))return false;
            }
            Frames->Phase=Step.Name;
            Test.AddInfo(FString::Printf(TEXT("History step=%s action=%d frame=%llu"),*Step.Name,Step.Action,GFrameCounter));
            return true;
        }
    public:
        FDaylightHistoryProof(FAutomationTestBase& InTest,FString InRoot,FString InArm)
            :Test(InTest),Root(MoveTemp(InRoot)),Arm(MoveTemp(InArm))
        {
            Steps={{TEXT("00-held-midnight-control"),2,75,true},{TEXT("01-baseline-noon"),1,15,true},
                {TEXT("02-baseline-night-5s"),2,5,true},{TEXT("03-baseline-night-15s"),0,15,true},
                {TEXT("04-baseline-diffuse"),3,0.5,true},{TEXT("05-baseline-night-75s"),0,75,true},
                {TEXT("06-candidate-noon"),4,15,true},{TEXT("07-candidate-night-5s"),2,5,true},
                {TEXT("08-candidate-night-15s"),0,15,true},{TEXT("09-candidate-diffuse"),3,0.5,true},
                {TEXT("10-candidate-night-75s"),0,75,true}};
            if(Arm==TEXT("primary"))Steps.Append({{TEXT("11-continuous-sunset-start"),5,15,true},
                {TEXT("12-continuous-15s"),6,15,true},{TEXT("13-continuous-45s"),7,45,true},
                {TEXT("14-continuous-75s-paused"),8,75,true},{TEXT("15-continuous-held-75s"),9,75,true}});
        }
        virtual bool Update() override
        {
            if(FPlatformTime::Seconds()-Started>540){Test.AddError(TEXT("Bounded history diagnostic timed out."));return Finish();}
            UWorld* World=GEditor->PlayWorld;
            auto* Viewport=GEngine->GameViewport?GEngine->GameViewport->GetGameViewport():nullptr;
            if(!World || !Viewport)return false;
            if(Index<0)
            {
                auto* Provider=World->GetSubsystem<UProjectTimeSubsystem>();
                auto* Player=World->GetFirstPlayerController();
                auto* Hero=Player?Cast<ACharacter>(Player->GetPawn()):nullptr;
                if(!World->GetName().Contains(TEXT("Manhattan")) || !Provider || !Provider->CanControlTime() || !Hero)return false;
                Time=Provider;
                AGeoReferencingSystem* Geo=nullptr;
                for(TActorIterator<AGeoReferencingSystem> It(World);It;++It)
                    if(auto* Controller=It->FindComponentByClass<UProjectLightComponent>()){Geo=*It;Light=Controller;}
                if(!Test.TestTrue(TEXT("Existing runtime composition is active"),Geo && Light.IsValid() && Light->GetApplyCount()>0))return Finish();
                OriginalNightFloor=Light->Settings.NightMinEv100;
                auto& Post=Light->Bindings.PostProcess->Settings;
                OriginalSceneOverride=Post.bOverride_LumenSceneLightingUpdateSpeed;
                OriginalSceneSpeed=Post.LumenSceneLightingUpdateSpeed;
                SettingsSaved=true;
                OriginalQuality=Scalability::GetQualityLevels();Scalability::FQualityLevels High;High.SetFromSingleQualityLevel(2);Scalability::SetQualityLevels(High);
                if(!Set(TEXT("r.Lumen.ScreenProbeGather.RadianceCache"),TEXT("1")) ||
                    !Set(TEXT("r.Lumen.ScreenProbeGather.Temporal.ClearHistoryEveryFrame"),TEXT("0")))return Finish();
                Light->Settings.NightMinEv100=-4.5f;
                FGeographicCoordinates Coordinates;FVector East,North,Up;FString Error;
                Geo->EngineToGeographic(FVector::ZeroVector,Coordinates);Geo->GetENUVectorsAtEngineLocation(FVector::ZeroVector,East,North,Up);
                if(!Test.TestTrue(TEXT("Existing Light consumes actual clock"),Light->Start(*Time,Coordinates.Latitude,Coordinates.Longitude,East,North,Up,Error)))return Finish();
                for(int32 Minute=0;Minute<720;++Minute)
                {
                    const auto Utc=FDateTime(2026,3,21,16)+FTimespan::FromMinutes(Minute);
                    FVector Direction;double Elevation;
                    if(ProjectLight::SunDirection(Utc,Coordinates.Latitude,Coordinates.Longitude,East,North,Up,Direction,Elevation) && Elevation<=2 && Elevation>1.5){Sunset=Utc;break;}
                }
                if(!Test.TestTrue(TEXT("Solar-relative continuous start exists"),Sunset.GetTicks()>0))return Finish();
                Hero->SetActorLocation(FVector(0,4000,1506.520));Hero->GetCharacterMovement()->StopMovementImmediately();
                Hero->GetCharacterMovement()->SetMovementMode(MOVE_Walking);Player->SetControlRotation(FRotator(-5,25,0));
                Viewport->SetFixedViewportSize(2560,1440);
                Frames=MakeShared<FHistoryFrames>();Frames->Origin=FPlatformTime::Seconds();GEngine->AddPerformanceDataConsumer(Frames);
                Index=0;if(!BeginStep())return Finish();return false;
            }
            if(Pending)
            {
                if(FScreenshotRequest::IsScreenshotRequested())return false;
                Pending=false;
                if(!Test.TestTrue(TEXT("Requested original image exists"),IFileManager::Get().FileExists(*(Root/(Steps[Index].Name+TEXT(".png"))))))return Finish();
                if(++Index==Steps.Num())return Finish();
                if(!BeginStep())return Finish();return false;
            }
            const auto& Step=Steps[Index];
            const bool JumpWindow=(Index>=2 && Index<=5) || (Index>=7 && Index<=10);
            const double Origin=JumpWindow && Step.Action!=3?JumpStarted:
                (Step.Action==6 || Step.Action==7 || Step.Action==8)?ContinuousStarted:StepStarted;
            const double Elapsed=FPlatformTime::Seconds()-Origin;
            if(Elapsed<Step.Seconds || GShaderCompilingManager->IsCompiling() ||
                FAssetCompilingManager::Get().GetNumRemainingAssets()>0 || IStreamingManager::Get().GetNumWantingResources()>0)return false;
            if(Step.Action==8 && !Pausing)
            {
                if(!Test.TestTrue(TEXT("Pause continuous endpoint without seeking"),Time->SetRate(0)))return Finish();
                Pausing=true;PauseFrame=GFrameCounter;return false;
            }
            if(Step.Action==8 && GFrameCounter-PauseFrame<3)return false;
            FProjectTimeSnapshot Snapshot;
            if(!Test.TestTrue(TEXT("Actual clock readable at capture"),Time->TryReadTime(Snapshot)))return Finish();
            if(JumpWindow && Step.Action!=3 && !Test.TestTrue(TEXT("Early/late checkpoint retained"),Elapsed<Step.Seconds+3))return Finish();
            FSceneViewFamilyContext Family(FSceneViewFamily::ConstructionValues(Viewport,World->Scene,GEngine->GameViewport->EngineShowFlags).SetRealtimeUpdate(true));
            FVector Camera;FRotator Rotation;
            auto* View=World->GetFirstPlayerController()->GetLocalPlayer()->CalcSceneView(&Family,Camera,Rotation,Viewport);
            if(!Test.TestNotNull(TEXT("Final consumer view resolves"),View))return Finish();
            if(!Test.TestTrue(TEXT("Matched wall camera retained"),Camera.Equals(FVector(20.845,4009.720,1579.520),1.0)))return Finish();
            const auto& Final=View->FinalPostProcessSettings;
            if(Arm==TEXT("runtime") && Index==7 && !Test.TestTrue(TEXT("Runtime recovery reaches final view"),Final.LumenSceneLightingUpdateSpeed>=4))return Finish();
            if(Arm==TEXT("runtime") && Index==10)
            {
                const auto& Post=Light->Bindings.PostProcess->Settings;
                Test.TestEqual(TEXT("Recovery expires and restores authored override"),bool(Post.bOverride_LumenSceneLightingUpdateSpeed),OriginalSceneOverride);
                Test.TestEqual(TEXT("Recovery expires and restores authored value"),Post.LumenSceneLightingUpdateSpeed,OriginalSceneSpeed);
            }
            Captures+=FString::Printf(TEXT("%s,%.3f,%s,%.3f,%.6f,%.6f,%d,%d,%.3f,%.3f,%.3f,%.3f,%.3f\n"),*Step.Name,Elapsed,*Snapshot.Utc.ToIso8601(),Snapshot.Rate,
                Light->Bindings.Sun->Intensity,Light->Bindings.NightFill->Intensity,Viewport->GetSizeXY().X,Viewport->GetSizeXY().Y,
                Final.LumenSceneLightingUpdateSpeed,Final.LumenFinalGatherLightingUpdateSpeed,Camera.X,Camera.Y,Camera.Z);
            Test.AddInfo(FString::Printf(TEXT("Capture %s elapsed=%.3f utc=%s rate=%.1f"),*Step.Name,Elapsed,*Snapshot.Utc.ToIso8601(),Snapshot.Rate));
            FScreenshotRequest::RequestScreenshot(Root/(Step.Name+TEXT(".png")),false,false);Pending=true;return false;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldDaylightHistoryTest,
    "Project.World.Realization.Presentation.DaylightHistory",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FProjectWorldDaylightHistoryTest::RunTest(const FString&)
{
    FString Root,Experience,Arm;
    if(!FParse::Value(FCommandLine::Get(),TEXT("ProjectTimeProofRoot="),Root) ||
        !FParse::Value(FCommandLine::Get(),TEXT("ProjectMenuPlayAutoExperience="),Experience) || Experience!=TEXT("ManhattanShowcase") ||
        !FParse::Value(FCommandLine::Get(),TEXT("ProjectTimeHistoryArm="),Arm) ||
        (Arm!=TEXT("primary") && Arm!=TEXT("scene") && Arm!=TEXT("temporal") && Arm!=TEXT("runtime")) || GEditor->PlayWorld ||
        !FPaths::ConvertRelativePathToFull(Root).StartsWith(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("tmp/"))))
    {AddError(TEXT("Requires explicit Manhattan demo, primary/scene/temporal/runtime diagnostic arm and scratch output root."));return false;}
    IFileManager::Get().MakeDirectory(*Root,true);
    const FString Menu=GetDefault<UGameMapsSettings>()->GetGameDefaultMap();FAutomationEditorCommonUtils::LoadMap(Menu);
    auto* Settings=NewObject<ULevelEditorPlaySettings>();Settings->SetPlayNetMode(PIE_Standalone);Settings->SetPlayNumberOfClients(1);
    Settings->SetRunUnderOneProcess(true);Settings->bLaunchSeparateServer=false;
    FRequestPlaySessionParams Request;Request.EditorPlaySettings=Settings;Request.GlobalMapOverride=Menu;Request.bAllowOnlineSubsystem=false;
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FStartHistoryPIE>(Request));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FDaylightHistoryProof>(*this,Root,Arm));return true;
}
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FProjectWorldDaylightHistoryTest,
    "Project.World.Realization.Presentation.DaylightHistory","[Slow][Integration][World][Diagnostic]")
#endif
