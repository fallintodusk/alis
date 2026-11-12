#include "ProjectWorldMeshTerrainStorageFixture.h"
#include "ProjectWorldMeshTerrainSectionCollection.h"
#include "ProjectWorldMeshTerrainProducer.h"
#include "ProjectWorldMeshTerrainSurfaceFidelity.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "MeshPartitionCompiledSection.h"
#include "MeshPartitionStaticMeshComponent.h"
#include "MeshPartitionCollisionComponent.h"
#include "MeshPartition.h"
#include "MeshPartitionDefinition.h"
#include "MeshPartitionTransformerPipeline.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Misc/PackageName.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "PackageTools.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/PlatformMisc.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UObjectHash.h"
#include "Tests/AutomationEditorCommon.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionHandle.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldTerrainStorageFixture
{
    bool ReplaySaved(FAutomationTestBase& Test, UWorld& OriginalWorld, const FString& Filename,
        const FProjectWorldCanonicalBundle& Bundle, const TMap<FName, FSplitSections>& Inputs,
        TFunctionRef<void()> ReleaseOriginalWorld, FJsonObject& Receipt,TConstArrayView<FVector> RequiredPoints,
        bool CompareStorageFrame,TFunction<bool(UWorld&,FJsonObject&)> ReloadObserver,
        TConstArrayView<FString> ExtraPackages,bool CompareBase)
    {
        using namespace UE::MeshPartition;
        FString Ini,Sentinel;
        if (!Test.TestTrue(TEXT("Saved section replay requires its cold isolated editor process and scratch settings."),
            GEditor && !GEditor->PlayWorld && FPlatformMisc::GetEnvironmentVariable(TEXT("ALIS_NO_PERSISTENT_EDITOR"))==TEXT("1") &&
            FParse::Param(FCommandLine::Get(),TEXT("ProjectSkipFrontEnd")) &&
            FParse::Value(FCommandLine::Get(),TEXT("testexit="),Sentinel) && Sentinel==TEXT("Automation Test Queue Empty") &&
            FParse::Value(FCommandLine::Get(),TEXT("EditorPerProjectUserSettingsINI="),Ini) &&
            FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(Ini),FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("tmp"))) &&
            FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(Filename),FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("tmp"))))) return false;
        struct FSectionIdentity { FName Name; FGuid BuildKey; FTransform Transform; FString Definition,Material; };
        struct FRootIdentity { FName Name; FTransform Transform; FString Definition; };
        TArray<FRootIdentity> Roots;
        TMap<FName,TArray<FSectionIdentity>> Identities;
        TMap<FName,FString> ExternalActors;
        TMap<FString,FString> ExternalHashes;
        TArray<UPackage*> OwnedPackages={OriginalWorld.GetPackage()};
        TArray<TWeakObjectPtr<UObject>> Originals={&OriginalWorld};
        const FString PackageName=OriginalWorld.GetPackage()->GetName();
        FString Before;
        if (!Test.TestTrue(TEXT("Section replay authenticates the actual saved map before releasing it."),
            FProjectWorldCanonicalLoader::ComputeFileSha256(Filename,Before))) return false;
        for (TActorIterator<AActor> It(&OriginalWorld);It;++It)
        {
            Originals.Add(*It);
            if (const auto* External=It->GetExternalPackage()) ExternalActors.Add(It->GetFName(),External->GetName());
        }
        for (auto* External : OriginalWorld.GetPackage()->GetExternalPackages())
        {
            const FString Path=FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(
                External->GetName(),FPackageName::GetAssetPackageExtension()));
            FString Hash;
            if (!Test.TestTrue(TEXT("Every native external actor/object package is saved and confined to the scratch scene."),
                FPaths::IsUnderDirectory(Path,FPaths::GetPath(Filename)) &&
                FProjectWorldCanonicalLoader::ComputeFileSha256(Path,Hash))) return false;
            OwnedPackages.AddUnique(External);ExternalHashes.Add(External->GetName(),Hash);
        }
        if (OriginalWorld.PersistentLevel->IsUsingExternalActors() &&
            !Test.TestTrue(TEXT("External-actor replay has a positive saved actor and package census."),
                !ExternalActors.IsEmpty() && !ExternalHashes.IsEmpty())) return false;
        for (const auto& Pair : ExternalActors)
            if (!Test.TestTrue(FString::Printf(TEXT("Every live external actor has its exact authenticated saved package: %s -> %s"),*Pair.Key.ToString(),*Pair.Value),
                ExternalHashes.Contains(Pair.Value))) return false;
        for (const auto& Name : ExtraPackages) if (Name!=PackageName)
        {
            auto* Extra=FindPackage(nullptr,*Name);
            const FString File=FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(Name,FPackageName::GetAssetPackageExtension()));
            FString Hash;
            if (!Test.TestTrue(TEXT("Additional saved scene assets are loaded, confined and authenticated before release."),
                Extra && FPaths::IsUnderDirectory(File,FPaths::GetPath(Filename)) &&
                FProjectWorldCanonicalLoader::ComputeFileSha256(File,Hash))) return false;
            OwnedPackages.AddUnique(Extra);ExternalHashes.Add(Name,Hash);
            TArray<UObject*> Objects;GetObjectsWithOuter(Extra,Objects,EGetObjectsFlags::IncludeNestedObjects);
            for (auto* Object : Objects) Originals.Add(Object);
        }
        for (TActorIterator<AMeshPartition> It(&OriginalWorld);It;++It)
        {
            auto* Definition=It->GetMeshPartitionDefinition();
            Roots.Add({It->GetFName(),It->GetActorTransform(),GetPathNameSafe(Definition)});
            if (Definition && Definition->GetPackage()==OriginalWorld.GetPackage())
            {
                Originals.Add(Definition);
                for (const auto& Variant : Definition->GetCompiledSectionBuildVariants()) Originals.Add(Variant.TransformerPipeline.Get());
            }
        }
        int32 ExpectedCount=0;
        for (const auto& Pair : Inputs) for (auto* Section : Pair.Value.Sections)
        {
            if (!Test.TestTrue(TEXT("The saved section census is live and belongs to the original world."),
                IsValid(Section) && Section->GetWorld()==&OriginalWorld)) return false;
            const auto& Info=Section->GetBuildInfo();
            Identities.FindOrAdd(Pair.Key).Add({Section->GetFName(),Info.BuildKey,Section->GetActorTransform(),
                GetPathNameSafe(Info.GetMegaMeshDefinition()),GetPathNameSafe(Info.GetMegaMeshDefinition()->GetMaterial())});
            ++ExpectedCount; Originals.Add(Section);
            for (const auto& Component : Section->GetMeshComponents())
            { Originals.Add(Component.Get()); Originals.Add(Component->GetStaticMesh()); }
            for (const auto& Component : Section->GetCollisionComponents()) Originals.Add(Component.Get());
        }
        if (!Test.TestTrue(TEXT("Replay cannot pass with an empty saved section census."),ExpectedCount>0 && Inputs.Num()==3)) return false;
        TArray<FString> OwnedPackageNames;
        for (auto* Owned:OwnedPackages) OwnedPackageNames.AddUnique(Owned->GetName());
        ReleaseOriginalWorld();
        OwnedPackages.Reset();
        for (const auto& Name:OwnedPackageNames) if (auto* Remaining=FindPackage(nullptr,*Name)) OwnedPackages.Add(Remaining);
        Receipt.SetNumberField(TEXT("owned_packages_before_release"),OwnedPackageNames.Num());
        Receipt.SetNumberField(TEXT("remaining_packages_after_world_release"),OwnedPackages.Num());
        if (!OwnedPackages.IsEmpty())
        {
            UPackageTools::FUnloadPackageParams Params(OwnedPackages); Params.bResetTransBuffer=false;
            if (!Test.TestTrue(TEXT("Remaining saved scene packages unload before replay."),UPackageTools::UnloadPackages(Params))) return false;
        }
        for (const auto& Original : Originals)
            if (!Test.TestFalse(TEXT("Replay cannot reuse a live original world, section, visual mesh or collider."),Original.IsValid())) return false;
        bool Opened=false;
        TArray<FWorldPartitionReference> LoadedReferences;
        ON_SCOPE_EXIT {LoadedReferences.Reset();if (Opened) FAutomationEditorCommonUtils::CreateNewMap();};
        TArray<FString> SavedAssetFiles;
        for (const auto& Pair : ExternalHashes)
            SavedAssetFiles.Add(FPackageName::LongPackageNameToFilename(Pair.Key,FPackageName::GetAssetPackageExtension()));
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().ScanFilesSynchronous(SavedAssetFiles,true);
        Receipt.SetNumberField(TEXT("fresh_asset_registry_files"),SavedAssetFiles.Num());
        const bool Loaded=FEditorFileUtils::LoadMap(Filename,false,false);
        UWorld* World=GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        Opened=World && World->GetPackage()->GetName()==PackageName;
        if (!Test.TestTrue(TEXT("Replay opens the exact saved map in a fresh native Editor world."),Loaded && Opened)) return false;
        if (auto* Partition=World->GetWorldPartition()) Partition->LoadAllActors(LoadedReferences);
        World->UpdateWorldComponents(false,false); World->SendAllEndOfFrameUpdates();
        int32 ExternalActorCount=0;
        for (TActorIterator<AActor> It(World);It;++It) if (const auto* External=It->GetExternalPackage())
        {
            const auto* Expected=ExternalActors.Find(It->GetFName());
            if (!Test.TestTrue(TEXT("Fresh native loading restores each exact saved external actor package binding."),
                Expected && *Expected==External->GetName())) return false;
            ++ExternalActorCount;
        }
        if (!Test.TestEqual(TEXT("Fresh native loading retains the entire external actor census."),ExternalActorCount,ExternalActors.Num())) return false;
        int32 FoundRoots=0;
        for (TActorIterator<AMeshPartition> It(World);It;++It)
        {
            const auto* Match=Roots.FindByPredicate([&](const auto& Root){ return Root.Name==It->GetFName(); });
            if (!Test.TestTrue(TEXT("The freshly loaded partition retains its saved identity, frame and private definition."),
                Match && It->GetActorTransform().Equals(Match->Transform,1.e-6) &&
                GetPathNameSafe(It->GetMeshPartitionDefinition())==Match->Definition)) return false;
            ++FoundRoots;
        }
        if (!Test.TestTrue(TEXT("Replay retains every actual saved partition root."),FoundRoots>0 && FoundRoots==Roots.Num())) return false;
        TMap<FName,ACompiledSection*> Sections;
        for (TActorIterator<ACompiledSection> It(World);It;++It)
        {
            if (!Test.TestFalse(TEXT("Reloaded native section names cannot alias."),Sections.Contains(It->GetFName()))) return false;
            Sections.Add(It->GetFName(),*It);
        }
        if (!Test.TestEqual(TEXT("Reload retains the complete compiled section census without new sections."),Sections.Num(),ExpectedCount)) return false;
        TArray<TSharedPtr<FJsonValue>> Variants;
        for (const auto& Pair : Inputs)
        {
            FSplitSections Split; Split.Parts=Pair.Value.Parts;
            for (const auto& Identity : Identities.FindChecked(Pair.Key))
            {
                auto* const* Found=Sections.Find(Identity.Name);
                if (!Test.TestTrue(TEXT("Saved section identity, variant, private definition and frame survive without rebuilding."),
                    Found && (*Found)->GetBuildInfo().BuildKey==Identity.BuildKey &&
                    (*Found)->GetBuildInfo().BuildVariantName==Pair.Key &&
                    GetPathNameSafe((*Found)->GetBuildInfo().GetMegaMeshDefinition())==Identity.Definition &&
                    (*Found)->GetBuildInfo().GetMegaMeshDefinition() &&
                    GetPathNameSafe((*Found)->GetBuildInfo().GetMegaMeshDefinition()->GetMaterial())==Identity.Material &&
                    (*Found)->GetActorTransform().Equals(Identity.Transform,1.e-6))) return false;
                Split.Sections.Add(*Found);
            }
            ProjectWorldTerrainSectionCollection::FCollected Collected;
            if (!ProjectWorldTerrainSectionCollection::Collect(Test,*World,Bundle,Split.Sections,Pair.Key,true,true,Collected,RequiredPoints)) return false;
            auto Row=MakeShared<FJsonObject>(); Row->SetStringField(TEXT("variant"),Pair.Key.ToString());
            if (Collected.RequiredPhysics.IsValid()) Row->SetObjectField(TEXT("required_physics"),Collected.RequiredPhysics);
            if (Collected.TerrainPhysicsIsolation) Row->SetObjectField(TEXT("terrain_physics_isolation"),Collected.TerrainPhysicsIsolation);
            TArray<TSharedPtr<FJsonValue>> Keys,Names;
            for (auto* Section : Split.Sections)
            {
                Keys.Add(MakeShared<FJsonValueString>(Section->GetBuildInfo().BuildKey.ToString(EGuidFormats::Digits)));
                Names.Add(MakeShared<FJsonValueString>(Section->GetPathName()));
            }
            Row->SetArrayField(TEXT("native_build_keys"),Keys); Row->SetArrayField(TEXT("native_section_paths"),Names);
            if (CompareStorageFrame && !CheckShared(Test,Bundle,Split,Collected.PerSection,*Row)) return false;
            FProjectWorldMeshTerrainSurfaceFidelity Fidelity; FString Error;
            if (CompareStorageFrame && !Test.TestFalse(TEXT("Reload still refuses the same payload with its actual storage compensation omitted."),
                ProjectWorldMeshTerrainProducer::CompareRefinedSurface(Bundle,Collected.Uncompensated,1.e-4,Fidelity,Error))) return false;
            const bool Covered=CompareBase ? ProjectWorldMeshTerrainProducer::CompareCanonicalSurface(Bundle,Collected.Actual,1.e-4,Fidelity,Error) :
                ProjectWorldMeshTerrainProducer::CompareRefinedSurface(Bundle,Collected.Actual,1.e-4,Fidelity,Error);
            Row->SetNumberField(TEXT("actual_faces"),Collected.Actual.Num());
            Row->SetNumberField(TEXT("expected_triangles"),Fidelity.ExpectedTriangles);
            Row->SetNumberField(TEXT("mismatched_triangles"),Fidelity.MismatchedTriangles);
            Row->SetNumberField(TEXT("maximum_whole_field_error_m"),Fidelity.MaximumErrorMeters);
            Row->SetArrayField(TEXT("components"),Collected.Components); Variants.Add(MakeShared<FJsonValueObject>(Row));
            if (!Test.TestTrue(TEXT("Every freshly loaded GPU/collision variant has complete canonical field fidelity."),Covered)) return false;
        }
        FString After;
        if (!Test.TestTrue(TEXT("Section replay measures saved bytes without writing a replacement map."),
            FProjectWorldCanonicalLoader::ComputeFileSha256(Filename,After) && After==Before)) return false;
        Receipt.SetStringField(TEXT("saved_map_sha256"),Before);
        Receipt.SetNumberField(TEXT("original_instances_released"),Originals.Num());
        Receipt.SetArrayField(TEXT("variants"),Variants);
        for (const auto& Pair : ExternalHashes)
        {
            FString Hash;
            if (!Test.TestTrue(TEXT("Saved external actor/object bytes survive unload and reload unchanged."),
                FProjectWorldCanonicalLoader::ComputeFileSha256(FPackageName::LongPackageNameToFilename(
                    Pair.Key,FPackageName::GetAssetPackageExtension()),Hash) && Hash==Pair.Value)) return false;
        }
        Receipt.SetNumberField(TEXT("saved_external_packages"),ExternalHashes.Num());
        Receipt.SetNumberField(TEXT("reloaded_external_actors"),ExternalActorCount);
        if (ReloadObserver && !ReloadObserver(*World,Receipt)) return false;
        Receipt.SetStringField(TEXT("scope"),TEXT("Fresh native Editor map load; same GPU/collision/frame/seam and physical-probe checks; no builder, save, rendered Nanite, WP streaming or gameplay acceptance."));
        return !Test.HasAnyErrors();
    }
}
#endif
