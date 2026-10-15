// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldMaterialReferenceUpdate.h"
#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "PhysicsEngine/BodySetup.h"
#include "Rendering/NaniteResources.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "StaticMeshResources.h"
#include "Utilities/ProjectSha256.h"

bool ProjectWorldMaterialReferenceUpdate::HashGeometry(UStaticMesh& Mesh, FString& Hash, FString& Error)
{
    auto* Render=Mesh.GetRenderData(); auto* Body=Mesh.GetBodySetup();
    if (!Render || !Body || Mesh.GetNumSourceModels()==0)
    {Error=TEXT("Material-only adoption requires existing source, render and collision data."); return false;}
    TArray<uint8> Bytes; FMemoryWriter Memory(Bytes); FObjectAndNameAsStringProxyArchive Ar(Memory,false);
    auto Bounds=Mesh.GetBounds(); Ar << Bounds.Origin << Bounds.BoxExtent << Bounds.SphereRadius;
    FString Key=Render->DerivedDataKey; Ar << Key;
    int32 Sources=Mesh.GetNumSourceModels(); Ar << Sources;
    for (int32 I=0; I<Sources; ++I)
    {
        auto* Description=Mesh.GetMeshDescription(I);
        if (!Description) {Error=TEXT("Unknown source mesh description cannot certify unchanged geometry."); return false;}
        Description->Serialize(Ar);
    }
    int32 Lods=Render->LODResources.Num(); Ar << Lods;
    for (const auto& Lod : Render->LODResources)
    {
        const auto& Positions=Lod.VertexBuffers.PositionVertexBuffer;
        const auto& Vertices=Lod.VertexBuffers.StaticMeshVertexBuffer;
        const auto& Colors=Lod.VertexBuffers.ColorVertexBuffer;
        uint32 Count=Positions.GetNumVertices(); Ar << Count;
        for (uint32 I=0; I<Count; ++I)
        {
            auto Position=Positions.VertexPosition(I); Ar << Position;
            auto X=Vertices.VertexTangentX(I); auto Y=Vertices.VertexTangentY(I); auto Z=Vertices.VertexTangentZ(I);
            Ar << X << Y << Z;
            uint32 Channels=Vertices.GetNumTexCoords(); Ar << Channels;
            for (uint32 C=0; C<Channels; ++C) {auto UV=Vertices.GetVertexUV(I,C); Ar << UV;}
            if (Colors.GetNumVertices()) {auto Color=Colors.VertexColor(I); Ar << Color;}
        }
        uint32 Indices=Lod.IndexBuffer.GetNumIndices(); Ar << Indices;
        for (uint32 I=0; I<Indices; ++I) {uint32 Index=Lod.IndexBuffer.GetIndex(I); Ar << Index;}
    }
    if (Render->NaniteResourcesPtr) Render->NaniteResourcesPtr->Serialize(Ar,&Mesh,false);
    Body->Serialize(Ar);
    if (Ar.IsError()) {Error=TEXT("Cannot serialize unchanged mesh/collision payload."); return false;}
    return FProjectSha256::HashBuffer(Bytes,Hash);
}
