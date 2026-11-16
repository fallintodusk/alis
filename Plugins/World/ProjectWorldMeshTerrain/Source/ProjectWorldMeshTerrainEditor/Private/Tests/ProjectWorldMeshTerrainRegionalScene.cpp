// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldMeshTerrainRegionalScene.h"
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
namespace ProjectWorldRegionalSceneTest
{
    using namespace UE::MeshPartition;
    bool FScene::Unchanged(FString& Error) const
        {
            for (const auto& Pair : SourceFiles)
            {
                FString Hash;
                if (!FProjectWorldCanonicalLoader::ComputeFileSha256(Pair.Key,Hash) || Hash!=Pair.Value)
                {Error=TEXT("Regional observation changed or lost a declared saved input.");return false;}
            }
            return true;
        }
    bool FScene::Finalizable(FString& Error) const
        {
            if (GEditor && GEditor->PlayWorld)
            {Error=TEXT("An active PIE world cannot be accepted as cleaned up.");return false;}
            return Unchanged(Error);
        }
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
    bool Census(FScene& State,UWorld* World,FString& Error,FName RequiredVariant)
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
    bool ReadReference(FScene& Scene,const FString& Path,const FString& Hash)
    {
    auto* State=&Scene;FString Actual,Error;
    if (!FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(Path),FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("tmp"))) ||
        !FProjectWorldCanonicalLoader::ComputeFileSha256(Path,Actual) || Actual!=Hash)
    {State->Test.AddError(TEXT("Supply a pinned confined saved-regional input."));return false;}
    State->Root=FPaths::GetPath(Path);

    if (!ReadObject(Path,State->Input)) {State->Test.AddError(TEXT("Cannot parse regional-view input."));return false;}
    const auto& Input=*State->Input;
    for (const TCHAR* Key : {TEXT("map"),TEXT("presentation_profile"),TEXT("presentation_sha256"),TEXT("vantage_plan"),
        TEXT("vantage_plan_sha256"),TEXT("sections"),TEXT("original_fixture")})
        if (!Input.HasTypedField<EJson::String>(Key)) {State->Test.AddError(TEXT("Regional-view input lacks a required string field."));return false;}
    const TArray<TSharedPtr<FJsonValue>>* Start=nullptr;const TSharedPtr<FJsonObject>* Saved=nullptr;
    if (!Input.TryGetArrayField(TEXT("start"),Start) || !Start || Start->Num()!=3 ||
        !Input.TryGetObjectField(TEXT("saved_files"),Saved) || !Saved || !Saved->IsValid() || (*Saved)->Values.Num()>1024)
    {State->Test.AddError(TEXT("Regional-view source inventory or camera placement is invalid."));return false;}
    for (const auto& Value : *Start)
    {double Number=0;if (!Value->TryGetNumber(Number) || !FMath::IsFinite(Number)) {State->Test.AddError(TEXT("Regional view needs a finite camera."));return false;}}
    State->Map=Input.GetStringField(TEXT("map"));FString MapFile;
    if (!FPackageName::TryConvertLongPackageNameToFilename(State->Map,MapFile,FPackageName::GetMapPackageExtension()) ||
        !FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(MapFile),FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("tmp"))))
    {State->Test.AddError(TEXT("Regional observation refuses a map outside disposable scratch ownership."));return false;}
    State->Profile=FPaths::ProjectDir()/Input.GetStringField(TEXT("presentation_profile"));State->ProfileHash=Input.GetStringField(TEXT("presentation_sha256"));
    State->Plan=FPaths::ProjectDir()/Input.GetStringField(TEXT("vantage_plan"));State->PlanHash=Input.GetStringField(TEXT("vantage_plan_sha256"));
    const FString Sections=FPaths::ProjectDir()/Input.GetStringField(TEXT("sections"));
    for (const auto& Pair : (*Saved)->Values)
    {
        FString Digest;if (!Pair.Value->TryGetString(Digest) || Digest.Len()!=64)
        {State->Test.AddError(TEXT("Every regional saved input needs an explicit digest."));return false;}
        const FString File=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/FString(*Pair.Key));
        if (!FPaths::IsUnderDirectory(File,FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("tmp"))))
        {State->Test.AddError(TEXT("Regional source packages must remain under scratch ownership."));return false;}
        State->SourceFiles.Add(File,Digest);
        if (File.EndsWith(TEXT(".uasset")) || File.EndsWith(TEXT(".umap")))
        {
            FString Package;
            if (!FPackageName::TryConvertFilenameToLongPackageName(File,Package))
            {State->Test.AddError(TEXT("A declared regional package cannot resolve through its native mount."));return false;}
            State->Packages.AddUnique(Package);
        }
    }
    if (!State->SourceFiles.Contains(FPaths::ConvertRelativePathToFull(Sections)) || !State->Unchanged(Error) ||
        !ReadObject(Sections,State->Expected))
    {State->Test.AddError(TEXT("Saved observations and package bytes must authenticate before parsing or consuming their identities. ")+Error);return false;}
    const FString Fixture=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/Input.GetStringField(TEXT("original_fixture")));
    if (!State->SourceFiles.Contains(FPaths::ConvertRelativePathToFull(MapFile)) ||
        !State->SourceFiles.Contains(FPaths::ConvertRelativePathToFull(Sections)) || !State->SourceFiles.Contains(Fixture))
    {State->Test.AddError(TEXT("The consumed map, section observations and original fixture require pins in the single saved-input inventory."));return false;}
    for (const auto& Row : State->Expected->GetArrayField(TEXT("variants"))) for (const auto& Binding : NativeBindings(*Row->AsObject()))
        if (!State->Packages.Contains(Binding->GetStringField(TEXT("actor_package"))))
        {State->Test.AddError(TEXT("A consumed native section package is absent from the authenticated inventory."));return false;}
    for (const auto& Row : State->Expected->GetObjectField(TEXT("regional_water_before"))->GetArrayField(TEXT("actors")))
        if (!State->Packages.Contains(Row->AsObject()->GetStringField(TEXT("actor_package"))) ||
            !State->Packages.Contains(FPackageName::ObjectPathToPackageName(Row->AsObject()->GetStringField(TEXT("mesh")))))
        {State->Test.AddError(TEXT("A consumed Water actor or mesh package is absent from the authenticated inventory."));return false;}
    const TSharedPtr<FJsonObject>* Structures=nullptr;
    if (State->Expected->TryGetObjectField(TEXT("regional_structures_before"),Structures))
        for (const auto& Row : (*Structures)->GetArrayField(TEXT("actors")))
            if (!State->Packages.Contains(Row->AsObject()->GetStringField(TEXT("actor_package"))) ||
                !State->Packages.Contains(FPackageName::ObjectPathToPackageName(Row->AsObject()->GetStringField(TEXT("mesh")))))
            {State->Test.AddError(TEXT("A consumed structural actor or mesh package is absent from the authenticated inventory."));return false;}
    const FString Definition=FPackageName::LongPackageNameToFilename(FPackageName::ObjectPathToPackageName(
        State->Expected->GetStringField(TEXT("expected_definition_object_path"))),FPackageName::GetAssetPackageExtension());
    State->SourceFiles.Add(Definition,State->Expected->GetStringField(TEXT("source_definition_sha256")));
    State->SourceFiles.Add(State->Profile,State->ProfileHash);State->SourceFiles.Add(State->Plan,State->PlanHash);
    if (State->SourceFiles.Num()<4 || !State->Unchanged(Error)) {State->Test.AddError(Error);return false;}
    TSharedPtr<FJsonObject> Original;
    if (State->SourceFiles.FindChecked(Fixture)!=State->Expected->GetStringField(TEXT("fixture_sha256")) ||
        !ReadObject(Fixture,Original) || !ProjectWorldTerrainCanonicalFixture::Read(State->Test,*Original,State->Bundle,true))
    {State->Test.AddError(TEXT("Saved regional view must bind its original source fixture and that fixture's authenticated canonical field."));return false;}
    State->SourceFiles.Add(FPaths::ConvertRelativePathToFull(Path),Hash);
    State->Receipt->SetStringField(TEXT("input_sha256"),Hash);
    return true;
    }
    bool Read(FScene& Scene,const FString& Path,const FString& Hash)
    {
    auto* State=&Scene;
    if (!ReadReference(Scene,Path,Hash)) return false;
    UWorld* EditorWorld=GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!EditorWorld || EditorWorld->GetOutermost()->GetName()!=State->Map)
    {State->Test.AddError(TEXT("Cold observer must open the exact declared saved regional map."));return false;}
    State->EditorWorld=EditorWorld;
    return true;
    }
}
#endif
