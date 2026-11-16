#include "ProjectWorldMeshTerrainSavedRegionScene.h"
#include "ProjectWorldMeshTerrainRegionalScene.h"
#include "ProjectWorldMeshTerrainBasinScene.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/StaticMeshActor.h"
#include "MeshPartitionCompiledSection.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "WorldPartition/WorldPartition.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldSavedRegionSceneTest
{
    using namespace ProjectWorldRegionalSceneTest;
    using FReferenceScene = ProjectWorldRegionalSceneTest::FScene;
    namespace
    {
        bool Pinned(FReferenceScene& Scene,const TSharedPtr<FJsonObject>& Pin,TSharedPtr<FJsonObject>& Out,FString& Error)
        {
            FString Label,Hash,Actual;
            if (!Pin || !Pin->TryGetStringField(TEXT("path"),Label) || !FPaths::IsRelative(Label) ||
                !Pin->TryGetStringField(TEXT("sha256"),Hash) || Hash.Len()!=64)
            {Error=TEXT("Saved product evidence requires explicit confined file pins.");return false;}
            const FString File=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/Label);
            if (!FPaths::IsUnderDirectory(File,FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("tmp/world"))) ||
                !FProjectWorldCanonicalLoader::ComputeFileSha256(File,Actual) || Actual!=Hash || !ReadObject(File,Out))
            {Error=TEXT("Saved product evidence is missing, foreign or changed.");return false;}
            Scene.SourceFiles.Add(File,Hash);return true;
        }
        bool LiveCopy(FReferenceScene& Scene,const FString& File,const FString& Hash,FString& Error)
        {
            FString Actual;
            if (!FProjectWorldCanonicalLoader::ComputeFileSha256(File,Actual) || Actual!=Hash)
            {Error=TEXT("A retained proof differs from its actual saved product authority.");return false;}
            Scene.SourceFiles.Add(File,Hash);return true;
        }
    }
    bool FProductScene::Read(FReferenceScene& Scene,const FString& Path,const FString& Hash,const FString& GameplayHash,FString& Error)
    {
        FString Actual;
        if (!FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(Path),FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("tmp/world"))) ||
            !FProjectWorldCanonicalLoader::ComputeFileSha256(Path,Actual) || Actual!=Hash)
        {Error=TEXT("Saved product walking requires a pinned scratch proof.");return false;}
        TSharedPtr<FJsonObject> Proof,Active,Original;
        if (!ReadObject(Path,Proof)) {Error=TEXT("Cannot parse saved product proof.");return false;}
        FString Schema,Compile,ReferenceHash;
        if (!Proof->TryGetStringField(TEXT("schema"),Schema) || Schema!=TEXT("project-world-saved-region-gameplay:v1") ||
            !Proof->TryGetStringField(TEXT("map"),Map) || !Map.StartsWith(TEXT("/ProjectWorldData/Generated/")) ||
            !Proof->TryGetStringField(TEXT("compile_result_sha256"),Compile) || Compile.Len()!=64 ||
            !Proof->TryGetStringField(TEXT("reference_gameplay_sha256"),ReferenceHash) || ReferenceHash!=GameplayHash ||
            !Proof->HasTypedField<EJson::Object>(TEXT("world_active")) || !Proof->HasTypedField<EJson::Array>(TEXT("manifests")) ||
            !ReadObject(FPaths::ProjectDir()/Scene.Input->GetStringField(TEXT("original_fixture")),Original) ||
            Original->GetObjectField(TEXT("canonical_result"))->GetStringField(TEXT("sha256"))!=Compile)
        {Error=TEXT("Saved product walking must consume the same authenticated canonical child and reference frame.");return false;}
        const auto ActivePin=Proof->GetObjectField(TEXT("world_active"));
        if (!Pinned(Scene,ActivePin,Active,Error)) return false;
        const FString AuthorityRoot=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("Plugins/World/ProjectWorldData/Data/Manifests"));
        if (!LiveCopy(Scene,AuthorityRoot/TEXT("active_set.json"),ActivePin->GetStringField(TEXT("sha256")),Error)) return false;
        const auto& Pins=Proof->GetArrayField(TEXT("manifests"));
        if (Pins.Num()<2 || Pins.Num()>3) {Error=TEXT("Saved walking needs the actual map, Terrain and optional structure manifests.");return false;}
        TSet<FString> Owners;bool MapPinned=false;
        for (const auto& Value : Pins)
        {
            const auto Pin=Value->AsObject();TSharedPtr<FJsonObject> Manifest;
            if (!Pinned(Scene,Pin,Manifest,Error)) return false;
            const FString Scope=Manifest->GetStringField(TEXT("scope_id")),Owner=Manifest->GetStringField(TEXT("owning_layer"));
            const auto Identity=Manifest->GetObjectField(TEXT("input_identity"));
            if (Owners.Contains(Owner) || (Owner!=TEXT("map") && Owner!=TEXT("terrain") && Owner!=TEXT("structures")) ||
                Identity->GetStringField(TEXT("map_package"))!=Map || (Owner!=TEXT("map") && Identity->GetStringField(TEXT("compile_result_sha256"))!=Compile))
            {Error=TEXT("Saved product floor owners do not match their actual map and effective child.");return false;}
            Owners.Add(Owner);TSharedPtr<FJsonObject> Entry;
            for (const auto& Row : Active->GetArrayField(TEXT("scopes")))
                if (Row->AsObject()->GetStringField(TEXT("scope_id"))==Scope) {if (Entry) {Error=TEXT("Duplicated saved floor scope.");return false;}Entry=Row->AsObject();}
            if (!Entry || Entry->GetStringField(TEXT("manifest_sha256"))!=Pin->GetStringField(TEXT("sha256")))
            {Error=TEXT("Saved floor evidence is not selected by actual World authority.");return false;}
            const FString ManifestFile=FPaths::ConvertRelativePathToFull(AuthorityRoot/Entry->GetStringField(TEXT("manifest_path")));
            if (!FPaths::IsUnderDirectory(ManifestFile,AuthorityRoot/TEXT("scopes")) ||
                !LiveCopy(Scene,ManifestFile,Pin->GetStringField(TEXT("sha256")),Error)) return false;
            for (const auto& Row : Manifest->GetArrayField(TEXT("artifacts")))
            {
                const auto Artifact=Row->AsObject();const FString Label=Artifact->GetStringField(TEXT("path"));
                const FString File=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/Label);FString Package;
                if (!FPaths::IsUnderDirectory(File,FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("Plugins/World/ProjectWorldData/Content"))) ||
                    Artifact->GetStringField(TEXT("digest_kind"))!=TEXT("sha256") ||
                    !LiveCopy(Scene,File,Artifact->GetStringField(TEXT("digest")),Error) ||
                    !FPackageName::TryConvertFilenameToLongPackageName(File,Package)) return false;
                if (Owner!=TEXT("map")) Packages.AddUnique(Package);
                else if (Package==Map) MapPinned=true;
            }
        }
        const TSharedPtr<FJsonObject>* Bodies=nullptr;
        const bool HasBodies=Scene.Expected->TryGetObjectField(TEXT("regional_structures_before"),Bodies) &&
            !(*Bodies)->GetArrayField(TEXT("actors")).IsEmpty();
        if (!MapPinned || !Owners.Contains(TEXT("terrain")) || (HasBodies && !Owners.Contains(TEXT("structures"))) || Packages.IsEmpty())
        {Error=TEXT("Saved product support has an incomplete map/Terrain/structure ownership census.");return false;}
        UWorld* World=GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (!World || World->GetOutermost()->GetName()!=Map) {Error=TEXT("The observer must open the exact actual saved product map.");return false;}
        Scene.Receipt->SetStringField(TEXT("reference_map"),Scene.Map);
        Scene.Receipt->SetStringField(TEXT("saved_product_map"),Map);
        Scene.Receipt->SetStringField(TEXT("saved_product_proof_sha256"),Hash);
        Scene.Receipt->SetStringField(TEXT("compile_result_sha256"),Compile);
        Scene.Receipt->SetStringField(TEXT("world_active_sha256"),ActivePin->GetStringField(TEXT("sha256")));
        Scene.Receipt->SetNumberField(TEXT("saved_floor_package_count"),Packages.Num());
        Scene.SourceFiles.Add(FPaths::ConvertRelativePathToFull(Path),Hash);Scene.Map=Map;Scene.EditorWorld=World;return true;
    }
    bool FProductScene::Ready(UWorld* World,FString& Error) const
    {
        int32 Instance=INDEX_NONE;
        if (!World || World->WorldType!=EWorldType::PIE || UWorld::RemovePIEPrefix(World->GetOutermost()->GetName(),&Instance)!=Map || Instance<0)
        {Error=TEXT("Saved product walking refuses a different PIE world.");return false;}
        auto* Partition=World->GetWorldPartition();
        if (!Partition || Partition->GetStreamingSources().IsEmpty() || !Partition->IsStreamingCompleted(&Partition->GetStreamingSources()))
        {Error=TEXT("Waiting for route-local product streaming.");return false;}
        return true;
    }
    bool FProductScene::Owns(const UPrimitiveComponent* Component) const
    {
        const auto* Actor=Component ? Component->GetOwner() : nullptr;
        if (!Actor) return false;
        const auto* Section=Cast<UE::MeshPartition::ACompiledSection>(Actor);
        if (Section ? Section->IsPlaceholder() :
            (!Actor->IsA<AStaticMeshActor>() || !Actor->ActorHasTag(TEXT("ProjectWorld.ConstructedSurface.v1")))) return false;
        const FString Source=Actor->GetPackage()->GetLoadedPath().GetPackageName();
        ProjectWorldBasinSceneTest::FInput Ownership;Ownership.PackageName=Map;Ownership.Packages=Packages;
        return ProjectWorldBasinSceneTest::OwnsSavedActor(Actor,Actor->GetWorld(),Ownership,Source);
    }
}
#endif
