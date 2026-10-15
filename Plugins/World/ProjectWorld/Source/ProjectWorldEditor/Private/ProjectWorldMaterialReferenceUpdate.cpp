// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldMaterialReferenceUpdate.h"
#include "ProjectWorldLayerProducerRegistry.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Utilities/ProjectSha256.h"

namespace ProjectWorldMaterialReferencePrivate
{
bool PackagePath(const UPackage* Package, FString& Relative)
{
    const FString Root=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    Relative=FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(Package->GetName(),FPackageName::GetAssetPackageExtension()));
    if (!FPaths::MakePathRelativeTo(Relative,*Root) || Relative.StartsWith(TEXT(".."))) return false;
    Relative.ReplaceInline(TEXT("\\"),TEXT("/")); return true;
}
bool Save(UPackage* Package, UObject* Asset, FString& Error)
{
    const FString File=FPackageName::LongPackageNameToFilename(Package->GetName(),FPackageName::GetAssetPackageExtension());
    FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone;
    if (!UPackage::SavePackage(Package,Asset,*File,Args)) {Error=TEXT("Material-reference package save failed: ")+File; return false;}
    return true;
}
}

bool IProjectWorldLayerProducer::UpdateMaterialReferences(const FProjectWorldLayerApplyContext& Context,
    const TArray<FProjectWorldLayerArtifactInventory>& Accepted, FProjectWorldLayerInventory& Inventory,
    FProjectWorldRealizationResult& Result, bool bApply, FString& Error) const
{
    return ProjectWorldMaterialReferenceUpdate::Update(*this,Context,Accepted,Inventory,Result,bApply,Error);
}

