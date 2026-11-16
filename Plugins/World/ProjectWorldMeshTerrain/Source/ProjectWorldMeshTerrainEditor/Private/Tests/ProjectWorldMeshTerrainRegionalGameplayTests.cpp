#include "ProjectWorldMeshTerrainRegionalScene.h"
#include "ProjectWorldMeshTerrainRegionalSupport.h"
#include "ProjectWorldMeshTerrainSavedRegionScene.h"
#include "Tests/ProjectWorldConstructedRuntimePawn.h"
#include "MeshPartitionCompiledSection.h"
#include "MeshPartitionCollisionComponent.h"
#include "AssetCompilingManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationEditorCommon.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldRegionalGameplayTest
{
    using namespace ProjectWorldRegionalSceneTest;
    using namespace ProjectWorldRegionalSupportTest;
    const FString MissingOwnerError=TEXT("Actual configured Hero is not grounded on a declared saved regional owner.");
    struct FState : ProjectWorldRegionalSceneTest::FScene
    {
        FSupport Support;TArray<FRoute> Routes;ProjectWorldConstructedRuntimeTest::FPawnSelection Selection;
        TWeakObjectPtr<ACharacter> Character;bool Finished=false,ClockOwned=false,PreviousFixed=false;
        TUniquePtr<ProjectWorldSavedRegionSceneTest::FProductScene> Product;
        double PreviousDelta=0;
        explicit FState(FAutomationTestBase& Test):ProjectWorldRegionalSceneTest::FScene(Test) {}
        void RestoreClock()
        {
            if (ClockOwned) {FApp::SetFixedDeltaTime(PreviousDelta);FApp::SetUseFixedTimeStep(PreviousFixed);ClockOwned=false;}
        }
        ~FState() {RestoreClock();}
    };
    bool OwnedFloor(const FState& State,const UPrimitiveComponent* Component)
    {
        if (!Component || !Component->GetOwner()) return false;
        if (State.Product) return State.Product->Owns(Component);
        const auto* Actor=Component->GetOwner();
        if (const auto* Section=Cast<UE::MeshPartition::ACompiledSection>(Actor))
            for (const auto& Row : State.Expected->GetArrayField(TEXT("variants")))
                for (const auto& Binding : NativeBindings(*Row->AsObject()))
                    if (Section->GetBuildInfo().BuildKey.ToString(EGuidFormats::Digits)==Binding->GetStringField(TEXT("build_key"))) return true;
        if (Actor->ActorHasTag(TEXT("ProjectWorld.ConstructedSurface.v1")))
        {
            const TSharedPtr<FJsonObject>* Structures=nullptr;
            if (!State.Expected->TryGetObjectField(TEXT("regional_structures_before"),Structures)) return false;
            int32 Instance=INDEX_NONE;const FString Source=Actor->GetPackage()->GetLoadedPath().GetPackageName();
            for (const auto& Row : (*Structures)->GetArrayField(TEXT("actors")))
                if (Source==Row->AsObject()->GetStringField(TEXT("actor_package"))) return true;
        }
        return false;
    }
    bool Ready(FState& State,UWorld* World,FString& Error)
    {
        return State.Product ? State.Product->Ready(World,Error) : Census(State,World,Error);
    }
    bool Contact(const FState& State,ACharacter& Pawn,FSample& Support,double& Foot,double& Bound,FString& Error)
    {
        if (!State.Support.Sample(Pawn.GetActorLocation(),Support,Error)) return false;
        auto* Move=Pawn.GetCharacterMovement();auto* Capsule=Pawn.GetCapsuleComponent();
        if (!Move->IsMovingOnGround() || !OwnedFloor(State,Move->CurrentFloor.HitResult.GetComponent()))
        {Error=MissingOwnerError;return false;}
        FCollisionQueryParams Query(SCENE_QUERY_STAT(ProjectWorldRegionalHero),true);Query.AddIgnoredActor(&Pawn);FHitResult Hit;
        FVector P=Pawn.GetActorLocation();P.Z=Support.Height;
        if (!Pawn.GetWorld()->LineTraceSingleByChannel(Hit,P+FVector(0,0,100),P-FVector(0,0,200),ECC_Pawn,Query) ||
            !OwnedFloor(State,Hit.GetComponent()) || FMath::Abs(Hit.ImpactPoint.Z-Support.Height)>.1)
        {Error=TEXT("The live regional world ray does not match its authenticated top support.");return false;}
        FSample FloorSupport;
        const auto& FloorHit=Move->CurrentFloor.HitResult;
        if (!State.Support.Sample(FloorHit.ImpactPoint,FloorSupport,Error) || FMath::Abs(FloorSupport.Height-FloorHit.ImpactPoint.Z)>.1)
        {Error=TEXT("The actual owning floor contact is not on admitted top support.");return false;}
        const auto Owner=FloorHit.GetActor()->ActorHasTag(TEXT("ProjectWorld.ConstructedSurface.v1")) ? ESupportLayer::Structure : ESupportLayer::Ground;
        if (!State.Support.FloorEnvelope(Pawn.GetActorLocation(),Capsule->GetScaledCapsuleRadius(),UCharacterMovementComponent::MAX_FLOOR_DIST,Owner,Bound,Error)) return false;
        Foot=Pawn.GetActorLocation().Z-Capsule->GetScaledCapsuleHalfHeight()-Support.Height;
        if (!FMath::IsFinite(Foot) || Foot<-.05 || Foot>Bound)
        {Error=FString::Printf(TEXT("Actual Hero foot exceeds its native capsule/slope envelope: offset_cm=%.17g bound_cm=%.17g."),Foot,Bound);return false;}
        return true;
    }
    class FStart final : public FStartPIEForAutomationCommand
    {
        TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
    public:
        explicit FStart(const FRequestPlaySessionParams& Request):FStartPIEForAutomationCommand(Request),Settings(Request.EditorPlaySettings.Get()) {}
    };
    class FWalk final : public IAutomationLatentCommand
    {
        TSharedRef<FState> State;double Began=0,Until=0,RouteBegan=0;uint64 Last=0;
        int32 Phase=0,Route=0,Direction=1,Next=1,Frames=0,Segments=0,GroundFrames=0,BodyFrames=0;
        double MinimumFoot=MAX_dbl,MaximumFoot=-MAX_dbl,MaximumBound=0,MinimumWaterClearance=MAX_dbl;int32 WetFrames=0;
        TWeakObjectPtr<AStaticMeshActor> ForeignControl;
        bool Spawn(UWorld& World)
        {
            ACharacter* Character=nullptr;APlayerController* Controller=nullptr;
            if (!ProjectWorldConstructedRuntimeTest::SpawnPawn(&World,State->Routes[Route].Points[0],State->Selection,State->Test,Character,Controller)) return false;
            State->Character=Character;return true;
        }
    public:
        explicit FWalk(TSharedRef<FState> In):State(In) {}
        bool Update() override
        {
            auto& S=*State;if (S.Test.HasAnyErrors()) return true;
            if (Began==0) Began=FPlatformTime::Seconds();UWorld* World=GEditor ? GEditor->PlayWorld.Get() : nullptr;FString Error;
            if (!World || FPlatformTime::Seconds()-Began>600) {S.Test.AddError(TEXT("Regional Hero route exceeded its native lifecycle budget."));return true;}
            if (Phase==0)
            {
                auto* Controller=World->GetFirstPlayerController();auto* Character=Controller ? Cast<ACharacter>(Controller->GetPawn()) : nullptr;
                int32 Meshes=0;if (Character) {TArray<USkeletalMeshComponent*> Components;Character->GetComponents(Components);for (auto* C : Components) Meshes+=C->IsRegistered() && C->GetSkinnedAsset()!=nullptr;}
                if (!Character || !Meshes || FAssetCompilingManager::Get().GetNumRemainingAssets()>0 || !Ready(S,World,Error))
                {if (FPlatformTime::Seconds()-Began>90) {S.Test.AddError(Error.IsEmpty() ? TEXT("The normally configured Hero did not become ready.") : Error);return true;}return false;}
                if (!Spawn(*World)) return true;
                S.PreviousFixed=FApp::UseFixedTimeStep();S.PreviousDelta=FApp::GetFixedDeltaTime();S.ClockOwned=true;
                FApp::SetFixedDeltaTime(1./60);FApp::SetUseFixedTimeStep(true);
                for (const auto& R : S.Routes) if (R.WidthMeters*100<2*S.Character->GetCapsuleComponent()->GetScaledCapsuleRadius())
                {S.Test.AddError(TEXT("The actual Hero capsule is wider than its admitted source route."));return true;}
                if (S.Product)
                {
                    auto* Control=World->SpawnActor<AStaticMeshActor>();ForeignControl=Control;
                    auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
                    if (!Control || !Cube) {S.Test.AddError(TEXT("Cannot create the transient foreign-floor control."));return true;}
                    Control->SetFlags(RF_Transient);Control->Tags.Add(TEXT("ProjectWorld.ConstructedSurface.v1"));
                    auto* Component=Control->GetStaticMeshComponent();
                    Component->SetMobility(EComponentMobility::Movable);Component->SetStaticMesh(Cube);
                    Component->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Component->SetCollisionResponseToAllChannels(ECR_Block);
                    Control->SetActorLocation(S.Routes[0].Points[0]+FVector(0,0,50));Control->SetActorScale3D(FVector(3,3,.1));
                    S.Character->SetActorLocation(S.Routes[0].Points[0]+FVector(0,0,200),false,nullptr,ETeleportType::TeleportPhysics);
                    S.Character->GetCharacterMovement()->SetMovementMode(MOVE_Falling);Until=World->GetTimeSeconds()+1;Phase=1;return false;
                }
                FVector Outside=S.Routes[0].Points[0];for (const auto& F : S.Support.Ground) for (const auto& P : F.Points) {Outside.X=FMath::Max(Outside.X,P.X);Outside.Y=FMath::Max(Outside.Y,P.Y);}
                Outside+=FVector(500,500,300);S.Character->SetActorLocation(Outside,false,nullptr,ETeleportType::TeleportPhysics);
                S.Character->GetCharacterMovement()->SetMovementMode(MOVE_Falling);Until=World->GetTimeSeconds()+.75;Phase=1;return false;
            }
            if (Last==GFrameCounter) return false;Last=GFrameCounter;auto* Pawn=S.Character.Get();const double Now=World->GetTimeSeconds();
            if (!Pawn) {S.Test.AddError(TEXT("The normally spawned regional Hero was lost."));return true;}
            if (Now<Until) return false;
            if (Phase==1)
            {
                FSample Sample;double Foot=0,Bound=0;
                if (S.Product)
                {
                    if (!Ready(S,World,Error)) {if(FPlatformTime::Seconds()-Began>180) S.Test.AddError(Error);return false;}
                    if (!S.Test.TestTrue(TEXT("A real foreign first blocker cannot qualify as saved product support."),
                        Pawn->GetCharacterMovement()->IsMovingOnGround() &&
                        Pawn->GetCharacterMovement()->CurrentFloor.HitResult.GetActor()==ForeignControl.Get() &&
                        !Contact(S,*Pawn,Sample,Foot,Bound,Error) && Error==MissingOwnerError)) return true;
                    S.Receipt->SetStringField(TEXT("foreign_floor_control_refusal"),Error);
                    ForeignControl->Destroy();ForeignControl.Reset();
                    S.Receipt->SetBoolField(TEXT("foreign_floor_actual_hero_control_passed"),true);
                }
                else
                {
                    if (!S.Test.TestTrue(TEXT("An actual unsupported Hero position falls and refuses the same contact checker."),
                        Pawn->GetCharacterMovement()->IsFalling() && !Contact(S,*Pawn,Sample,Foot,Bound,Error))) return true;
                    S.Receipt->SetBoolField(TEXT("unsupported_actual_hero_control_passed"),true);
                }
                if (!Spawn(*World)) return true;Until=Now+1.;RouteBegan=Until;Phase=2;return false;
            }
            FSample Sample;double Foot=0,Bound=0;
            if (!Contact(S,*Pawn,Sample,Foot,Bound,Error)) {S.Test.AddError(Error);return true;}
            ++Frames;GroundFrames+=!Sample.Structure;BodyFrames+=Sample.Structure;
            if (Sample.Wet) {++WetFrames;MinimumWaterClearance=FMath::Min(MinimumWaterClearance,Sample.Height-Sample.WaterHeight);}
            MinimumFoot=FMath::Min(MinimumFoot,Foot);MaximumFoot=FMath::Max(MaximumFoot,Foot);MaximumBound=FMath::Max(MaximumBound,Bound);
            const auto& Points=S.Routes[Route].Points;const FVector Target=Points[Direction>0 ? Next : Points.Num()-1-Next];
            FVector Delta=Target-Pawn->GetActorLocation();Delta.Z=0;
            if (Delta.Length()<15)
            {
                ++Segments;++Next;
                if (Next==Points.Num())
                {
                    Pawn->GetCharacterMovement()->StopMovementImmediately();
                    if (Direction>0) {Direction=-1;Next=1;}
                    else if (++Route<S.Routes.Num()) {Direction=1;Next=1;if (!Spawn(*World)) return true;Until=Now+1.;RouteBegan=Until;}
                    else
                    {
                        if (!Ready(S,World,Error)) {S.Test.AddError(Error);return true;}
                        S.Finished=true;S.Receipt->SetNumberField(TEXT("walking_frames"),Frames);S.Receipt->SetNumberField(TEXT("completed_bidirectional_routes"),S.Routes.Num());
                        S.Receipt->SetNumberField(TEXT("walking_segments"),Segments);S.Receipt->SetNumberField(TEXT("ground_supported_frames"),GroundFrames);S.Receipt->SetNumberField(TEXT("structure_supported_frames"),BodyFrames);
                        S.Receipt->SetNumberField(TEXT("frames_over_actual_water_footprint"),WetFrames);
                        if (WetFrames) S.Receipt->SetNumberField(TEXT("minimum_actual_support_above_water_m"),MinimumWaterClearance*.01);
                        S.Receipt->SetNumberField(TEXT("minimum_foot_offset_cm"),MinimumFoot);S.Receipt->SetNumberField(TEXT("maximum_foot_offset_cm"),MaximumFoot);S.Receipt->SetNumberField(TEXT("maximum_native_floor_envelope_cm"),MaximumBound);
                        S.Receipt->SetNumberField(TEXT("actual_capsule_radius_cm"),Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius());S.Receipt->SetNumberField(TEXT("actual_capsule_half_height_cm"),Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
                        S.Receipt->SetStringField(TEXT("actual_hero_class"),Pawn->GetClass()->GetPathName());S.Receipt->SetStringField(TEXT("expected_definition_id"),S.Selection.Definition.ToString());return true;
                    }
                }
            }
            else Pawn->AddMovementInput(Delta.GetSafeNormal(),1,true);
            if (Now-RouteBegan>480) {S.Test.AddError(TEXT("A regional Hero route made no bounded terminal progress."));return true;}
            return false;
        }
    };
    class FCleanup final : public IAutomationLatentCommand
    {
        TSharedRef<FState> State;double Began=0;
    public:
        explicit FCleanup(TSharedRef<FState> In):State(In) {}
        bool Update() override
        {
            if (Began==0) Began=FPlatformTime::Seconds();
            if (GEditor && GEditor->PlayWorld && FPlatformTime::Seconds()-Began<30) return false;
            State->RestoreClock();FString Error;const bool Clean=State->Finalizable(Error);
            State->Test.TestTrue(TEXT("Configured-Hero gameplay ends PIE with every saved scene byte unchanged."),Clean);
            if (!Clean) State->Test.AddError(Error);
            State->Receipt->SetStringField(TEXT("status"),Clean && State->Finished && !State->Test.HasAnyErrors() ? TEXT("accepted") : TEXT("failed_or_incomplete"));
            State->Receipt->SetStringField(TEXT("scope"),TEXT("Actual normally configured Hero, continuous CMC walking both directions on declared diagnostic routes of the unchanged saved scene; route origins remain in the pinned input. No city spawn, full-way, input-device, navigation, streaming or geographic admission."));
            if(State->Product) State->Receipt->SetStringField(TEXT("scope"),TEXT("Actual configured Hero walking declared reference routes in the unchanged selected saved product map. Current manifests own every floor; local canonical fields are reference expectations. No geographic adoption or packaged/performance acceptance."));
            FString Text;FJsonSerializer::Serialize(State->Receipt,TJsonWriterFactory<>::Create(&Text));
            State->Test.TestTrue(TEXT("Regional Hero evidence survives success and refusal."),FFileHelper::SaveStringToFile(Text,*(State->Root/TEXT("gameplay.json"))));return true;
        }
    };
}
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FProjectWorldRegionalGameplayTest,"Project.World.Realization.MeshTerrain.CanonicalRegionGameplay",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
void FProjectWorldRegionalGameplayTest::GetTests(TArray<FString>& Names,TArray<FString>& Parameters) const
{
    FString Value;const TCHAR* Command=FCommandLine::Get();
    if (FParse::Value(Command,TEXT("ProjectWorldRegionalGameplay="),Value) ||
        FParse::Value(Command,TEXT("ProjectWorldRegionalGameplayHash="),Value) ||
        FParse::Param(Command,TEXT("ProjectWorldRegionalGameplay")) || FParse::Param(Command,TEXT("ProjectWorldRegionalGameplayHash")) ||
        FParse::Value(Command,TEXT("ProjectWorldSavedRegionGameplay="),Value) || FParse::Value(Command,TEXT("ProjectWorldSavedRegionGameplayHash="),Value) ||
        FParse::Param(Command,TEXT("ProjectWorldSavedRegionGameplay")) || FParse::Param(Command,TEXT("ProjectWorldSavedRegionGameplayHash")))
    {Names.Add(TEXT(""));Parameters.Add(TEXT(""));}
}
bool FProjectWorldRegionalGameplayTest::RunTest(const FString&)
{
    using namespace ProjectWorldRegionalGameplayTest;FString Path,Hash,Actual,Error;
    if (!FParse::Value(FCommandLine::Get(),TEXT("ProjectWorldRegionalGameplay="),Path) || !FParse::Value(FCommandLine::Get(),TEXT("ProjectWorldRegionalGameplayHash="),Hash) ||
        !FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(Path),FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("tmp/world"))) ||
        !FProjectWorldCanonicalLoader::ComputeFileSha256(Path,Actual) || Actual!=Hash)
    {AddError(TEXT("Supply a pinned confined regional Hero route input."));return false;}
    TSharedPtr<FJsonObject> Input;if (!ReadObject(Path,Input)) {AddError(TEXT("Cannot parse regional Hero input."));return false;}
    for (const TCHAR* Key : {TEXT("scene_input"),TEXT("route_source")})
    {
        const TSharedPtr<FJsonObject>* Ref=nullptr;
        if (!Input->TryGetObjectField(Key,Ref) || !Ref || !(*Ref)->HasTypedField<EJson::String>(TEXT("path")) ||
            !(*Ref)->HasTypedField<EJson::String>(TEXT("sha256"))) {AddError(TEXT("Regional Hero inputs need explicit scene and route pins."));return false;}
    }
    auto State=MakeShared<FState>(*this);
    const auto Source=Input->GetObjectField(TEXT("scene_input"));
    FString ProductPath,ProductHash;const TCHAR* Command=FCommandLine::Get();
    const bool HasProduct=FParse::Value(Command,TEXT("ProjectWorldSavedRegionGameplay="),ProductPath);
    const bool HasProductHash=FParse::Value(Command,TEXT("ProjectWorldSavedRegionGameplayHash="),ProductHash);
    if(HasProduct || HasProductHash || FParse::Param(Command,TEXT("ProjectWorldSavedRegionGameplay")) || FParse::Param(Command,TEXT("ProjectWorldSavedRegionGameplayHash")))
    {
        if(!HasProduct || !HasProductHash || !ReadReference(*State,FPaths::ProjectDir()/Source->GetStringField(TEXT("path")),Source->GetStringField(TEXT("sha256"))))
        {AddError(TEXT("Saved product walking requires both proof pins and an authenticated regional reference."));return false;}
        State->Product=MakeUnique<ProjectWorldSavedRegionSceneTest::FProductScene>();
        if(!State->Product->Read(*State,ProductPath,ProductHash,Hash,Error)) {AddError(Error);return false;}
    }
    else if (!Read(*State,FPaths::ProjectDir()/Source->GetStringField(TEXT("path")),Source->GetStringField(TEXT("sha256")))) return false;
    State->Root=FPaths::GetPath(Path);State->SourceFiles.Add(FPaths::ConvertRelativePathToFull(Path),Hash);
    State->Receipt->SetStringField(TEXT("scene_input_sha256"),Source->GetStringField(TEXT("sha256")));
    State->Receipt->SetStringField(TEXT("input_sha256"),Hash);
    const auto Route=Input->GetObjectField(TEXT("route_source"));State->SourceFiles.Add(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/Route->GetStringField(TEXT("path"))),Route->GetStringField(TEXT("sha256")));
    if (!State->Unchanged(Error) || !State->Support.Read(*State,Error) || !ReadRoutes(*Input,State->Routes,Error)) {AddError(Error);return false;}
    if (!ProjectWorldConstructedRuntimeTest::ConfiguredPawnRequested() || !ProjectWorldConstructedRuntimeTest::ReadPawnSelection(*this,State->Selection) ||
        !State->Selection.GameModeClass || !State->Selection.PawnClass || !State->Selection.Definition.IsValid())
    {AddError(TEXT("Regional gameplay requires the actual configured GameMode, Hero class and definition; generic pawn fallback is forbidden."));return false;}
    auto* Settings=NewObject<ULevelEditorPlaySettings>(GetTransientPackage());Settings->SetPlayNetMode(PIE_Standalone);Settings->SetPlayNumberOfClients(1);Settings->SetRunUnderOneProcess(true);Settings->bLaunchSeparateServer=false;
    FRequestPlaySessionParams Request;Request.EditorPlaySettings=Settings;Request.bAllowOnlineSubsystem=false;Request.GameModeOverride=State->Selection.GameModeClass;
    Request.StartLocation=State->Routes[0].Points[0]+FVector(0,0,150);
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FStart>(Request));FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FWalk>(State));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FEndPlayMapCommand>());FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FCleanup>(State));return true;
}
#endif
