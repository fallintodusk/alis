// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "CoreMinimal.h"
#include "MeshDescription.h"

struct FProjectWorldCanonicalBundle;
struct FProjectWorldCanonicalCell;
struct FProjectWorldCanonicalFeature;

namespace ProjectWorldWaterSourceApproximation
{
    bool Refine(FMeshDescription& Mesh,
        TConstArrayView<const FProjectWorldCanonicalFeature*> TriangleSources,
        const FProjectWorldCanonicalBundle& Bundle,const FProjectWorldCanonicalCell& Cell,
        const FVector& Origin,double SurfaceOffsetMeters,
        bool& OutChanged,FString& OutGeometryHash,FString& OutError);
}