bool ProjectWorldMaterialReferenceUpdate::Update(const IProjectWorldLayerProducer& Producer,
    const FProjectWorldLayerApplyContext& Context, const TArray<FProjectWorldLayerArtifactInventory>& Accepted,
    FProjectWorldLayerInventory& Inventory, FProjectWorldRealizationResult& Result, bool bApply, FString& Error)
{
    using namespace ProjectWorldMaterialReferencePrivate;
    const auto& Declaration=Producer.GetDeclaration();
    UMaterialInterface* const* Selected=Context.PresentationMaterials.Find(Declaration.PresentationMaterialRole);
    if (Declaration.PresentationMaterialRole.IsNone() || !Selected || !*Selected || (*Selected)->GetOutermost()->IsDirty())
    {Error=TEXT("Owner has no authenticated material-reference operation or selected clean material."); return false;}
    TMap<FString,const FProjectWorldLayerArtifactInventory*> Membership;
    for (const auto& A : Accepted)
    {
        FString Digest;
        if (Membership.Contains(A.Path) || !FProjectSha256::HashFile(FPaths::ProjectDir()/A.Path,Digest) || Digest!=A.Digest)
        {Error=TEXT("Prior material membership is missing, duplicate or drifted: ")+A.Path; return false;}
        Membership.Add(A.Path,&A);
    }
    struct FTarget {AStaticMeshActor* Actor; UStaticMesh* Mesh; FString Geometry;};
    TArray<FTarget> Targets; TSet<FString> Seen;
    for (TActorIterator<AStaticMeshActor> I(&Context.World); I; ++I)
    {
        bool Owned=true; for (FName Tag : Declaration.OwnedActors.OwnedTags) Owned &= I->Tags.Contains(Tag);
        if (!Owned) continue;
        auto* Component=I->GetStaticMeshComponent(); UStaticMesh* Mesh=Component ? Component->GetStaticMesh().Get() : nullptr;
        FString ActorPath,MeshPath,Geometry;
        if (!Mesh || !PackagePath(I->GetPackage(),ActorPath) || !PackagePath(Mesh->GetOutermost(),MeshPath) ||
            !Mesh->GetOutermost()->GetName().StartsWith(Context.Inputs.Layer.ArtifactRoot) ||
            !Membership.Contains(ActorPath) || !Membership.Contains(MeshPath) || Seen.Contains(ActorPath) || Seen.Contains(MeshPath) ||
            Membership[ActorPath]->Kind!=TEXT("external_actor") || Membership[MeshPath]->Kind!=TEXT("asset") ||
            Mesh->GetStaticMaterials().Num()!=1 || Component->GetNumMaterials()!=1 ||
            I->GetPackage()->IsDirty() || Mesh->GetOutermost()->IsDirty() || !HashGeometry(*Mesh,Geometry,Error))
        {if (Error.IsEmpty()) Error=TEXT("Material-reference actor/mesh differs from accepted owner membership."); return false;}
        Seen.Add(ActorPath); Seen.Add(MeshPath); Targets.Add({*I,Mesh,Geometry});
    }
    for (const auto& Pair : Membership)
    {
        if (Seen.Contains(Pair.Key)) continue;
        FString Package;
        if (Pair.Value->Kind!=TEXT("asset") || !FPackageName::TryConvertFilenameToLongPackageName(FPaths::ProjectDir()/Pair.Key,Package))
        {Error=TEXT("Accepted actor is absent from material-reference ownership census."); return false;}
        const FString Name=FPackageName::GetShortName(Package);
        auto* Diagnostic=LoadObject<UMaterialInterface>(nullptr,*(Package+TEXT(".")+Name));
        if (!Package.StartsWith(Context.Inputs.Layer.ArtifactRoot) || !Diagnostic || Diagnostic->GetOutermost()->IsDirty())
        {Error=TEXT("Unknown unconsumed artifact cannot be adopted as material-only output."); return false;}
    }
    if (!bApply) return true;
    int64 Builds=0; TArray<TPair<UStaticMesh*,FDelegateHandle>> Handles;
    for (const auto& T : Targets) Handles.Emplace(T.Mesh,T.Mesh->OnPreMeshBuild().AddLambda([&Builds](UStaticMesh*){++Builds;}));
    ON_SCOPE_EXIT {for (const auto& Handle : Handles) Handle.Key->OnPreMeshBuild().Remove(Handle.Value);};
    for (const auto& T : Targets)
    {
        auto* Component=T.Actor->GetStaticMeshComponent();
        const FTransform Transform=T.Actor->GetActorTransform(); const auto Tags=T.Actor->Tags;
        const bool SavedChange=T.Mesh->GetStaticMaterials()[0].MaterialInterface!=*Selected;
        const bool OverrideChange=Component->OverrideMaterials.IsValidIndex(0) && Component->OverrideMaterials[0] && Component->OverrideMaterials[0]!=*Selected;
        if (SavedChange)
        {
            // The native setter rebuilds meshes and physics. This operation owns only
            // the existing slot reference; retain every slot/build/collision property.
            T.Mesh->GetStaticMaterials()[0].MaterialInterface=*Selected;
            T.Mesh->MarkPackageDirty();
            if (!Save(T.Mesh->GetOutermost(),T.Mesh,Error)) return false;
        }
        if (OverrideChange)
        {
            Component->SetMaterial(0,*Selected); T.Actor->GetPackage()->MarkPackageDirty();
            if (!Save(T.Actor->GetPackage(),nullptr,Error)) return false;
            ++Result.SelfSavedActorMutationCount;
        }
        FString After;
        if (Builds || !HashGeometry(*T.Mesh,After,Error) || After!=T.Geometry ||
            !T.Actor->GetActorTransform().Equals(Transform) || T.Actor->Tags!=Tags || Component->GetMaterial(0)!=*Selected)
        {Error=TEXT("Material-only update changed geometry, invoked a mesh build or failed effective binding verification."); return false;}
        Inventory.Metrics.FindOrAdd(TEXT("material_mesh_writes"))+=SavedChange ? 1 : 0;
        Inventory.Metrics.FindOrAdd(TEXT("material_actor_writes"))+=OverrideChange ? 1 : 0;
        FString MeshPath; PackagePath(T.Mesh->GetOutermost(),MeshPath);
        Inventory.MaterialGeometryInputs.Add({MeshPath,T.Geometry});
    }
    Inventory.Artifacts=Accepted;
	Inventory.MaterialReferenceRole=Declaration.PresentationMaterialRole;
    for (auto& A : Inventory.Artifacts)
        if (!FProjectSha256::HashFile(FPaths::ProjectDir()/A.Path,A.Digest)) {Error=TEXT("Cannot authenticate saved material output."); return false;}
    Inventory.Metrics.Add(TEXT("material_geometry_checks"),Targets.Num());
    Inventory.Metrics.Add(TEXT("material_mesh_build_events"),Builds);
    Inventory.PostApplyBuilder.Reset();
    return true;
}
