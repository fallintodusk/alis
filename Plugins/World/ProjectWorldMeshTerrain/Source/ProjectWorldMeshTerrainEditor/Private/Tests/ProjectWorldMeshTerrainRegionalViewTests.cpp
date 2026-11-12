// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "Tests/ProjectWorldRegionalEvidence.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldMeshTerrainCanonicalFixture.h"
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
    struct FState
    {
        FAutomationTestBase& Test;FString Root,Map,Profile,ProfileHash,Plan,PlanHash;
        FProjectWorldCanonicalBundle Bundle;TSharedPtr<FJsonObject> Input,Expected;
        TSharedRef<FJsonObject> Receipt=MakeShared<FJsonObject>();
        TMap<FString,FString> SourceFiles;TArray<FString> Packages;TWeakObjectPtr<UWorld> EditorWorld;
        bool Configured=false,Captured=false;uint64 LastFrame=0;int32 Settled=0;double Started=0;
        explicit FState(FAutomationTestBase& In):Test(In) {}
        bool Unchanged(FString& Error) const
        {
            for (const auto& Pair : SourceFiles)
            {
                FString Hash;
                if (!FProjectWorldCanonicalLoader::ComputeFileSha256(Pair.Key,Hash) || Hash!=Pair.Value)
                {Error=TEXT("Regional observation changed or lost a declared saved input.");return false;}
            }
            return true;
        }
        bool Finalizable(FString& Error) const
        {
            if (GEditor && GEditor->PlayWorld)
            {Error=TEXT("An active PIE world cannot be accepted as cleaned up.");return false;}
            return Unchanged(Error);
        }
    };
    bool ReadObject(const FString& Path,TSharedPtr<FJsonObject>& Out)
    {
        FString Text;return FFileHelper::LoadFileToString(Text,*Path) &&
            FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Out) && Out.IsValid();
    }
    TArray<TSharedPtr<FJsonObject>> NativeBindings(const FJsonObject& Row)
    {
        TArray<TSharedPtr<FJsonObject>> Out;const TArray<TSharedPtr<FJsonValue>>* Values=nullptr;
        if (Row.TryGetArrayField(TEXT("native_sections"),Values))
        {for (const auto& Value : *Values) Out.Add(Value->AsObject());}
        else
        {
            auto One=MakeShared<FJsonObject>();One->SetStringField(TEXT("build_key"),Row.GetStringField(TEXT("native_build_key")));
            One->SetStringField(TEXT("actor_package"),Row.GetStringField(TEXT("native_actor_package")));Out.Add(One);
        }
        return Out;
    }
    bool Census(FState& State,UWorld* World,FString& Error,FName RequiredVariant=NAME_None)
    {
        Error.Reset();int32 Instance=INDEX_NONE;
        if (!World || World->WorldType!=EWorldType::PIE ||
            UWorld::RemovePIEPrefix(World->GetOutermost()->GetName(),&Instance)!=State.Map || Instance<0)
        {Error=TEXT("Runtime census refuses editor previews or a different saved map.");return false;}
        for (TActorIterator<APreviewSection> It(World);It;++It) if (!It->IsHidden())
        {Error=TEXT("Runtime evidence refuses a co-resident visible editor preview section.");return false;}
        int32 Sections=0,Drawn=0,Water=0,Structures=0;TSet<FName> ExpectedVariants,Found;TSet<FString> Consumed;
        ProjectWorldBasinSceneTest::FInput Ownership;Ownership.PackageName=State.Map;Ownership.Packages=State.Packages;
        for (TActorIterator<ACompiledSection> It(World);It;++It)
        {
            ++Sections;const auto& Info=It->GetBuildInfo();auto* Definition=Info.GetMegaMeshDefinition();
            if (!Definition || It->IsPlaceholder() || !Definition->GetMaterial() ||
                Definition->GetPathName()!=State.Expected->GetStringField(TEXT("expected_definition_object_path")) ||
                Definition->GetMaterial()->GetPathName()!=State.Expected->GetStringField(TEXT("accepted_material")))
            {Error=TEXT("Runtime section lost its accepted definition/material or is a placeholder.");return false;}
            if (ExpectedVariants.IsEmpty())
            {
                FName Preview;auto* Platform=UE::MeshPartition::EditorUtils::GetPreviewPlatform(Preview);
                ExpectedVariants.Append(Definition->GetCompiledSectionBuildVariantNamesForCurrentEditorPreview(Preview,Platform));
            }
            if (!ExpectedVariants.Contains(Info.BuildVariantName) ||
                (RequiredVariant!=NAME_None && Info.BuildVariantName!=TEXT("collision") && Info.BuildVariantName!=RequiredVariant))
            {Error=TEXT("Runtime census refuses a different selected native variant.");return false;}
            TSharedPtr<FJsonObject> Match;
            for (const auto& Row : State.Expected->GetArrayField(TEXT("variants")))
                if (Row->AsObject()->GetStringField(TEXT("variant"))==Info.BuildVariantName.ToString()) Match=Row->AsObject();
            TSharedPtr<FJsonObject> Binding;
            if (Match) for (const auto& Entry : NativeBindings(*Match))
                if (Info.BuildKey.ToString(EGuidFormats::Digits)==Entry->GetStringField(TEXT("build_key")) &&
                    ProjectWorldBasinSceneTest::OwnsSavedActor(*It,World,Ownership,Entry->GetStringField(TEXT("actor_package")))) Binding=Entry;
            if (!Binding || Consumed.Contains(Binding->GetStringField(TEXT("actor_package"))))
            {Error=TEXT("Runtime section is not the declared saved native build.");return false;}
            Consumed.Add(Binding->GetStringField(TEXT("actor_package")));
            if (!It->GetActorTransform().Equals(FTransform::Identity,0))
            {Error=TEXT("Runtime section changed its saved identity frame.");return false;}
            Found.Add(Info.BuildVariantName);
            for (const auto& Component : It->GetMeshComponents())
            {
                auto* Material=Component->GetMaterial(0);const auto* InstanceMaterial=Cast<UMaterialInstance>(Material);
                if (Material) Material->EnsureIsComplete();
                const auto* Resource=Material ? Material->GetMaterialResource(GetFeatureLevelShaderPlatform_Checked(World->GetFeatureLevel())) : nullptr;
                const bool AcceptedMaterial=Material==Definition->GetMaterial() || (InstanceMaterial && InstanceMaterial->IsChildOf(Definition->GetMaterial()));
                auto Observation=MakeShared<FJsonObject>();
                Observation->SetStringField(TEXT("variant"),Info.BuildVariantName.ToString());
                Observation->SetStringField(TEXT("material"),GetPathNameSafe(Material));
                Observation->SetStringField(TEXT("expected_material"),GetPathNameSafe(Definition->GetMaterial()));
                Observation->SetBoolField(TEXT("channel_texture"),It->GetChannelTexture()!=nullptr);
                Observation->SetNumberField(TEXT("channel_table_bytes"),It->GetChannelTable().Num());
                Observation->SetBoolField(TEXT("actor_hidden"),It->IsHidden());
                Observation->SetBoolField(TEXT("registered"),Component->IsRegistered());
                Observation->SetBoolField(TEXT("visible"),Component->IsVisible());
                Observation->SetBoolField(TEXT("hidden_in_game"),Component->bHiddenInGame);
                Observation->SetBoolField(TEXT("main_pass"),Component->bRenderInMainPass);
                Observation->SetBoolField(TEXT("scene_proxy"),Component->GetSceneProxy()!=nullptr);
                Observation->SetBoolField(TEXT("nanite_proxy"),Component->GetSceneProxy() && Component->GetSceneProxy()->IsNaniteMesh());
                Observation->SetBoolField(TEXT("nanite_data"),Component->GetStaticMesh() && Component->GetStaticMesh()->HasValidNaniteData());
                Observation->SetBoolField(TEXT("force_disable_nanite"),Component->IsForceDisableNanite());
                Observation->SetBoolField(TEXT("accepted_material"),AcceptedMaterial);
                Observation->SetBoolField(TEXT("shader_complete"),Resource && Resource->IsGameThreadShaderMapComplete());
                State.Receipt->SetObjectField(TEXT("last_component_observation"),Observation);
                if (!It->GetChannelTexture() || It->GetChannelTable().IsEmpty() || It->IsHidden() ||
                    !Component->IsRegistered() || !Component->IsVisible() || Component->bHiddenInGame || !Component->bRenderInMainPass || !Component->GetSceneProxy() ||
                    !Component->GetSceneProxy()->IsNaniteMesh() || !Component->GetStaticMesh() ||
                    !Component->GetStaticMesh()->HasValidNaniteData() || Component->IsForceDisableNanite() ||
                    !AcceptedMaterial ||
                    !Resource || !Resource->IsGameThreadShaderMapComplete())
                {FString Detail;FJsonSerializer::Serialize(Observation,TJsonWriterFactory<>::Create(&Detail));
                    Error=TEXT("Runtime compiled-component readiness refuses: ")+Detail;return false;}
                ++Drawn;
            }
        }
        int32 RequiredSections=0;
        for (const auto& Row : State.Expected->GetArrayField(TEXT("variants")))
            if (ExpectedVariants.Contains(FName(*Row->AsObject()->GetStringField(TEXT("variant"))))) RequiredSections+=NativeBindings(*Row->AsObject()).Num();
        if (Sections!=RequiredSections) {Error=TEXT("Runtime observation is missing a declared selected-variant section.");return false;}
        const auto& ExpectedWater=State.Expected->GetObjectField(TEXT("regional_water_before"))->GetArrayField(TEXT("actors"));
        TArray<FString> SeenWaterPackages;
        for (TActorIterator<AStaticMeshActor> It(World);It;++It)
        {
            if (!It->ActorHasTag(TEXT("ProjectWorld.Water.v1"))) continue;
            auto* Component=It->GetStaticMeshComponent();UStaticMesh* Mesh=Component->GetStaticMesh();auto* Material=Component->GetMaterial(0);
            FString OwnedPackage;
            for (const auto& Row : ExpectedWater)
            {
                const auto E=Row->AsObject();
                if (Mesh && Material && Mesh->GetPathName()==E->GetStringField(TEXT("mesh")) &&
                    Material->GetPathName()==E->GetStringField(TEXT("material")) &&
                    ProjectWorldBasinSceneTest::OwnsSavedActor(*It,World,Ownership,E->GetStringField(TEXT("actor_package")))) OwnedPackage=E->GetStringField(TEXT("actor_package"));
            }
            if (OwnedPackage.IsEmpty() || It->IsHidden() || !Component->IsVisible() || !Component->GetSceneProxy() ||
                Component->GetCollisionEnabled()!=ECollisionEnabled::NoCollision)
            {Error=TEXT("Runtime Water lost its saved ownership, material, visibility or nonphysical role.");return false;}
            SeenWaterPackages.Add(OwnedPackage);++Water;
        }
        TArray<FString> RequiredWaterPackages;
        for (const auto& Row : ExpectedWater) RequiredWaterPackages.Add(Row->AsObject()->GetStringField(TEXT("actor_package")));
        if (!ProjectWorldRegionDomain::ExactWaterBindings(RequiredWaterPackages,SeenWaterPackages))
        {Error=TEXT("Runtime Water bindings are duplicated or incomplete.");return false;}
        const TSharedPtr<FJsonObject>* Structural=nullptr;
        if (State.Expected->TryGetObjectField(TEXT("regional_structures_before"),Structural))
        {
            const auto& Expected=(*Structural)->GetArrayField(TEXT("actors"));TSet<FString> Seen;
            for (TActorIterator<AStaticMeshActor> It(World);It;++It) if (It->ActorHasTag(TEXT("ProjectWorld.ConstructedSurface.v1")))
            {
                auto* C=It->GetStaticMeshComponent();UStaticMesh* Mesh=C->GetStaticMesh();auto* Material=C->GetMaterial(0);FString Package;
                if (Material) Material->EnsureIsComplete();
                const auto* Resource=Material ? Material->GetMaterialResource(GetFeatureLevelShaderPlatform_Checked(World->GetFeatureLevel())) : nullptr;
                for (const auto& Row : Expected)
                {
                    const auto E=Row->AsObject();
                    if (Mesh && Material && Mesh->GetPathName()==E->GetStringField(TEXT("mesh")) && Material->GetPathName()==E->GetStringField(TEXT("material")) &&
                        State.Bundle.Features.Contains(E->GetStringField(TEXT("feature_id"))) &&
                        ProjectWorldBasinSceneTest::OwnsSavedActor(*It,World,Ownership,E->GetStringField(TEXT("actor_package")))) Package=E->GetStringField(TEXT("actor_package"));
                }
                if (Package.IsEmpty() || Seen.Contains(Package) || It->IsHidden() || !C->IsRegistered() || !C->IsVisible() || C->bHiddenInGame ||
                    !C->bRenderInMainPass || !C->GetSceneProxy() || !C->GetSceneProxy()->IsNaniteMesh() || !Mesh->HasValidNaniteData() ||
                    !Resource || !Resource->IsGameThreadShaderMapComplete() ||
                    C->GetCollisionEnabled()!=ECollisionEnabled::QueryAndPhysics || C->GetCollisionResponseToChannel(ECC_Pawn)!=ECR_Block)
                {Error=TEXT("Runtime admitted structure lost its saved identity, visible geometry or physical role.");return false;}
                Seen.Add(Package);++Structures;
            }
            if (Structures!=Expected.Num() || Structures!=2) {Error=TEXT("Runtime combined scene is missing a complete admitted structural fragment.");return false;}
        }
        else for (TActorIterator<AStaticMeshActor> It(World);It;++It) if (It->ActorHasTag(TEXT("ProjectWorld.ConstructedSurface.v1")))
        {Error=TEXT("Runtime scene contains an undeclared constructed actor.");return false;}
        if (Sections==0 || Drawn==0 || Water!=ExpectedWater.Num() || Water==0 || Found.Num()!=ExpectedVariants.Num())
        {Error=TEXT("Runtime census has missing compiled Terrain or ordinary Water.");return false;}
        State.Receipt->SetNumberField(TEXT("compiled_sections"),Sections);
        State.Receipt->SetNumberField(TEXT("drawn_native_components"),Drawn);
        State.Receipt->SetNumberField(TEXT("ordinary_water_actors"),Water);
        State.Receipt->SetNumberField(TEXT("admitted_constructed_actors"),Structures);
        State.Receipt->SetStringField(TEXT("world_type"),TEXT("PIE"));return true;
    }
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldRegionalViewTest,
    "Project.World.Realization.MeshTerrain.CanonicalRegionView",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldRegionalViewTest::RunTest(const FString&)
{
    using namespace ProjectWorldRegionalViewTest;
    if (!ProjectWorldRegionDomain::VerifyInputContracts(*this)) return false;
    FString Path,Hash,Actual,Error;
    if (!FParse::Value(FCommandLine::Get(),TEXT("ProjectWorldRegionalView="),Path) ||
        !FParse::Value(FCommandLine::Get(),TEXT("ProjectWorldRegionalViewHash="),Hash) ||
        !FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(Path),FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("tmp"))) ||
        !FProjectWorldCanonicalLoader::ComputeFileSha256(Path,Actual) || Actual!=Hash)
    {AddError(TEXT("Supply a pinned confined saved-regional-view input."));return false;}
    auto State=MakeShared<FState>(*this);State->Root=FPaths::GetPath(Path);
    if (!ReadObject(Path,State->Input)) {AddError(TEXT("Cannot parse regional-view input."));return false;}
    const auto& Input=*State->Input;
    for (const TCHAR* Key : {TEXT("map"),TEXT("presentation_profile"),TEXT("presentation_sha256"),TEXT("vantage_plan"),
        TEXT("vantage_plan_sha256"),TEXT("sections"),TEXT("original_fixture")})
        if (!Input.HasTypedField<EJson::String>(Key)) {AddError(TEXT("Regional-view input lacks a required string field."));return false;}
    const TArray<TSharedPtr<FJsonValue>>* Start=nullptr;const TSharedPtr<FJsonObject>* Saved=nullptr;
    if (!Input.TryGetArrayField(TEXT("start"),Start) || !Start || Start->Num()!=3 ||
        !Input.TryGetObjectField(TEXT("saved_files"),Saved) || !Saved || !Saved->IsValid() || (*Saved)->Values.Num()>1024)
    {AddError(TEXT("Regional-view source inventory or camera placement is invalid."));return false;}
    for (const auto& Value : *Start)
    {double Number=0;if (!Value->TryGetNumber(Number) || !FMath::IsFinite(Number)) {AddError(TEXT("Regional view needs a finite camera."));return false;}}
    State->Map=Input.GetStringField(TEXT("map"));FString MapFile;
    if (!FPackageName::TryConvertLongPackageNameToFilename(State->Map,MapFile,FPackageName::GetMapPackageExtension()) ||
        !FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(MapFile),FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("tmp"))))
    {AddError(TEXT("Regional observation refuses a map outside disposable scratch ownership."));return false;}
    State->Profile=FPaths::ProjectDir()/Input.GetStringField(TEXT("presentation_profile"));State->ProfileHash=Input.GetStringField(TEXT("presentation_sha256"));
    State->Plan=FPaths::ProjectDir()/Input.GetStringField(TEXT("vantage_plan"));State->PlanHash=Input.GetStringField(TEXT("vantage_plan_sha256"));
    const FString Sections=FPaths::ProjectDir()/Input.GetStringField(TEXT("sections"));
    for (const auto& Pair : (*Saved)->Values)
    {
        FString Digest;if (!Pair.Value->TryGetString(Digest) || Digest.Len()!=64)
        {AddError(TEXT("Every regional saved input needs an explicit digest."));return false;}
        const FString File=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/FString(*Pair.Key));
        if (!FPaths::IsUnderDirectory(File,FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("tmp"))))
        {AddError(TEXT("Regional source packages must remain under scratch ownership."));return false;}
        State->SourceFiles.Add(File,Digest);
        if (File.EndsWith(TEXT(".uasset")) || File.EndsWith(TEXT(".umap")))
        {
            FString Package;
            if (!FPackageName::TryConvertFilenameToLongPackageName(File,Package))
            {AddError(TEXT("A declared regional package cannot resolve through its native mount."));return false;}
            State->Packages.AddUnique(Package);
        }
    }
    if (!State->SourceFiles.Contains(FPaths::ConvertRelativePathToFull(Sections)) || !State->Unchanged(Error) ||
        !ReadObject(Sections,State->Expected))
    {AddError(TEXT("Saved observations and package bytes must authenticate before parsing or consuming their identities. ")+Error);return false;}
    const FString Fixture=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/Input.GetStringField(TEXT("original_fixture")));
    if (!State->SourceFiles.Contains(FPaths::ConvertRelativePathToFull(MapFile)) ||
        !State->SourceFiles.Contains(FPaths::ConvertRelativePathToFull(Sections)) || !State->SourceFiles.Contains(Fixture))
    {AddError(TEXT("The consumed map, section observations and original fixture require pins in the single saved-input inventory."));return false;}
    for (const auto& Row : State->Expected->GetArrayField(TEXT("variants"))) for (const auto& Binding : NativeBindings(*Row->AsObject()))
        if (!State->Packages.Contains(Binding->GetStringField(TEXT("actor_package"))))
        {AddError(TEXT("A consumed native section package is absent from the authenticated inventory."));return false;}
    for (const auto& Row : State->Expected->GetObjectField(TEXT("regional_water_before"))->GetArrayField(TEXT("actors")))
        if (!State->Packages.Contains(Row->AsObject()->GetStringField(TEXT("actor_package"))) ||
            !State->Packages.Contains(FPackageName::ObjectPathToPackageName(Row->AsObject()->GetStringField(TEXT("mesh")))))
        {AddError(TEXT("A consumed Water actor or mesh package is absent from the authenticated inventory."));return false;}
    const TSharedPtr<FJsonObject>* Structures=nullptr;
    if (State->Expected->TryGetObjectField(TEXT("regional_structures_before"),Structures))
        for (const auto& Row : (*Structures)->GetArrayField(TEXT("actors")))
            if (!State->Packages.Contains(Row->AsObject()->GetStringField(TEXT("actor_package"))) ||
                !State->Packages.Contains(FPackageName::ObjectPathToPackageName(Row->AsObject()->GetStringField(TEXT("mesh")))))
            {AddError(TEXT("A consumed structural actor or mesh package is absent from the authenticated inventory."));return false;}
    const FString Definition=FPackageName::LongPackageNameToFilename(FPackageName::ObjectPathToPackageName(
        State->Expected->GetStringField(TEXT("expected_definition_object_path"))),FPackageName::GetAssetPackageExtension());
    State->SourceFiles.Add(Definition,State->Expected->GetStringField(TEXT("source_definition_sha256")));
    State->SourceFiles.Add(State->Profile,State->ProfileHash);State->SourceFiles.Add(State->Plan,State->PlanHash);
    if (State->SourceFiles.Num()<4 || !State->Unchanged(Error)) {AddError(Error);return false;}
    TSharedPtr<FJsonObject> Original;
    if (State->SourceFiles.FindChecked(Fixture)!=State->Expected->GetStringField(TEXT("fixture_sha256")) ||
        !ReadObject(Fixture,Original) || !ProjectWorldTerrainCanonicalFixture::Read(*this,*Original,State->Bundle,true))
    {AddError(TEXT("Saved regional view must bind its original source fixture and that fixture's authenticated canonical field."));return false;}
    UWorld* EditorWorld=GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!EditorWorld || EditorWorld->GetOutermost()->GetName()!=State->Map)
    {AddError(TEXT("Cold observer must open the exact declared saved regional map."));return false;}
    State->EditorWorld=EditorWorld;
    State->Receipt->SetStringField(TEXT("input_sha256"),Hash);
    auto* Settings=NewObject<ULevelEditorPlaySettings>(GetTransientPackage());
    Settings->SetPlayNetMode(PIE_Standalone);Settings->SetPlayNumberOfClients(1);Settings->SetRunUnderOneProcess(true);Settings->bLaunchSeparateServer=false;
    FRequestPlaySessionParams Request;Request.EditorPlaySettings=Settings;Request.bAllowOnlineSubsystem=false;
    Request.GameModeOverride=AGameModeBase::StaticClass();
    Request.StartLocation=FVector((*Start)[0]->AsNumber(),(*Start)[1]->AsNumber(),(*Start)[2]->AsNumber());
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FStart>(Request));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FObserve>(State));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FEndPlayMapCommand>());
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FCleanup>(State));return true;
}
#endif
