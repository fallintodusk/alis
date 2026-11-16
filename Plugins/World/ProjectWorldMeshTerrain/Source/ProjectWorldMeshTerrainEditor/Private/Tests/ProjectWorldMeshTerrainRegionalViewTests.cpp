// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "Tests/ProjectWorldRegionalEvidence.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldMeshTerrainCanonicalFixture.h"
#include "ProjectWorldMeshTerrainRegionalScene.h"
#include "ProjectWorldMeshTerrainRegionDomain.h"
#include "ProjectWorldMeshTerrainBasinScene.h"
#include "MeshPartitionCompiledSection.h"
#include "MeshPartitionPreviewSection.h"
#include "MeshPartitionDefinition.h"
#include "MeshPartitionStaticMeshComponent.h"
#include "MeshPartitionEditorUtils.h"
#include "AssetCompilingManager.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "PrimitiveSceneProxy.h"
#include "Serialization/JsonSerializer.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationEditorCommon.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldRegionalViewTest
{
    using namespace UE::MeshPartition;
    using namespace ProjectWorldRegionalSceneTest;
    struct FState : ProjectWorldRegionalSceneTest::FScene
    {
        bool Configured=false,Captured=false;uint64 LastFrame=0;int32 Settled=0;double Started=0;
        explicit FState(FAutomationTestBase& In):ProjectWorldRegionalSceneTest::FScene(In) {}
    };
    bool Controls(FState& State,UWorld& World)
    {
        FString Error;auto& Test=State.Test;int32 Actors=0;
        for (TActorIterator<AActor> It(&World);It;++It) ++Actors;
        Test.TestFalse(TEXT("A stale presentation profile refuses before actor mutation."),ProjectWorldRegionalEvidence::Configure(
            World,State.Bundle,State.Profile,FString::ChrN(64,'f'),*State.Receipt,Error));
        int32 After=0;for (TActorIterator<AActor> It(&World);It;++It) ++After;
        Test.TestEqual(TEXT("Refused presentation preserves the actual actor census."),After,Actors);
        Test.TestFalse(TEXT("An actual editor preview cannot pass the runtime census."),Census(State,State.EditorWorld.Get(),Error));
        Test.TestFalse(TEXT("The actual compiled world refuses a wrong variant requirement."),Census(State,&World,Error,TEXT("invalid_variant")));
        Test.TestFalse(TEXT("Cleanup cannot be accepted while its actual PIE world remains active."),State.Finalizable(Error));
        TArray<TPair<UStaticMeshComponent*,bool>> Hidden;
        for (TActorIterator<ACompiledSection> It(&World);It;++It) for (const auto& Component : It->GetMeshComponents())
        {Hidden.Add({Component.Get(),Component->IsVisible()});Component->SetVisibility(false);}
        Test.TestFalse(TEXT("No drawn native components must refuse even with all saved actors present."),Census(State,&World,Error));
        for (const auto& Pair : Hidden) Pair.Key->SetVisibility(Pair.Value);
        if (Hidden.IsEmpty()) return false;
        auto* Component=Hidden[0].Key;auto* Original=Component->GetMaterial(0);
        Component->SetMaterial(0,UMaterial::GetDefaultMaterial(MD_Surface));
        Test.TestFalse(TEXT("A substituted actual Terrain material refuses."),Census(State,&World,Error));Component->SetMaterial(0,Original);
        for (TActorIterator<AStaticMeshActor> It(&World);It;++It) if (It->ActorHasTag(TEXT("ProjectWorld.Water.v1")))
        {
            const bool WasHidden=It->IsHidden();It->SetActorHiddenInGame(true);
            Test.TestFalse(TEXT("Hidden ordinary Water cannot pass a nonempty saved inventory."),Census(State,&World,Error));
            It->SetActorHiddenInGame(WasHidden);break;
        }
        auto Water=State.Expected->GetObjectField(TEXT("regional_water_before"));
        auto Bindings=Water->GetArrayField(TEXT("actors"));
        if (Bindings.Num()>1)
        {
            auto Duplicated=Bindings;Duplicated[1]=Duplicated[0];Water->SetArrayField(TEXT("actors"),Duplicated);
            Test.TestFalse(TEXT("A duplicated Water binding with another actual actor omitted refuses an unchanged two-actor world."),Census(State,&World,Error));
            Water->SetArrayField(TEXT("actors"),Bindings);
        }
        for (TActorIterator<AStaticMeshActor> It(&World);It;++It) if (It->ActorHasTag(TEXT("ProjectWorld.ConstructedSurface.v1")))
        {
            const bool WasHidden=It->IsHidden();It->SetActorHiddenInGame(true);
            Test.TestFalse(TEXT("A hidden admitted fragment refuses combined appearance."),Census(State,&World,Error));
            It->SetActorHiddenInGame(WasHidden);break;
        }
        Test.TestFalse(TEXT("A stale camera plan refuses before any capture directory is created."),ProjectWorldRegionalEvidence::Capture(
            World,State.Plan,FString::ChrN(64,'f'),State.Root/TEXT("rejected_images"),*State.Receipt,Error));
        Test.TestTrue(TEXT("Known-bad controls leave every saved dependency unchanged."),State.Unchanged(Error));
        State.Receipt->SetBoolField(TEXT("refusal_controls_passed"),!Test.HasAnyErrors());return !Test.HasAnyErrors();
    }
    class FStart final : public FStartPIEForAutomationCommand
    {
        TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
    public:
        explicit FStart(const FRequestPlaySessionParams& Request):FStartPIEForAutomationCommand(Request),Settings(Request.EditorPlaySettings.Get()) {}
    };
    class FObserve final : public IAutomationLatentCommand
    {
        TSharedRef<FState> State;bool Tested=false;
    public:
        explicit FObserve(TSharedRef<FState> In):State(In) {}
        bool Update() override
        {
            auto& S=*State;if (S.Test.HasAnyErrors()) return true;
            if (S.Started==0) S.Started=FPlatformTime::Seconds();
            UWorld* World=GEditor ? GEditor->PlayWorld.Get() : nullptr;FString Error;
            if (!World || FPlatformTime::Seconds()-S.Started>90)
            {S.Test.AddError(TEXT("Saved regional PIE is absent or exceeded its readiness budget."));return true;}
            if (!S.Configured)
            {
                if (auto* Controller=World->GetFirstPlayerController()) if (APawn* Pawn=Controller->GetPawn())
                {
                    if (!Pawn->IsA<ADefaultPawn>()) {S.Test.AddError(TEXT("The regional observer must not replace or hide a gameplay character."));return true;}
                    Pawn->SetActorHiddenInGame(true);
                }
                if (!ProjectWorldRegionalEvidence::Configure(*World,S.Bundle,S.Profile,S.ProfileHash,*S.Receipt,Error))
                {S.Test.AddError(Error);return true;}
                S.Configured=true;
            }
            if (S.LastFrame==GFrameCounter) return false;S.LastFrame=GFrameCounter;
            if (FAssetCompilingManager::Get().GetNumRemainingAssets()>0 || !Census(S,World,Error))
            {S.Settled=0;if (FPlatformTime::Seconds()-S.Started>85) {S.Test.AddError(Error.IsEmpty() ? TEXT("Regional assets did not settle.") : Error);return true;}return false;}
            if (++S.Settled<3) return false;
            if (!Tested) {Tested=true;if (!Controls(S,*World)) return true;S.Settled=0;World->SendAllEndOfFrameUpdates();return false;}
            if (!ProjectWorldRegionalEvidence::Capture(*World,S.Plan,S.PlanHash,S.Root/TEXT("images"),*S.Receipt,Error))
            {S.Test.AddError(Error);return true;}
            S.Captured=true;S.Receipt->SetNumberField(TEXT("settled_distinct_frames"),S.Settled);return true;
        }
    };
    class FCleanup final : public IAutomationLatentCommand
    {
        TSharedRef<FState> State;double Started=0;
    public:
        explicit FCleanup(TSharedRef<FState> In):State(In) {}
        bool Update() override
        {
            if (Started==0) Started=FPlatformTime::Seconds();
            if (GEditor && GEditor->PlayWorld && FPlatformTime::Seconds()-Started<30) return false;
            FString Error;const bool Clean=State->Finalizable(Error);
            State->Test.TestTrue(TEXT("Native PIE shutdown leaves every saved input byte-identical."),Clean);
            if (!Clean) State->Test.AddError(Error);
            State->Receipt->SetStringField(TEXT("status"),Clean && State->Captured && !State->Test.HasAnyErrors() ? TEXT("accepted") : TEXT("failed_or_incomplete"));
            State->Receipt->SetStringField(TEXT("scope"),TEXT("Saved regional compiled Terrain and ordinary Water, actual-profile settled PIE appearance. No Hero, navigation, streaming or geographic admission."));
            FString Text;FJsonSerializer::Serialize(State->Receipt,TJsonWriterFactory<>::Create(&Text));
            State->Test.TestTrue(TEXT("Runtime appearance receipt survives success and refusal."),FFileHelper::SaveStringToFile(Text,*(State->Root/TEXT("runtime-view.json"))));
            return true;
        }
    };
}
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FProjectWorldRegionalViewTest,
    "Project.World.Realization.MeshTerrain.CanonicalRegionView",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
