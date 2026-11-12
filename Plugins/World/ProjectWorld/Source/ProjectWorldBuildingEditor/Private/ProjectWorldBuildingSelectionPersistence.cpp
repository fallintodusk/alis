#include "ProjectWorldBuildingSelection.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldGeneratedActors.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Materials/Material.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/Linker.h"
#include "UObject/SavePackage.h"
#include "Utilities/ProjectSha256.h"

namespace ProjectWorldBuildingSelection
{
    namespace
    {
        const FName AssemblyTag(TEXT("Project.BuildingAssembly.Generated=v1"));
        const FString FeaturePrefix(TEXT("Project.BuildingAssembly.Feature="));
        bool Feature(const AActor& A,FString& Id)
        {
            if (!A.Tags.Contains(AssemblyTag)) return false;
            for (const auto& Tag : A.Tags) if (Tag.ToString().StartsWith(FeaturePrefix)) {Id=Tag.ToString().RightChop(FeaturePrefix.Len());return true;}
            return false;
        }
        FString Key(const ProjectBuildingAssembly::FKit& Kit)
        {return ProjectBuildingAssembly::GeometryIdentity(TEXT("shared-kit"),{},Kit,FTransform::Identity);}
        FString Package(const FProjectWorldRealizationLayer& Layer,const ProjectBuildingAssembly::FKit& Kit)
        {return Layer.ArtifactRoot+TEXT("Kits/SK_")+Key(Kit);}
        bool Save(UPackage& P,FString& Error)
        {
            const FString File=FPackageName::LongPackageNameToFilename(P.GetName(),FPackageName::GetAssetPackageExtension());
            IFileManager::Get().MakeDirectory(*FPaths::GetPath(File),true);FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
            if (!UPackage::SavePackage(&P,nullptr,*File,Args)) {Error=TEXT("Building projection package could not be saved: ")+P.GetName();return false;}
            return true;
        }
        bool Modules(const FProjectWorldRealizationLayer& Layer,const ProjectBuildingAssembly::FKit& Kit,
            bool Create,TArray<UStaticMesh*>& Out,bool& Written,FString& Error)
        {
            Out.Reset();Written=false;const FString Name=Package(Layer,Kit);
            const FString File=FPackageName::LongPackageNameToFilename(Name,FPackageName::GetAssetPackageExtension());
            const bool Exists=FPaths::FileExists(File);auto* P=Exists ? LoadPackage(nullptr,*Name,LOAD_None) : nullptr;
            if (!Exists && Create) P=CreatePackage(*Name);
            if (!P) {Error=TEXT("Shared original kit package is unavailable.");return false;}
            for (int Part=0;Part<4;++Part)
            {
                const FName MeshName(*FString::Printf(TEXT("Module%d"),Part));auto* M=FindObject<UStaticMesh>(P,*MeshName.ToString());
                if (!Exists && Create)
                {
                    if (M) {Error=TEXT("Unexpected unsaved shared kit object.");return false;}
                    M=ProjectBuildingAssembly::CreateModule(*P,MeshName,static_cast<ProjectBuildingAssembly::EPart>(Part),Kit,
                        UMaterial::GetDefaultMaterial(MD_Surface),RF_Public|RF_Standalone);
                    if (M) FAssetRegistryModule::AssetCreated(M);
                }
                if (!ProjectBuildingAssembly::ModuleMatches(M,static_cast<ProjectBuildingAssembly::EPart>(Part),Kit))
                {Error=TEXT("Shared original kit differs from its admitted geometry.");return false;}
                Out.Add(M);
            }
            if (!Exists && Create) {if (!Save(*P,Error)) return false;Written=true;}
            return true;
        }
        FGuid Guid(const FProjectWorldCanonicalBundle& Bundle,const FString& Id)
        {return ProjectWorldGeneratedGeometry::StableGuid(Bundle.GridId+TEXT("|building-assembly|")+Id);}
        bool InputsMatch(const FProjectWorldLayerApplyContext& C,const FSet& Set,const FProjectWorldLayerInventory& I,FString& Error)
        {
            for (const auto& Cell : C.Inputs.Bundle.Cells)
            {
                FString Hash;if (!HashCell(C.Inputs,Set,Cell,Hash,Error)) return false;
                const auto* Expected=I.CanonicalInputs.FindByPredicate([&](const auto& V){return V.UnitId==Cell.CellId;});
                if (!Expected || Expected->Hash!=Hash) {Error=TEXT("Building selection inputs changed after dirty planning.");return false;}
            }
            return true;
        }
        bool Dirty(const FProjectWorldLayerApplyContext& C,const FString& Cell)
        {return C.FinalDirtyUnits.Contains(TEXT("*")) || C.FinalDirtyUnits.Contains(Cell);}
        UMaterialInterface* Material(const FProjectWorldLayerApplyContext& C)
        {const auto* M=C.PresentationMaterials.Find(TEXT("building"));return M ? *M : nullptr;}
        void Runtime(AActor& Actor,const FSelected& S,const FProjectWorldCanonicalBundle& Bundle)
        {
            Actor.Tags.AddUnique(ProjectWorldGeneratedGeometry::GeneratedTag);Actor.Tags.AddUnique(TEXT("ProjectWorld.BuildingMassing.v2"));
            Actor.Tags.AddUnique(FName(*(TEXT("ProjectWorld.BuildingCell=")+S.CellId)));
            Actor.Tags.AddUnique(FName(*(TEXT("ProjectWorld.Cell=")+S.CellId)));
            Actor.Tags.AddUnique(FName(*(TEXT("ProjectWorld.Grid=")+Bundle.GridId)));
            Actor.SetIsSpatiallyLoaded(true);
            Actor.bEnableAutoLODGeneration=false;Actor.SetHLODLayer(nullptr);
            Actor.GetRootComponent()->SetMobility(EComponentMobility::Static);
            TInlineComponentArray<UInstancedStaticMeshComponent*> Parts(&Actor);
            for (auto* Part : Parts) {Part->SetMobility(EComponentMobility::Static);Part->SetCanEverAffectNavigation(false);}
        }
    }
    bool Apply(const FProjectWorldLayerApplyContext& C,const FSet& Set,FProjectWorldLayerInventory& I,
        FProjectWorldRealizationResult& Result,FString& Error)
    {
        TArray<AActor*> Retire;TMap<FString,UPackage*> PreviousKits;
        for (TActorIterator<AActor> It(&C.World);It;++It)
        {
            FString Id;if (!Feature(**It,Id)) continue;
            TInlineComponentArray<UInstancedStaticMeshComponent*> Parts(*It);
            for (auto* Part : Parts) if (UStaticMesh* Mesh=Part->GetStaticMesh())
            {
                auto* P=Mesh->GetOutermost();
                if (P->GetName().StartsWith(C.Inputs.Layer.ArtifactRoot+TEXT("Kits/"))) PreviousKits.Add(P->GetName(),P);
            }
            if (!Set.Buildings.ContainsByPredicate([&](const auto& S){return S.FeatureId==Id;})) Retire.Add(*It);
        }
        if (!Set.bConfigured && Set.Buildings.IsEmpty() && Retire.IsEmpty()) return true;
        if (!InputsMatch(C,Set,I,Error) || !Material(C)) return false;
        // Qualify every selection before creating assets or suppressing any massing.
        TArray<ProjectWorldBuildingAssembly::FPrepared> Plans;
        for (const auto& S : Set.Buildings)
        {auto& P=Plans.AddDefaulted_GetRef();if (!Prepare(C.Inputs,S,P,Error)) return false;}
        for (int Index=0;Index<Set.Buildings.Num();++Index)
        {
            const auto& S=Set.Buildings[Index];bool Existing=false;
            for (TActorIterator<AActor> It(&C.World);It;++It) {FString Id;if (Feature(**It,Id) && Id==S.FeatureId) Existing=true;}
            // An explicit cold reconstruction can retain identical inputs while its actors are absent.
            if (!Dirty(C,S.CellId) && Existing) continue;
            TArray<UStaticMesh*> Shared;bool Written=false;
            if (!Modules(C.Inputs.Layer,S.Kit,true,Shared,Written,Error)) return false;
            I.Metrics.FindOrAdd(TEXT("kit_packages_written"))+=Written;
            ProjectBuildingAssembly::FNativeOptions O;O.Modules=Shared;O.ActorGuid=Guid(C.Inputs.Bundle,S.FeatureId);
            O.ActorName=FName(*(TEXT("ProjectWorld_Assembly_")+O.ActorGuid.ToString(EGuidFormats::Digits)));O.bExternalActor=true;
            auto& P=Plans[Index];AActor* Actor=nullptr;bool Reused=false;
            if (!ProjectBuildingAssembly::Apply(C.World,S.FeatureId,P.Plan,S.Kit,P.Transform,Material(C),Actor,Reused,Error,O)) return false;
            if (Reused) continue;
            Runtime(*Actor,S,C.Inputs.Bundle);Actor->MarkPackageDirty();
            // The map owner rehomes external packages when assigning a new map its final name.
            const bool TemporaryMap=C.World.PersistentLevel->GetPackage()->GetName().StartsWith(TEXT("/Temp/"));
            if (!TemporaryMap && !Save(*Actor->GetExternalPackage(),Error)) return false;
            if (Existing) ++Result.UpdatedActorCount;else ++Result.CreatedActorCount;
            if (!TemporaryMap) ++Result.SelfSavedActorMutationCount;
            ++I.Metrics.FindOrAdd(TEXT("assembly_actors_written"));
            I.Metrics.FindOrAdd(TEXT("assembly_instances_written"))+=P.Plan.Instances.size();
        }
        for (auto* A : Retire)
        {
            if (!A->IsPackageExternal() || !A->Tags.Contains(ProjectWorldGeneratedGeometry::GeneratedTag) ||
                !A->Tags.Contains(TEXT("ProjectWorld.BuildingMassing.v2")))
            {Error=TEXT("Deselected assembly is outside registered persistent ownership.");return false;}
            UPackage* ActorPackage=A->GetExternalPackage();const FString PackageName=ActorPackage->GetName();
            if (!C.World.EditorDestroyActor(A,true)) {Error=TEXT("Cannot retire deselected assembly actor.");return false;}
            ResetLoaders(ActorPackage);
            const FString File=FPackageName::LongPackageNameToFilename(PackageName,FPackageName::GetAssetPackageExtension());
            if (FPaths::FileExists(File) && !IFileManager::Get().Delete(*File,false,true)) {Error=TEXT("Cannot retire deselected assembly package.");return false;}
            ++Result.RemovedActorCount;++Result.SelfSavedActorMutationCount;
        }
        TSet<FString> LiveKits;for (const auto& S : Set.Buildings) LiveKits.Add(Package(C.Inputs.Layer,S.Kit));
        for (const auto& Old : PreviousKits) if (!LiveKits.Contains(Old.Key))
        {
            ResetLoaders(Old.Value);
            const FString File=FPackageName::LongPackageNameToFilename(Old.Key,FPackageName::GetAssetPackageExtension());
            if (FPaths::FileExists(File) && !IFileManager::Get().Delete(*File,false,true))
            {Error=TEXT("Cannot retire the unused owned kit package.");return false;}
            ++I.Metrics.FindOrAdd(TEXT("kit_packages_retired"));
        }
        return true;
    }
    bool Capture(const FProjectWorldLayerApplyContext& C,const FSet& Set,FProjectWorldLayerInventory& I,
        FProjectWorldRealizationResult&,FString& Error)
    {
        TSet<FString> Seen,Packages;int64 Instances=0;
        for (TActorIterator<AActor> It(&C.World);It;++It)
        {
            FString Id;if (!Feature(**It,Id)) continue;
            const auto* S=Set.Buildings.FindByPredicate([&](const auto& V){return V.FeatureId==Id;});
            if (!S || Seen.Contains(Id)) {Error=TEXT("Assembly actor selection is extra or duplicated.");return false;}
            ProjectWorldBuildingAssembly::FPrepared P;TArray<UStaticMesh*> Shared;bool Written=false;
            if (!Prepare(C.Inputs,*S,P,Error) || !Modules(C.Inputs.Layer,S->Kit,false,Shared,Written,Error)) return false;
            const FString Identity=ProjectBuildingAssembly::GeometryIdentity(Id,P.Plan,S->Kit,P.Transform);
            if (It->GetActorGuid()!=Guid(C.Inputs.Bundle,Id) || !It->IsPackageExternal() || !It->GetIsSpatiallyLoaded() ||
                It->bEnableAutoLODGeneration || It->GetHLODLayer() ||
                !ProjectBuildingAssembly::Matches(**It,Identity,P.Plan,S->Kit,P.Transform,Material(C),Shared))
            {Error=TEXT("Saved assembly differs from admitted geometry, shared assets or spatial ownership.");return false;}
            TInlineComponentArray<UInstancedStaticMeshComponent*> Parts(*It);
            for (auto* Part : Parts) if (Part->Mobility!=EComponentMobility::Static || Part->CanEverAffectNavigation())
            {Error=TEXT("Assembly component has unsupported mobility or navigation.");return false;}
            const FVector Roof=P.Transform.TransformPosition(FVector(P.Input.Width/2,P.Input.Depth/2,P.Input.Height)*100);
            FHitResult Hit;
            if (!C.World.LineTraceSingleByChannel(Hit,Roof+FVector(0,0,100),Roof-FVector(0,0,100),ECC_Visibility) ||
                Hit.GetActor()!=*It || FMath::Abs(Hit.ImpactPoint.Z-Roof.Z)>.05)
            {Error=TEXT("Shared assembly roof does not retain its admitted physical support.");return false;}
            FString Semantic;FProjectSha256::HashNormalizedText(Identity,Semantic);
            if (!ProjectWorldLayerProducerRegistry::AddPackageArtifact(It->GetPackage()->GetName(),TEXT("external_actor"),Semantic,I,Error)) return false;
            const FString KitPackage=Package(C.Inputs.Layer,S->Kit);
            if (!Packages.Contains(KitPackage))
            {
                FProjectSha256::HashNormalizedText(Key(S->Kit),Semantic);
                if (!ProjectWorldLayerProducerRegistry::AddPackageArtifact(KitPackage,TEXT("asset"),Semantic,I,Error)) return false;
                Packages.Add(KitPackage);
            }
            Seen.Add(Id);Instances+=P.Plan.Instances.size();
        }
        if (Seen.Num()!=Set.Buildings.Num()) {Error=TEXT("Admitted assembly is missing; blockout suppression cannot commit.");return false;}
        I.Metrics.Add(TEXT("assembly_actors"),Seen.Num());I.Metrics.Add(TEXT("assembly_instances"),Instances);
        I.Metrics.Add(TEXT("shared_kit_packages"),Packages.Num());I.Metrics.Add(TEXT("shared_kit_meshes"),Packages.Num()*4);
        return (!Set.bConfigured && Set.Buildings.IsEmpty()) || InputsMatch(C,Set,I,Error);
    }
}
