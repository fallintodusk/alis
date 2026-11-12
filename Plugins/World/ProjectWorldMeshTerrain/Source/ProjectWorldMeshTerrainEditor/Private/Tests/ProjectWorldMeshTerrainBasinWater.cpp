// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldMeshTerrainBasinScene.h"
#include "ProjectWorldAuthoredOverlay.h"
#include "ProjectWorldLayerProducerRegistry.h"
#include "Tests/ProjectWorldBasinFixture.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "StaticMeshResources.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldBasinSceneTest
{
    namespace
    {
        bool SameMaterial(const FJsonObject& Frozen,const FJsonObject& Current)
        {
            for (const auto* Key : {TEXT("binding_id"),TEXT("object_path"),TEXT("package_sha256"),TEXT("semantic_identity"),TEXT("manifest_sha256")})
            {FString A,B;if (!Frozen.TryGetStringField(Key,A) || !Current.TryGetStringField(Key,B) || A.IsEmpty() || A!=B) return false;}
            return true;
        }
        TArray<TSharedPtr<FJsonValue>> Numbers(std::initializer_list<double> Values)
        {
            TArray<TSharedPtr<FJsonValue>> Out;for (double V : Values) Out.Add(MakeShared<FJsonValueNumber>(V));return Out;
        }
        bool Observe(FAutomationTestBase& Test,UWorld* World,const FInput& Input,const TSharedPtr<FJsonObject>& Expected,TSharedPtr<FJsonObject>& Out,FString& Error)
        {
            Out.Reset();Error.Reset();
            AStaticMeshActor* Actor=nullptr;int32 Count=0;
            for (TActorIterator<AStaticMeshActor> It(World);It;++It) if (It->ActorHasTag(TEXT("ProjectWorld.Water.v1"))) {Actor=*It;++Count;}
            if (!Test.TestEqual(TEXT("The complete joint basin has exactly one actual Water actor."),Count,1)) return false;
            auto* Component=Actor->GetStaticMeshComponent();UStaticMesh* Mesh=Component ? Component->GetStaticMesh() : nullptr;
            UMaterialInterface* Material=nullptr;TSharedPtr<FJsonObject> MaterialIdentity;
            if (!ProjectWorldBasinFixture::ResolveWaterMaterial(Material,MaterialIdentity,Error)) {Test.AddError(Error);return false;}
            const TSharedPtr<FJsonObject>* FrozenMaterial=nullptr;
            if (Expected && (!Expected->TryGetObjectField(TEXT("material"),FrozenMaterial) || !FrozenMaterial || !FrozenMaterial->IsValid() ||
                !SameMaterial(**FrozenMaterial,*MaterialIdentity)))
            {Error=TEXT("Water replay refuses stale frozen material identity.");return false;}
            if (!Test.TestTrue(TEXT("Actual Water retains accepted material, identity frame and its nonphysical spatial role."),
                Mesh && Mesh->GetRenderData() && Mesh->GetRenderData()->LODResources.Num()==1 &&
                Actor->GetActorTransform().Equals(FTransform::Identity,0) && Component->GetComponentTransform().Equals(FTransform::Identity,0) &&
                Actor->GetIsSpatiallyLoaded() && !Actor->bEnableAutoLODGeneration && !Actor->GetHLODLayer() &&
                Component->GetCollisionEnabled()==ECollisionEnabled::NoCollision && !Component->CanEverAffectNavigation() &&
                !Mesh->bHasNavigationData && !Mesh->GetNaniteSettings().bEnabled && !Mesh->HasValidNaniteData() &&
                Mesh->GetStaticMaterials().Num()==1 && Mesh->GetStaticMaterials()[0].MaterialInterface==Material && Component->GetMaterial(0)==Material)) return false;
            const FString SourcePackage=Expected ? Expected->GetStringField(TEXT("actor_package")) : Actor->GetPackage()->GetName();
            if (!Test.TestTrue(TEXT("Water mesh and actor have exact saved scene ownership."),
                Mesh->GetOutermost()->GetName().StartsWith(Input.Mount) &&
                FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(Mesh->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension())),Input.Directory) &&
                (!Expected || (OwnsSavedActor(Actor,World,Input,SourcePackage) &&
                    Mesh->GetOutermost()->GetName()==Expected->GetStringField(TEXT("mesh_package")))))) return false;
            const auto& LOD=Mesh->GetRenderData()->LODResources[0];
            const uint32 Vertices=LOD.VertexBuffers.PositionVertexBuffer.GetNumVertices(),Indices=LOD.IndexBuffer.GetNumIndices(),Stride=LOD.IndexBuffer.Is32Bit() ? 4 : 2;
            TArray<uint8> PositionBytes,IndexBytes;
            if (!Test.TestTrue(TEXT("Actual saved Water has complete readable non-Nanite GPU geometry."),Vertices==45 && Indices==43*3 &&
                LOD.VertexBuffers.PositionVertexBuffer.GetStride()==sizeof(FVector3f) &&
                ProjectWorldCompositeTerrainTests::ReadBuffer(LOD.VertexBuffers.PositionVertexBuffer.VertexBufferRHI.GetReference(),Vertices*sizeof(FVector3f),PositionBytes) &&
                ProjectWorldCompositeTerrainTests::ReadBuffer(LOD.IndexBuffer.IndexBufferRHI.GetReference(),Indices*Stride,IndexBytes))) return false;
            const auto& Feature=Input.Bundle.Features.FindChecked(Input.Bundle.Cells[0].OwnedFeatureIds[0]);
            TSet<FVector3f> Source,Actual;
            for (const auto& P : Feature.GeometryPolygons[0].Outer)
                Source.Add(FVector3f(FProjectWorldCanonicalLoader::CanonicalToUnreal(Input.Bundle,FVector(P,Feature.WaterSurface.LevelMeters))));
            TArray<TSharedPtr<FJsonValue>> Points,Faces;
            for (uint32 I=0;I<Vertices;++I)
            {
                FVector3f P;FMemory::Memcpy(&P,PositionBytes.GetData()+I*sizeof(P),sizeof(P));Actual.Add(P);
                Points.Add(MakeShared<FJsonValueArray>(Numbers({P.X,P.Y,P.Z})));
                if (Expected)
                {
                    const auto& E=Expected->GetArrayField(TEXT("world_vertices_cm"))[I]->AsArray();
                    if (!Test.TestTrue(TEXT("Every actual Water GPU vertex survives saved reload and PIE exactly."),P.X==E[0]->AsNumber() && P.Y==E[1]->AsNumber() && P.Z==E[2]->AsNumber())) return false;
                }
            }
            if (!Test.TestTrue(TEXT("The actual zero-lift stage uses every original common-ring XY after its declared binary32 encoding."),
                Actual.Num()==45 && Source.Num()==45 && Actual.Includes(Source) && Source.Includes(Actual))) return false;
            for (uint32 I=0;I<Indices;I+=3)
            {
                uint32 IDs[3];
                for (uint32 J=0;J<3;++J)
                {
                    if (Stride==4) FMemory::Memcpy(&IDs[J],IndexBytes.GetData()+(I+J)*Stride,4);
                    else {uint16 ID;FMemory::Memcpy(&ID,IndexBytes.GetData()+(I+J)*Stride,2);IDs[J]=ID;}
                    if (!Test.TestTrue(TEXT("Every actual Water index stays inside the complete buffer."),IDs[J]<Vertices)) return false;
                    if (Expected && !Test.TestEqual(TEXT("Every oriented Water index survives saved reload and PIE exactly."),IDs[J],
                        uint32(Expected->GetArrayField(TEXT("triangles"))[I/3]->AsArray()[J]->AsNumber()))) return false;
                }
                Faces.Add(MakeShared<FJsonValueArray>(Numbers({double(IDs[0]),double(IDs[1]),double(IDs[2])})));
            }
            FString MeshSha;
            if (!Test.TestTrue(TEXT("The actual Water mesh package remains saved and byte-authenticated."),FProjectWorldCanonicalLoader::ComputeFileSha256(
                FPackageName::LongPackageNameToFilename(Mesh->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),MeshSha) &&
                (!Expected || MeshSha==Expected->GetStringField(TEXT("mesh_sha256"))))) return false;
            Out=MakeShared<FJsonObject>();Out->SetStringField(TEXT("actor_package"),SourcePackage);
            Out->SetStringField(TEXT("actual_actor_instance_package"),Actor->GetPackage()->GetName());
            Out->SetStringField(TEXT("mesh_package"),Mesh->GetOutermost()->GetName());Out->SetStringField(TEXT("mesh_sha256"),MeshSha);
            Out->SetObjectField(TEXT("material"),MaterialIdentity);Out->SetArrayField(TEXT("world_vertices_cm"),Points);Out->SetArrayField(TEXT("triangles"),Faces);
            return true;
        }
    }
    bool ApplyJointWater(FAutomationTestBase& Test,UWorld* World,const FJsonObject& Document,FInput& Input)
    {
        FString Error;UMaterialInterface* Material=nullptr;TSharedPtr<FJsonObject> Identity;
        if (!ProjectWorldBasinFixture::CheckRefusals(Document,Input.Bundle,Test)) return false;
        if (!ProjectWorldBasinFixture::AddWater(Document,Input.Bundle,Error) ||
            !ProjectWorldBasinFixture::ResolveWaterMaterial(Material,Identity,Error)) {Test.AddError(Error);return false;}
        const auto* Producer=ProjectWorldLayerProducerRegistry::Find(TEXT("project_water_mesh"),1,Error);
        if (!Producer) {Test.AddError(Error);return false;}
        FProjectWorldRealizationLayer Layer;Layer.LayerId=TEXT("identified_basin_water");Layer.GeneratorId=TEXT("project_water_mesh");Layer.GeneratorVersion=1;
        Layer.ArtifactRoot=Input.Mount+TEXT("Water/");Layer.CanonicalSelectors={TEXT("water")};Layer.SpatialOwnership=TEXT("cell_local");
        Layer.DirtyGranularity=EProjectWorldDirtyGranularity::CanonicalCell;Layer.RuntimeMapping=TEXT("world_partition_spatial");
        Layer.NormalizedSettings=TEXT("{\"material_shading_model\":\"solid_opaque\",\"nanite\":false,\"surface_offset_m\":0.25}");
        if (!ProjectWorldLayerProducerRegistry::ValidateLayer(Layer,Error)) {Test.AddError(Error);return false;}
        FProjectWorldRealizationProfile Profile;Profile.Layers={Layer};FProjectWorldAuthoredOverlaySet Overlays;
        FProjectWorldLayerInputs Inputs{Input.Bundle,Profile,Profile.Layers[0],Overlays};FProjectWorldLayerUnitInputs Units;
        if (!Producer->HashUnitInputs(Inputs,Units,Error)) {Test.AddError(Error);return false;}
        if (!Test.TestTrue(TEXT("The Water dependency is exactly the one complete selected cell."),Units.HashByUnit.Num()==1 && Units.HashByUnit.Contains(Input.Bundle.Cells[0].CellId))) return false;
        FProjectWorldRealizationResult Result;auto& Inventory=Result.LayerInventories.AddDefaulted_GetRef();
        Inventory.LayerId=Layer.LayerId;Inventory.GeneratorId=Layer.GeneratorId;Inventory.GeneratorVersion=1;
        Inventory.ArtifactRoot=Layer.ArtifactRoot;Inventory.FinalDirtyUnits={Input.Bundle.Cells[0].CellId};
        const TMap<FName,UMaterialInterface*> Materials{{TEXT("water"),Material}};
        FProjectWorldLayerApplyContext Context{*World,Inputs,Inventory.FinalDirtyUnits,Materials};
        if (!Producer->Apply(Context,Inventory,Result,Error) || !Producer->CaptureArtifacts(Context,Inventory,Result,Error)) {Test.AddError(Error);return false;}
        if (!Test.TestEqual(TEXT("The real Water producer emits exactly the complete43-face basin."),Result.GetProducerMetric(TEXT("project_water_mesh"),TEXT("triangles_written")),int64(43))) return false;
        for (const auto& Artifact : Inventory.Artifacts)
        {
            FString Package;
            if (!Test.TestTrue(TEXT("Every Water artifact resolves to its exact scratch package."),
                FPackageName::TryConvertFilenameToLongPackageName(FPaths::ProjectDir()/Artifact.Path,Package) && Package.StartsWith(Input.Mount))) return false;
            Input.Packages.AddUnique(Package);
        }
        if (!Observe(Test,World,Input,nullptr,Input.Expected,Error)) {if (!Error.IsEmpty()) Test.AddError(Error);return false;}return true;
    }
    bool MeasureJointWater(FAutomationTestBase& Test,UWorld* World,const FInput& Input,const FString& Prefix,FJsonObject& Receipt)
    {
        TSharedPtr<FJsonObject> Actual,Refused;FString Error;const auto Expected=Input.Expected->GetObjectField(TEXT("water"));
        auto Stale=MakeShared<FJsonObject>(*Expected);auto StaleMaterial=MakeShared<FJsonObject>(*Expected->GetObjectField(TEXT("material")));
        StaleMaterial->SetStringField(TEXT("semantic_identity"),TEXT("stale"));Stale->SetObjectField(TEXT("material"),StaleMaterial);
        if (!Test.TestTrue(TEXT("Actual saved/PIE Water replay refuses stale frozen material before reading geometry."),
            !Observe(Test,World,Input,Stale,Refused,Error) && Error==TEXT("Water replay refuses stale frozen material identity.") && !Refused.IsValid())) return false;
        if (!Observe(Test,World,Input,Expected,Actual,Error)) {if (!Error.IsEmpty()) Test.AddError(Error);return false;}
        Receipt.SetObjectField(Prefix+TEXT("water"),Actual);return true;
    }
    void TrackWaterInstances(UWorld* World,TArray<TWeakObjectPtr<UObject>>& Instances)
    {
        for (TActorIterator<AStaticMeshActor> It(World);It;++It) if (It->ActorHasTag(TEXT("ProjectWorld.Water.v1")))
        {Instances.Add(*It);Instances.Add(It->GetStaticMeshComponent()->GetStaticMesh());}
    }
}
#endif
