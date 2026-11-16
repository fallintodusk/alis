// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "CoreMinimal.h"
#include "ProjectWorldMeshTerrainSurfaceFidelity.h"
class AActor;
class UStaticMeshComponent;
struct FProjectWorldCanonicalBundle;

namespace ProjectWorldMeshTerrainAuditSurfaces
{
    struct FSurfaceProbe
    {
        FString Actor;
        FBoxSphereBounds Bounds;
        TArray<FVector> Vertices;
        TArray<FIntVector> Triangles;
    };
    TArray<UStaticMeshComponent*> VisualComponents(const AActor& Section);
    bool BuildSurfaceProbe(const AActor* Section,const UStaticMeshComponent* Component,
        FSurfaceProbe& OutProbe,bool IncludeTriangles);
    bool CompareRefinements(const FProjectWorldCanonicalBundle& Bundle,
        const TArray<FSurfaceProbe>& Nanite,const TArray<FSurfaceProbe>& Fallback,double ToleranceMeters,
        FProjectWorldMeshTerrainSurfaceFidelity& NaniteResult,FProjectWorldMeshTerrainSurfaceFidelity& FallbackResult,
        TArray<FString>& Failures);
}
