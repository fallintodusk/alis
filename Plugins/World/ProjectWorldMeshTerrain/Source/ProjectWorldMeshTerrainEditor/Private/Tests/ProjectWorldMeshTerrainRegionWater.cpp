#include "ProjectWorldMeshTerrainRegionWater.h"
#include "ProjectWorldMeshTerrainBasinScene.h"
#include "ProjectWorldAuthoredOverlay.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldDataRoots.h"
#include "ProjectWorldLayerProducerRegistry.h"
#include "ProjectWorldVerifyTestFlow.h"
#include "Tests/ProjectWorldBasinFixture.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Materials/MaterialInterface.h"
#include "StaticMeshResources.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldRegionWater
{
    namespace
    {
        TArray<TSharedPtr<FJsonValue>> Numbers(std::initializer_list<double> Values)
        {TArray<TSharedPtr<FJsonValue>> Out;for(double V:Values) Out.Add(MakeShared<FJsonValueNumber>(V));return Out;}
        bool Invoke(FAutomationTestBase& Test,UWorld& World,const FProjectWorldCanonicalBundle& Bundle,FState& State,bool Apply)
        {
            auto& Layer=State.Profile.Layers[0];FString Error;
            const auto* Producer=ProjectWorldLayerProducerRegistry::Find(Layer.GeneratorId,Layer.GeneratorVersion,Error);
            UMaterialInterface* Material=nullptr;TSharedPtr<FJsonObject> Identity;
            if (!Test.TestTrue(TEXT("Regional Water resolves its existing producer and accepted presentation."),
                Producer && ProjectWorldBasinFixture::ResolveWaterMaterial(Material,Identity,Error))) {Test.AddError(Error);return false;}
            FProjectWorldAuthoredOverlaySet Overlays;const FProjectWorldLayerInputs Inputs{Bundle,State.Profile,Layer,Overlays};
            const TArray<FString> Dirty={TEXT("*")};TMap<FName,UMaterialInterface*> Materials;Materials.Add(TEXT("water"),Material);
            const FProjectWorldLayerApplyContext Context{World,Inputs,Dirty,Materials};FProjectWorldRealizationResult Result;
            FProjectWorldLayerInventory I;I.LayerId=Layer.LayerId;I.GeneratorId=Layer.GeneratorId;I.GeneratorVersion=Layer.GeneratorVersion;
            I.ArtifactRoot=Layer.ArtifactRoot;I.NormalizedLayerContractHash=Layer.ContractHash;I.FinalDirtyUnits=Dirty;Result.LayerInventories.Add(I);
            const bool Success=Apply ? Producer->Apply(Context,Result.LayerInventories[0],Result,Error) :
                Producer->CaptureArtifacts(Context,Result.LayerInventories[0],Result,Error);
            if (!Test.TestTrue(TEXT("Ordinary Water uses registered Apply/Capture without another geometry implementation."),Success)) {Test.AddError(Error);return false;}
            return true;
        }
    }
    bool Apply(FAutomationTestBase& Test,UWorld& World,const FJsonObject& Document,
        const FProjectWorldCanonicalBundle& Bundle,const FString& Mount,TArray<FString>& Packages,FState& State)
    {
        const TSharedPtr<FJsonObject>* Ref=nullptr;FString Plugin,Relative,Expected,Path,Code,Error;FProjectWorldDataRoots Roots;
        if (!Test.TestTrue(TEXT("Regional Water declares an authenticated profile in its data owner."),
            Document.TryGetObjectField(TEXT("water_profile"),Ref) && Ref &&
            (*Ref)->TryGetStringField(TEXT("world_data_plugin"),Plugin) && (*Ref)->TryGetStringField(TEXT("path"),Relative) &&
            (*Ref)->TryGetStringField(TEXT("sha256"),Expected) && FPaths::IsRelative(Relative) &&
            FProjectWorldDataRoots::Resolve(Plugin,Roots,Error))) return false;
        Path=FPaths::ConvertRelativePathToFull(Roots.DataRoot/Relative);FString Actual;
        if (!Test.TestTrue(TEXT("The profile is confined and unchanged before reading."),
            FPaths::IsUnderDirectory(Path,Roots.DataRoot/TEXT("Profiles/Realization")) && Expected.Len()==64 &&
            FProjectWorldCanonicalLoader::ComputeFileSha256(Path,Actual) && Actual==Expected &&
            ProjectWorldRealizationProfile::Load(Path,State.Profile,Code,Error) && State.Profile.WorldDataPluginName==Plugin &&
            State.Profile.CanonicalProfileId==Bundle.ProfileId))
        {Test.AddError(Code+TEXT(": ")+Error);return false;}
        State.ProfileHash=Actual;State.Profile.Layers.RemoveAll([](const auto& L){return L.LayerId!=TEXT("water");});
        if (!Test.TestTrue(TEXT("The source profile supplies exactly one ordinary Water layer."),State.Profile.Layers.Num()==1)) return false;
        auto& Layer=State.Profile.Layers[0];Layer.ArtifactRoot=Mount+TEXT("Water/");
        Layer=ProjectWorldVerifyTestFlow::Finalize(MoveTemp(Layer));
        if (!Invoke(Test,World,Bundle,State,true)) return false;
        for (TActorIterator<AStaticMeshActor> It(&World);It;++It) if (It->ActorHasTag(TEXT("ProjectWorld.Water.v1")))
        {
            UStaticMesh* Mesh=It->GetStaticMeshComponent()->GetStaticMesh();
            if (!Test.TestTrue(TEXT("Only scratch-owned ordinary Water packages join the scene."),Mesh && Mesh->GetOutermost()->GetName().StartsWith(Mount))) return false;
            Packages.AddUnique(Mesh->GetOutermost()->GetName());
        }
        return true;
    }
    bool Observe(FAutomationTestBase& Test,UWorld& World,const FProjectWorldCanonicalBundle& Bundle,
        FState& State,FJsonObject& Receipt,bool Reloaded)
    {
        TArray<AStaticMeshActor*> Subjects;
        for (TActorIterator<AStaticMeshActor> It(&World);It;++It)
            if (It->ActorHasTag(TEXT("ProjectWorld.Water.v1"))) Subjects.Add(*It);
        if (!Test.TestEqual(TEXT("Regional ordinary Water census covers every declared nonempty original cell."),Subjects.Num(),Bundle.Cells.Num())) return false;
        Subjects.Sort([](const AStaticMeshActor& A,const AStaticMeshActor& B){return A.GetPackage()->GetName()<B.GetPackage()->GetName();});
        if (!Reloaded && !State.Before.IsValid())
        {
            const auto Tags=Subjects[0]->Tags;Subjects[0]->Tags.Remove(TEXT("ProjectWorld.Water.v1"));
            Test.AddExpectedError(TEXT("Regional ordinary Water census"),EAutomationExpectedErrorFlags::Contains,1);
            FJsonObject Control;const bool Accepted=Observe(Test,World,Bundle,State,Control,false);Subjects[0]->Tags=Tags;
            if (!Test.TestFalse(TEXT("Omitting the actual Water subject cannot pass regional observation."),Accepted)) return false;
        }
        if (Reloaded && !Test.TestTrue(TEXT("Reopened Water has an actual pre-release observation."),State.Before.IsValid())) return false;
        if (Reloaded) for (const auto& Original:State.OriginalMeshes)
            if (!Test.TestFalse(TEXT("Reopened Water cannot reuse an original mesh object."),Original.IsValid())) return false;
        if (!Invoke(Test,World,Bundle,State,false)) return false;
        if (!Reloaded) State.OriginalMeshes.Reset();
        TArray<TSharedPtr<FJsonValue>> Actors;
        for (auto* Subject : Subjects)
        {
            auto* It=Subject;
            auto* C=It->GetStaticMeshComponent();UStaticMesh* Mesh=C ? C->GetStaticMesh() : nullptr;
            if (!Test.TestTrue(TEXT("Saved Water is deliberately nonphysical and exposes one complete render field."),
                Mesh && Mesh->GetRenderData() && Mesh->GetRenderData()->LODResources.Num()==1 &&
                C->GetCollisionEnabled()==ECollisionEnabled::NoCollision && !C->CanEverAffectNavigation())) return false;
            const auto& LOD=Mesh->GetRenderData()->LODResources[0];const uint32 Count=LOD.VertexBuffers.PositionVertexBuffer.GetNumVertices();
            if (!Reloaded) State.OriginalMeshes.Add(Mesh);
            const uint32 Indices=LOD.IndexBuffer.GetNumIndices(),Stride=LOD.IndexBuffer.Is32Bit() ? 4 : 2;TArray<uint8> Positions,IndexBytes;
            if (!Test.TestTrue(TEXT("Water measurements read actual saved GPU vertices and indices."),Count>0 && Indices>0 && Indices%3==0 &&
                LOD.VertexBuffers.PositionVertexBuffer.GetStride()==sizeof(FVector3f) &&
                ProjectWorldCompositeTerrainTests::ReadBuffer(LOD.VertexBuffers.PositionVertexBuffer.VertexBufferRHI.GetReference(),Count*sizeof(FVector3f),Positions) &&
                ProjectWorldCompositeTerrainTests::ReadBuffer(LOD.IndexBuffer.IndexBufferRHI.GetReference(),Indices*Stride,IndexBytes))) return false;
            auto Row=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Vertices,Faces;
            for(uint32 I=0;I<Count;++I)
            {
                FVector3f Local;FMemory::Memcpy(&Local,Positions.GetData()+I*sizeof(Local),sizeof(Local));
                const FVector P=FProjectWorldCanonicalLoader::UnrealToCanonical(Bundle,C->GetComponentTransform().TransformPosition(FVector(Local)));
                if (!Test.TestTrue(TEXT("Every saved Water position is finite in the original canonical frame."),!P.ContainsNaN())) return false;
                Vertices.Add(MakeShared<FJsonValueArray>(Numbers({P.X,P.Y,P.Z})));
            }
            for(uint32 I=0;I<Indices;I+=3)
            {
                uint32 Id[3]={};for(int32 J=0;J<3;++J) FMemory::Memcpy(&Id[J],IndexBytes.GetData()+(I+J)*Stride,Stride);
                if (!Test.TestTrue(TEXT("Every saved Water index addresses its measured field."),Id[0]<Count && Id[1]<Count && Id[2]<Count)) return false;
                Faces.Add(MakeShared<FJsonValueArray>(Numbers({double(Id[0]),double(Id[1]),double(Id[2])})));
            }
            Row->SetStringField(TEXT("actor_package"),It->GetPackage()->GetName());Row->SetStringField(TEXT("mesh"),Mesh->GetPathName());
            Row->SetStringField(TEXT("material"),GetPathNameSafe(C->GetMaterial(0)));Row->SetArrayField(TEXT("vertices_canonical"),Vertices);
            Row->SetArrayField(TEXT("triangles"),Faces);Actors.Add(MakeShared<FJsonValueObject>(Row));
        }
        auto Current=MakeShared<FJsonObject>();Current->SetArrayField(TEXT("actors"),Actors);
        Current->SetStringField(TEXT("profile_sha256"),State.ProfileHash);
        Current->SetNumberField(TEXT("complete_cell_count"),Bundle.Cells.Num());
        Current->SetStringField(TEXT("scope"),TEXT("Actual regional ordinary Water; preparation consumes only the selected complete original cells. Full-world preparation equivalence and geography acceptance are not proved."));
        if (Reloaded)
        {
            FString Before,After;FJsonSerializer::Serialize(State.Before.ToSharedRef(),TJsonWriterFactory<>::Create(&Before));
            FJsonSerializer::Serialize(Current,TJsonWriterFactory<>::Create(&After));
            if (!Test.TestTrue(TEXT("Reopened ordinary Water retains its complete measured field, binding and saved identity."),Before==After)) return false;
        }
        else State.Before=Current;
        Receipt.SetObjectField(Reloaded ? TEXT("regional_water_reloaded") : TEXT("regional_water_before"),Current);
        return true;
    }
}
#endif
