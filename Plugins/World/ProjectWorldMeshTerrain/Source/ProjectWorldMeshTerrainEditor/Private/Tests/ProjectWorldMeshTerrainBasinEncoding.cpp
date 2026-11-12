// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldMeshTerrainBasinScene.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "Misc/AutomationTest.h"
#include "Rendering/NaniteResources.h"
#include "StaticMeshCompiler.h"
#include "StaticMeshOperations.h"
#include "StaticMeshResources.h"
#include "UObject/UnrealType.h"
#include "Utilities/ProjectSha256.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldBasinSceneTest
{
    namespace
    {
        bool Field(UStaticMesh* Mesh,TMap<FString,int32>& Out,int32& VertexCount,FString& Hash)
        {
            if (!Mesh || !Mesh->GetRenderData() || Mesh->GetRenderData()->LODResources.Num()!=1) return false;
            const auto& LOD=Mesh->GetRenderData()->LODResources[0];
            const uint32 Count=LOD.VertexBuffers.PositionVertexBuffer.GetNumVertices(),Indices=LOD.IndexBuffer.GetNumIndices(),Stride=LOD.IndexBuffer.Is32Bit() ? 4 : 2;
            if (Count==0 || Count>65536 || Indices==0 || Indices>65536 || Indices%3 || LOD.VertexBuffers.PositionVertexBuffer.GetStride()!=sizeof(FVector3f)) return false;
            TArray<uint8> Positions,IDs;
            if (!ProjectWorldCompositeTerrainTests::ReadBuffer(LOD.VertexBuffers.PositionVertexBuffer.VertexBufferRHI.GetReference(),Count*sizeof(FVector3f),Positions) ||
                !ProjectWorldCompositeTerrainTests::ReadBuffer(LOD.IndexBuffer.IndexBufferRHI.GetReference(),Indices*Stride,IDs)) return false;
            TArray<FString> Vertices;
            for (uint32 I=0;I<Count;++I)
            {
                FVector3f P;FMemory::Memcpy(&P,Positions.GetData()+I*sizeof(P),sizeof(P));if (P.ContainsNaN()) return false;
                uint32 Bits[3];FMemory::Memcpy(Bits,&P,sizeof(P));
                Vertices.Add(FString::Printf(TEXT("%08x,%08x,%08x"),Bits[0],Bits[1],Bits[2]));
            }
            for (uint32 I=0;I<Indices;I+=3)
            {
                FString Keys[3];
                for (uint32 J=0;J<3;++J)
                {
                    uint32 ID=0;if (Stride==4) FMemory::Memcpy(&ID,IDs.GetData()+(I+J)*Stride,4);
                    else {uint16 Value;FMemory::Memcpy(&Value,IDs.GetData()+(I+J)*Stride,2);ID=Value;}
                    if (ID>=Count) return false;Keys[J]=Vertices[ID];
                }
                const FString A=Keys[0]+TEXT("|")+Keys[1]+TEXT("|")+Keys[2],B=Keys[1]+TEXT("|")+Keys[2]+TEXT("|")+Keys[0],C=Keys[2]+TEXT("|")+Keys[0]+TEXT("|")+Keys[1];
                ++Out.FindOrAdd(FMath::Min(A,FMath::Min(B,C)));
            }
            TArray<FString> Keys;Out.GetKeys(Keys);Keys.Sort();FString Text;
            for (const auto& Key : Keys) Text+=Key+TEXT(":")+LexToString(Out[Key])+TEXT("\n");
            VertexCount=Count;return FProjectSha256::HashNormalizedText(Text,Hash);
        }
    }
    TStrongObjectPtr<UStaticMesh> CompileEncodingCandidate(FAutomationTestBase& Test,UStaticMesh* Source,int32 Precision,FJsonObject& Metrics)
    {
        const auto Empty=[]() {return TStrongObjectPtr<UStaticMesh>(nullptr);};
        if (!Test.TestTrue(TEXT("Encoding comparison starts from one complete actual native Nanite source model."),Source && Source->HasValidNaniteData() && Source->GetNumSourceModels()==1 && Precision>=4 && Precision<=16)) return Empty();
        const FMeshDescription* Description=Source->GetMeshDescription(0);
        if (!Test.TestNotNull(TEXT("The actual saved section retains its source mesh description."),Description)) return Empty();
        const auto Attributes=FStaticMeshOperations::ComputeSHAHash(*Description);
        TStrongObjectPtr<UStaticMesh> Candidate(NewObject<UStaticMesh>(GetTransientPackage(),NAME_None,RF_Transient));
        Candidate->SetNumSourceModels(1);Candidate->GetSourceModel(0).BuildSettings=Source->GetSourceModel(0).BuildSettings;
        Candidate->GetSourceModel(0).ReductionSettings=Source->GetSourceModel(0).ReductionSettings;
        Candidate->GetStaticMaterials()=Source->GetStaticMaterials();auto Settings=Source->GetNaniteSettings();Settings.PositionPrecision=Precision;
        Candidate->SetNaniteSettings(Settings);Candidate->bSupportRayTracing=Source->bSupportRayTracing;
        Candidate->bGenerateMeshDistanceField=Source->bGenerateMeshDistanceField;Candidate->bHasNavigationData=Source->bHasNavigationData;
        UStaticMesh::FBuildMeshDescriptionsParams Params;Params.bUseHashAsGuid=true;Params.bMarkPackageDirty=false;
        if (!Test.TestTrue(TEXT("The isolated encoding candidate builds with native geometry and unchanged CPU retention."),
            !Source->bAllowCPUAccess && Candidate->BuildFromMeshDescriptions({Description},Params))) return Empty();
        FStaticMeshCompilingManager::Get().FinishAllCompilation();FlushRenderingCommands();
        const auto* Built=Candidate->GetMeshDescription(0);
        if (!Test.TestTrue(TEXT("All source attributes, materials and build settings survive the encoding-only comparison."),Built &&
            FStaticMeshOperations::ComputeSHAHash(*Built)==Attributes && FStaticMeshOperations::ComputeSHAHash(*Description)==Attributes &&
            Candidate->GetStaticMaterials()==Source->GetStaticMaterials() && Candidate->GetNaniteSettings()==Settings && !Candidate->bAllowCPUAccess &&
            FMeshBuildSettings::StaticStruct()->CompareScriptStruct(&Source->GetSourceModel(0).BuildSettings,&Candidate->GetSourceModel(0).BuildSettings,PPF_None) &&
            FMeshReductionSettings::StaticStruct()->CompareScriptStruct(&Source->GetSourceModel(0).ReductionSettings,&Candidate->GetSourceModel(0).ReductionSettings,PPF_None))) return Empty();
        TMap<FString,int32> Original,Actual;int32 OriginalVertices=0,ActualVertices=0;FString OriginalHash,ActualHash;
        if (!Test.TestTrue(TEXT("Both encoding arms expose complete actual GPU geometry."),Field(Source,Original,OriginalVertices,OriginalHash) && Field(Candidate.Get(),Actual,ActualVertices,ActualHash))) return Empty();
        int32 Mismatched=0,Faces=0;
        for (const auto& Pair : Original) {Faces+=Pair.Value;Mismatched+=FMath::Abs(Pair.Value-Actual.FindRef(Pair.Key));}
        for (const auto& Pair : Actual) if (!Original.Contains(Pair.Key)) Mismatched+=Pair.Value;
        Metrics.SetStringField(TEXT("source_attribute_sha1"),Attributes.ToString());Metrics.SetStringField(TEXT("source_gpu_field_sha256"),OriginalHash);
        Metrics.SetStringField(TEXT("candidate_gpu_field_sha256"),ActualHash);Metrics.SetNumberField(TEXT("mismatched_gpu_faces"),Mismatched);
        Metrics.SetNumberField(TEXT("source_gpu_faces"),Faces);Metrics.SetNumberField(TEXT("source_gpu_vertices"),OriginalVertices);
        Metrics.SetNumberField(TEXT("candidate_gpu_vertices"),ActualVertices);Metrics.SetNumberField(TEXT("requested_position_precision"),Precision);
        Metrics.SetBoolField(TEXT("source_never_stream"),Source->NeverStream);Metrics.SetBoolField(TEXT("candidate_never_stream"),Candidate->NeverStream);
        Metrics.SetStringField(TEXT("construction_scope"),TEXT("Native BuildFromMeshDescriptions sets NeverStream=true; source attributes/material/build/reduction/Nanite settings are compared, with position precision the sole authored geometry-setting change. No streaming lifetime claim."));
        Metrics.SetNumberField(TEXT("effective_position_precision"),Candidate->GetRenderData()->NaniteResourcesPtr->PositionPrecision);
        if (!Test.TestTrue(FString::Printf(TEXT("Encoding field/precision: mismatched_faces=%d faces=%d source_vertices=%d candidate_vertices=%d source_hash=%s candidate_hash=%s requested=%d effective=%d."),
            Mismatched,Faces,OriginalVertices,ActualVertices,*OriginalHash,*ActualHash,Precision,Candidate->GetRenderData()->NaniteResourcesPtr->PositionPrecision),
            Mismatched==0 && Faces==1606 && OriginalHash==ActualHash && OriginalVertices==ActualVertices && Candidate->HasValidNaniteData() &&
            Candidate->GetRenderData()->NaniteResourcesPtr->PositionPrecision==Precision)) return Empty();
        return Candidate;
    }
}
#endif
