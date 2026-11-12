// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldMeshTerrainCoastalFixture.h"
#include "ProjectWorldMeshTerrainStorageFixture.h"
#include "ProjectWorldAuthoredOverlay.h"
#include "ProjectWorldLayerProducerRegistry.h"
#include "Tests/ProjectWorldBasinFixture.h"
#include "MeshPartitionDefinition.h"
#include "MeshPartitionTransformerPipeline.h"
#include "MeshPartitionWPActorPropertiesTransformer.h"
#include "MeshPartitionCompiledSection.h"
#include "MeshPartitionMeshBuilder.h"
#include "MeshPartitionMeshData.h"
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
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "StaticMeshResources.h"
#include "Serialization/JsonSerializer.h"
#include "Utilities/ProjectSha256.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldCoastalSceneTest
{
    namespace
    {
        bool ObserveWater(FAutomationTestBase& Test,UWorld& World,const ProjectWorldBasinSceneTest::FInput& Input,
            const FJsonObject* Expected,TSharedPtr<FJsonObject>& Out)
        {
            Out.Reset();TArray<TSharedPtr<FJsonValue>> Actors;TSet<FString> Seen;FString Error;
            UMaterialInterface* Material=nullptr;TSharedPtr<FJsonObject> Identity;
            if (!ProjectWorldBasinFixture::ResolveWaterMaterial(Material,Identity,Error)) {Test.AddError(Error);return false;}
            for (TActorIterator<AStaticMeshActor> It(&World);It;++It)
            {
                if (!It->ActorHasTag(TEXT("ProjectWorld.Water.v1"))) continue;auto* C=It->GetStaticMeshComponent();UStaticMesh* Mesh=C ? C->GetStaticMesh() : nullptr;
                if (!Test.TestTrue(TEXT("Saved ordinary Water retains material, spatial ownership and nonphysical geometry."),
                    Mesh && Mesh->GetRenderData() && Mesh->GetRenderData()->LODResources.Num()==1 && C->GetMaterial(0) && C->GetMaterial(0)==Material &&
                    C->GetCollisionEnabled()==ECollisionEnabled::NoCollision && !C->CanEverAffectNavigation() && !Mesh->bHasNavigationData &&
                    !Mesh->HasValidNaniteData() && !Mesh->GetNaniteSettings().bEnabled && It->GetIsSpatiallyLoaded() && !It->bEnableAutoLODGeneration && !It->GetHLODLayer())) return false;
                const TSharedPtr<FJsonObject> Frozen=Expected ? [&](){for (const auto& V : Expected->GetArrayField(TEXT("actors")))
                    if (V->AsObject()->GetStringField(TEXT("mesh"))==Mesh->GetPathName()) return V->AsObject();return TSharedPtr<FJsonObject>();}() : nullptr;
                const FString SourcePackage=Expected && Frozen ? Frozen->GetStringField(TEXT("actor_package")) : It->GetPackage()->GetName();
                if (!Test.TestTrue(TEXT("Water actor binds a unique saved source package and exact mesh."),
                    !Seen.Contains(SourcePackage) && Mesh->GetOutermost()->GetName().StartsWith(Input.Mount) &&
                    (!Expected || (Frozen && ProjectWorldBasinSceneTest::OwnsSavedActor(*It,&World,Input,SourcePackage))))) return false;
                Seen.Add(SourcePackage);const auto& LOD=Mesh->GetRenderData()->LODResources[0];const uint32 Count=LOD.VertexBuffers.PositionVertexBuffer.GetNumVertices();
                const uint32 Indices=LOD.IndexBuffer.GetNumIndices(),Stride=LOD.IndexBuffer.Is32Bit() ? 4 : 2;TArray<uint8> Positions,IndexBytes;
                if (!Test.TestTrue(TEXT("Water exposes its complete actual GPU field."),Count>0 && Indices>0 && Indices%3==0 &&
                    LOD.VertexBuffers.PositionVertexBuffer.GetStride()==sizeof(FVector3f) &&
                    ProjectWorldCompositeTerrainTests::ReadBuffer(LOD.VertexBuffers.PositionVertexBuffer.VertexBufferRHI.GetReference(),Count*sizeof(FVector3f),Positions) &&
                    ProjectWorldCompositeTerrainTests::ReadBuffer(LOD.IndexBuffer.IndexBufferRHI.GetReference(),Indices*Stride,IndexBytes))) return false;
                auto Field=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Vertices,Faces;
                for (uint32 I=0;I<Count;++I)
                {
                    FVector3f Local;FMemory::Memcpy(&Local,Positions.GetData()+I*sizeof(Local),sizeof(Local));const FVector P=C->GetComponentTransform().TransformPosition(FVector(Local));
                    if (!Test.TestTrue(TEXT("Every ordinary Water vertex retains canonical0 plus .25m presentation."),
                        FMath::Abs(P.Z-(.25-Input.Bundle.HeightOriginMeters)*100)<=.01)) return false;
                    TArray<TSharedPtr<FJsonValue>> Row;for (double V : {P.X,P.Y,P.Z}) Row.Add(MakeShared<FJsonValueNumber>(V));Vertices.Add(MakeShared<FJsonValueArray>(Row));
                }
                for (uint32 I=0;I<Indices;I+=3)
                {
                    TArray<TSharedPtr<FJsonValue>> Row;
                    for (uint32 K=0;K<3;++K)
                    {uint32 Id=0;if (Stride==4) FMemory::Memcpy(&Id,IndexBytes.GetData()+(I+K)*Stride,4);
                        else {uint16 V;FMemory::Memcpy(&V,IndexBytes.GetData()+(I+K)*Stride,2);Id=V;}
                        if (!Test.TestTrue(TEXT("Water indices remain in the exact vertex inventory."),Id<Count)) return false;Row.Add(MakeShared<FJsonValueNumber>(Id));}
                    Faces.Add(MakeShared<FJsonValueArray>(Row));
                }
                Field->SetArrayField(TEXT("world_vertices_cm"),Vertices);Field->SetArrayField(TEXT("triangles"),Faces);FString Text,GeometryHash,FileHash;
                FJsonSerializer::Serialize(Field,TJsonWriterFactory<>::Create(&Text));FTCHARToUTF8 Utf8(*Text);TArray<uint8> Bytes;Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()),Utf8.Length());
                if (!Test.TestTrue(TEXT("Water saved asset and complete oriented GPU field remain authenticated."),
                    FProjectSha256::HashBuffer(Bytes,GeometryHash) && FProjectWorldCanonicalLoader::ComputeFileSha256(FPackageName::LongPackageNameToFilename(
                    Mesh->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),FileHash) && (!Frozen ||
                    (GeometryHash==Frozen->GetStringField(TEXT("geometry_sha256")) && FileHash==Frozen->GetStringField(TEXT("asset_sha256")))))) return false;
                auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("actor_package"),SourcePackage);Row->SetStringField(TEXT("mesh"),Mesh->GetPathName());
                Row->SetStringField(TEXT("geometry_sha256"),GeometryHash);Row->SetStringField(TEXT("asset_sha256"),FileHash);Row->SetObjectField(TEXT("field"),Field);Actors.Add(MakeShared<FJsonValueObject>(Row));
            }
            if (!Test.TestTrue(TEXT("Complete saved Water inventory survives without substitution or omission."),Actors.Num()>0 && (!Expected || Actors.Num()==Expected->GetArrayField(TEXT("actors")).Num()))) return false;
            Out=MakeShared<FJsonObject>();Out->SetArrayField(TEXT("actors"),Actors);Out->SetNumberField(TEXT("actor_count"),Actors.Num());Out->SetObjectField(TEXT("material"),Identity);
            Out->SetNumberField(TEXT("canonical_stage_m"),0);Out->SetNumberField(TEXT("presentation_offset_m"),.25);return true;
        }
    }
    bool CheckWater(FAutomationTestBase& Test,UWorld& World,const ProjectWorldBasinSceneTest::FInput& Input,FJsonObject& Receipt)
    {
        TSharedPtr<FJsonObject> Actual;if (!ObserveWater(Test,World,Input,Input.Expected->GetObjectField(TEXT("water")).Get(),Actual)) return false;
        Receipt.SetObjectField(TEXT("water"),Actual);return true;
    }
    bool ValidateInput(FAutomationTestBase& Test,const FJsonObject& Document,const FProjectWorldCanonicalBundle& Bundle)
    {
        const TSharedPtr<FJsonObject>* Context=nullptr;FString Error;
        const auto Refuse=[&](){Test.AddError(TEXT("Invalid closed coastal scene context: ")+Error);return false;};
        if (!Document.TryGetObjectField(TEXT("coastal_context"),Context) || !Context || !Context->IsValid()) return Refuse();
        for (const auto* Name : {TEXT("base_result"),TEXT("runtime_fixture")})
        {
            FString Path,Hash;const FString HashKey=FString(Name)+TEXT("_sha256");
            if (!(*Context)->TryGetStringField(Name,Path) || !(*Context)->TryGetStringField(HashKey,Hash) || Hash.Len()!=64 ||
                !FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(Path),FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("tmp")))) return Refuse();
            for (const TCHAR V : Hash) if (!FChar::IsHexDigit(V)) return Refuse();
        }
        const auto Numeric=[](const TSharedPtr<FJsonValue>& Value,int32 Count)
        {
            const TArray<TSharedPtr<FJsonValue>>* Values=nullptr;
            if (!Value.IsValid() || !Value->TryGetArray(Values) || !Values || Values->Num()!=Count) return false;
            for (const auto& V : *Values) if (!V.IsValid() || V->Type!=EJson::Number || !FMath::IsFinite(V->AsNumber())) return false;
            return true;
        };
        if (!Numeric((*Context)->TryGetField(TEXT("start")),3) || !Numeric((*Context)->TryGetField(TEXT("render_bounds")),4)) return Refuse();
        const auto& Bounds=(*Context)->GetArrayField(TEXT("render_bounds"));
        if (Bounds[0]->AsNumber()>=Bounds[2]->AsNumber() || Bounds[1]->AsNumber()>=Bounds[3]->AsNumber()) return Refuse();
        const TArray<TSharedPtr<FJsonValue>>* Routes=nullptr;
        if (!(*Context)->TryGetArrayField(TEXT("short_routes"),Routes) || !Routes || Routes->Num()!=3) return Refuse();
        for (const auto& Route : *Routes)
        {
            const TArray<TSharedPtr<FJsonValue>>* Points=nullptr;
            if (!Route.IsValid() || !Route->TryGetArray(Points) || !Points || Points->Num()<2 || Points->Num()>32) return Refuse();
            for (const auto& Point : *Points) if (!Numeric(Point,2)) return Refuse();
        }
        ProjectWorldCoastalFixture::FUpper Upper;
        if (!ProjectWorldCoastalFixture::LoadUpper((*Context)->GetStringField(TEXT("runtime_fixture")),
            (*Context)->GetStringField(TEXT("runtime_fixture_sha256")),Bundle,Upper,Error)) return Refuse();
        auto Probe=Bundle;
        if (!ProjectWorldCoastalFixture::AddOriginalWater((*Context)->GetStringField(TEXT("base_result")),
            (*Context)->GetStringField(TEXT("base_result_sha256")),Probe,Error)) return Refuse();
        return true;
    }
    bool BindNativeParts(FAutomationTestBase& Test,const FProjectWorldCanonicalBundle& Bundle,
        const UE::MeshPartition::FMeshData& Native,const FTransform& Parent,ProjectWorldTerrainStorageFixture::FSplitSections& Split)
    {
        using namespace UE::MeshPartition;
        const auto& B=Bundle.Cells[0].Bounds;FGridSettings Grid;Grid.CellSize=3000;Grid.bIs2D=true;
        Grid.WorldOriginOffset=FProjectWorldCanonicalLoader::CanonicalToUnreal(Bundle,FVector(B.X,B.Y,Bundle.HeightOriginMeters));
        auto Parts=GridHelpers::BuildGridCellMeshes(Native,Grid,Parent);TSet<FIntVector> Seen;Split.Parts.Reset();
        if (!Test.TestEqual(TEXT("Actual native section count equals the independently resolved grid inventory."),Split.Sections.Num(),Parts.Num())) return false;
        for (auto* Section : Split.Sections)
        {
            const auto& Info=Section->GetBuildInfo();const auto& Actual=Info.GridSettings;
            if (!Test.TestTrue(TEXT("Every native section binds the exact requested grid and unique source part."),
                Actual.CellSize==Grid.CellSize && Actual.bIs2D && Actual.WorldOriginOffset==Grid.WorldOriginOffset &&
                Info.HasValidGridCellCoord() && !Seen.Contains(Info.GridCellCoord) && Parts.Contains(Info.GridCellCoord))) return false;
            Seen.Add(Info.GridCellCoord);Split.Parts.Add(MakeShared<FMeshData>(MoveTemp(Parts.FindChecked(Info.GridCellCoord))));
        }
        return true;
    }
    bool Prepare(FAutomationTestBase& Test,UWorld& World,const FProjectWorldCanonicalBundle& Bundle,
        UE::MeshPartition::UMeshPartitionDefinition& Definition,const UE::MeshPartition::UMeshPartitionDefinition& Shared)
    {
        using namespace UE::MeshPartition;FString Error;
        if (!ProjectWorldTerrainStorageFixture::PrepareDefinition(Test,Definition,Shared)) return false;
        for (const auto& ReadOnly : Definition.GetCompiledSectionBuildVariants())
        {
            auto& Variant=const_cast<FCompiledSectionBuildVariant&>(ReadOnly);
            auto& Steps=const_cast<TArray<TInstancedStruct<FTransformer>>&>(Variant.TransformerPipeline->GetTransformers());
            Steps.Add(TInstancedStruct<FTransformer>::Make<FWPActorPropertiesTransformer>());
            Variant.bSplitSectionsToMatchWorldPartitionRuntimeGrid=true;
        }
        const auto& B=Bundle.Cells[0].Bounds;
        const FVector Origin=FProjectWorldCanonicalLoader::CanonicalToUnreal(Bundle,FVector(B.X,B.Y,Bundle.HeightOriginMeters));
        if (!ProjectWorldCoastalFixture::ConfigureGrid(World,Origin,Error)) {Test.AddError(Error);return false;}
        const FString File=FPackageName::LongPackageNameToFilename(Definition.GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
        FSavePackageArgs Save;Save.TopLevelFlags=RF_Public|RF_Standalone;
        return Test.TestTrue(TEXT("Private definition and native grid are saved before native dependency resolution."),
            UPackage::SavePackage(Definition.GetOutermost(),&Definition,*File,Save));
    }
    bool ApplyWater(FAutomationTestBase& Test,UWorld& World,const FJsonObject& Document,ProjectWorldBasinSceneTest::FInput& Input)
    {
        const auto Context=Document.GetObjectField(TEXT("coastal_context"));FString Error;
        if (!ProjectWorldCoastalFixture::AddOriginalWater(Context->GetStringField(TEXT("base_result")),
            Context->GetStringField(TEXT("base_result_sha256")),Input.Bundle,Error)) {Test.AddError(Error);return false;}
        UMaterialInterface* Material=nullptr;TSharedPtr<FJsonObject> Identity;
        if (!ProjectWorldBasinFixture::ResolveWaterMaterial(Material,Identity,Error)) {Test.AddError(Error);return false;}
        const auto* Producer=ProjectWorldLayerProducerRegistry::Find(TEXT("project_water_mesh"),1,Error);
        if (!Producer) {Test.AddError(Error);return false;}
        FProjectWorldRealizationLayer Layer;Layer.LayerId=TEXT("coastal_water");Layer.GeneratorId=TEXT("project_water_mesh");Layer.GeneratorVersion=1;
        Layer.ArtifactRoot=Input.Mount+TEXT("Water/");Layer.CanonicalSelectors={TEXT("water")};Layer.SpatialOwnership=TEXT("cell_local");
        Layer.DirtyGranularity=EProjectWorldDirtyGranularity::CanonicalCell;Layer.RuntimeMapping=TEXT("world_partition_spatial");
        Layer.NormalizedSettings=TEXT("{\"material_shading_model\":\"solid_opaque\",\"nanite\":false,\"surface_offset_m\":0.25}");
        if (!ProjectWorldLayerProducerRegistry::ValidateLayer(Layer,Error)) {Test.AddError(Error);return false;}
        FProjectWorldRealizationProfile Profile;Profile.Layers={Layer};FProjectWorldAuthoredOverlaySet Overlays;
        FProjectWorldLayerInputs Inputs{Input.Bundle,Profile,Profile.Layers[0],Overlays};FProjectWorldLayerUnitInputs Units;
        if (!Producer->HashUnitInputs(Inputs,Units,Error) || Units.HashByUnit.Num()!=1) {Test.AddError(Error);return false;}
        FProjectWorldRealizationResult Result;auto& Inventory=Result.LayerInventories.AddDefaulted_GetRef();
        Inventory.LayerId=Layer.LayerId;Inventory.GeneratorId=Layer.GeneratorId;Inventory.GeneratorVersion=1;
        Inventory.ArtifactRoot=Layer.ArtifactRoot;Inventory.FinalDirtyUnits={Input.Bundle.Cells[0].CellId};
        const TMap<FName,UMaterialInterface*> Materials{{TEXT("water"),Material}};
        FProjectWorldLayerApplyContext Apply{World,Inputs,Inventory.FinalDirtyUnits,Materials};
        if (!Producer->Apply(Apply,Inventory,Result,Error) || !Producer->CaptureArtifacts(Apply,Inventory,Result,Error)) {Test.AddError(Error);return false;}
        for (const auto& Artifact : Inventory.Artifacts)
        {
            FString Package;
            if (!FPackageName::TryConvertFilenameToLongPackageName(FPaths::ProjectDir()/Artifact.Path,Package) || !Package.StartsWith(Input.Mount))
            {Test.AddError(TEXT("Coastal Water output escaped its private mount."));return false;}
            Input.Packages.AddUnique(Package);
        }
        return ObserveWater(Test,World,Input,nullptr,Input.Expected);
    }
    bool InstallUpper(FAutomationTestBase& Test,UWorld& World,const FJsonObject& Document,const FProjectWorldCanonicalBundle& Bundle,
        const FString& Mount,TArray<FString>& Packages,FJsonObject& Receipt)
    {
        const auto Context=Document.GetObjectField(TEXT("coastal_context"));ProjectWorldCoastalFixture::FUpper Upper;FString Error;
        if (!ProjectWorldCoastalFixture::LoadUpper(Context->GetStringField(TEXT("runtime_fixture")),Context->GetStringField(TEXT("runtime_fixture_sha256")),Bundle,Upper,Error))
        {Test.AddError(Error);return false;}
        TArray<TSharedPtr<FJsonValue>> Inventory;
        for (int32 I=0;I<Upper.Units.Num();++I)
        {
            const FString Name=Mount+FString::Printf(TEXT("Upper/Unit_%d"),I);auto* Package=CreatePackage(*Name);
            auto* Mesh=ProjectWorldCoastalFixture::CompileUpper(Upper.Units[I],Test,*Package);
            if (!Mesh || !ProjectWorldCoastalFixture::SameUpper(Upper.Units[I],*Mesh,Test)) return false;
            const FString File=FPackageName::LongPackageNameToFilename(Name,FPackageName::GetAssetPackageExtension());
            FSavePackageArgs Save;Save.TopLevelFlags=RF_Public|RF_Standalone;
            if (!Test.TestTrue(TEXT("Exact retained Upper mesh saves under its private asset package."),UPackage::SavePackage(Package,Mesh,*File,Save))) return false;
            Packages.AddUnique(Name);
            auto* Actor=World.SpawnActor<AStaticMeshActor>();if (!Test.TestNotNull(TEXT("Retained Upper actor exists."),Actor)) return false;
            Actor->SetActorLocation(Upper.Units[I].Origin);Actor->Tags.Add(UpperTag);
            Actor->Tags.Add(FName(*FString::Printf(TEXT("ProjectWorld.Test.UpperUnit=%d"),I)));
            Actor->SetIsSpatiallyLoaded(true);Actor->SetHLODLayer(nullptr);Actor->bEnableAutoLODGeneration=false;
            auto* Component=Actor->GetStaticMeshComponent();Component->SetStaticMesh(Mesh);Component->SetMobility(EComponentMobility::Static);
            Component->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Component->SetCollisionResponseToAllChannels(ECR_Block);
            Component->SetCanEverAffectNavigation(true);Actor->MarkPackageDirty();
            auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("group"),Upper.Groups[I]);Row->SetNumberField(TEXT("unit"),I);
            Row->SetStringField(TEXT("mesh"),Mesh->GetPathName());Row->SetStringField(TEXT("actor_package"),Actor->GetPackage()->GetName());
            Row->SetNumberField(TEXT("faces"),Upper.Units[I].Triangles.Num());Inventory.Add(MakeShared<FJsonValueObject>(Row));
        }
        Receipt.SetArrayField(TEXT("coastal_upper"),Inventory);return true;
    }
}
#endif