void FProjectWorldRegionalViewTest::GetTests(TArray<FString>& Names,TArray<FString>& Parameters) const
{
    FString Value;const TCHAR* Command=FCommandLine::Get();
    if (FParse::Value(Command,TEXT("ProjectWorldRegionalView="),Value) ||
        FParse::Value(Command,TEXT("ProjectWorldRegionalViewHash="),Value) ||
        FParse::Param(Command,TEXT("ProjectWorldRegionalView")) || FParse::Param(Command,TEXT("ProjectWorldRegionalViewHash")))
    {Names.Add(TEXT(""));Parameters.Add(TEXT(""));}
}
bool FProjectWorldRegionalViewTest::RunTest(const FString&)
{
    using namespace ProjectWorldRegionalViewTest;
    if (!ProjectWorldRegionDomain::VerifyInputContracts(*this)) return false;
    FString Path,Hash;
    if (!FParse::Value(FCommandLine::Get(),TEXT("ProjectWorldRegionalView="),Path) ||
        !FParse::Value(FCommandLine::Get(),TEXT("ProjectWorldRegionalViewHash="),Hash))
    {AddError(TEXT("Supply a pinned confined saved-regional-view input."));return false;}
    auto State=MakeShared<FState>(*this);
    if (!ProjectWorldRegionalSceneTest::Read(*State,Path,Hash)) return false;
    const auto& Start=State->Input->GetArrayField(TEXT("start"));
    auto* Settings=NewObject<ULevelEditorPlaySettings>(GetTransientPackage());
    Settings->SetPlayNetMode(PIE_Standalone);Settings->SetPlayNumberOfClients(1);Settings->SetRunUnderOneProcess(true);Settings->bLaunchSeparateServer=false;
    FRequestPlaySessionParams Request;Request.EditorPlaySettings=Settings;Request.bAllowOnlineSubsystem=false;
    Request.GameModeOverride=AGameModeBase::StaticClass();
    Request.StartLocation=FVector(Start[0]->AsNumber(),Start[1]->AsNumber(),Start[2]->AsNumber());
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FStart>(Request));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FObserve>(State));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FEndPlayMapCommand>());
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FCleanup>(State));return true;
}
#endif
