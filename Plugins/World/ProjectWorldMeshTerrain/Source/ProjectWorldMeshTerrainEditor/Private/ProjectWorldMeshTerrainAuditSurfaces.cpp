// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldMeshTerrainAuditSurfaces.h"
#include "ProjectWorldCanonicalBundle.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "MeshPartitionCompiledSection.h"
#include "MeshPartitionStaticMeshComponent.h"
#include "StaticMeshResources.h"

namespace ProjectWorldMeshTerrainAuditSurfaces
{
    TArray<UStaticMeshComponent*> VisualComponents(const AActor& Section)
    {
        TArray<UStaticMeshComponent*> Result;
        // Native sections also register auxiliary RVT meshes outside their primary inventory.
        if (const auto* Compiled=Cast<UE::MeshPartition::ACompiledSection>(&Section))
            for (const auto& Component : Compiled->GetMeshComponents()) Result.Add(Component);
        return Result;
    }

    bool BuildSurfaceProbe(const AActor* Section,const UStaticMeshComponent* Component,
        FSurfaceProbe& OutProbe,bool IncludeTriangles)
    {
        const UStaticMesh* Mesh=Component ? Component->GetStaticMesh() : nullptr;
        const FStaticMeshRenderData* Data=Mesh ? Mesh->GetRenderData() : nullptr;
        if (!Section || !Data || Data->LODResources.IsEmpty()) return false;
        const auto& Positions=Data->LODResources[0].VertexBuffers.PositionVertexBuffer;
        if (!Positions.GetNumVertices()) return false;
        OutProbe.Actor=Section->GetPathName();OutProbe.Bounds=Component->Bounds;
        OutProbe.Vertices.Reserve(Positions.GetNumVertices());
        for (uint32 Index=0;Index<Positions.GetNumVertices();++Index)
            OutProbe.Vertices.Add(Component->GetComponentTransform().TransformPosition(FVector(Positions.VertexPosition(Index))));
        if (IncludeTriangles)
        {
            const auto Indices=Data->LODResources[0].IndexBuffer.GetArrayView();
            for (int32 Index=0;Index+2<Indices.Num();Index+=3)
                OutProbe.Triangles.Add(FIntVector(Indices[Index],Indices[Index+1],Indices[Index+2]));
        }
        return true;
    }

    bool CompareRefinements(const FProjectWorldCanonicalBundle& Bundle,
        const TArray<FSurfaceProbe>& Nanite,const TArray<FSurfaceProbe>& Fallback,double ToleranceMeters,
        FProjectWorldMeshTerrainSurfaceFidelity& NaniteResult,FProjectWorldMeshTerrainSurfaceFidelity& FallbackResult,
        TArray<FString>& Failures)
    {
        FString Error;
        const auto Compare=[&](const TArray<FSurfaceProbe>& Surfaces,FProjectWorldMeshTerrainSurfaceFidelity& Result)
        {
            Result={};Error.Reset();
            TArray<FProjectWorldSurfaceTriangle> Faces;
            for (const auto& Surface : Surfaces) for (const auto& Triangle : Surface.Triangles)
            {
                if (!Surface.Vertices.IsValidIndex(Triangle.X) || !Surface.Vertices.IsValidIndex(Triangle.Y) ||
                    !Surface.Vertices.IsValidIndex(Triangle.Z)) {Error=TEXT("A compiled surface index escapes its vertex buffer.");return false;}
                Faces.Add({{FProjectWorldCanonicalLoader::UnrealToCanonical(Bundle,Surface.Vertices[Triangle.X]),
                    FProjectWorldCanonicalLoader::UnrealToCanonical(Bundle,Surface.Vertices[Triangle.Y]),
                    FProjectWorldCanonicalLoader::UnrealToCanonical(Bundle,Surface.Vertices[Triangle.Z])}});
            }
            return ProjectWorldMeshTerrainProducer::CompareRefinedSurface(Bundle,Faces,ToleranceMeters,Result,Error);
        };
        const bool NaniteAccepted=Compare(Nanite,NaniteResult);
        if (!NaniteAccepted) Failures.Add(TEXT("Compiled refinement fidelity failed (nanite): ")+Error);
        const bool FallbackAccepted=Compare(Fallback,FallbackResult);
        if (!FallbackAccepted) Failures.Add(TEXT("Compiled refinement fidelity failed (fallback): ")+Error);
        return NaniteAccepted && FallbackAccepted;
    }
}
