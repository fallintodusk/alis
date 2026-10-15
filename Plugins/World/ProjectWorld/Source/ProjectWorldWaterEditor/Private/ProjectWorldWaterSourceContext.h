// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "ProjectWorldCanonicalBundle.h"

namespace ProjectWorldWaterSourceContext
{
    inline TArray<FString> FeatureIds(const FProjectWorldCanonicalBundle& Bundle,
        const FProjectWorldCanonicalCell& Cell)
    {
        TSet<FString> Unique;
        Unique.Append(Cell.OwnedFeatureIds);
        Unique.Append(Cell.ReferencedFeatureIds);
        TArray<FString> Result;
        for (const FString& Id : Unique)
        {
            const auto* Feature=Bundle.Features.Find(Id);
            if (Feature && Feature->FeatureClass==TEXT("water") && Feature->WaterSurface.bValid)
                Result.Add(Id);
        }
        Result.Sort();
        return Result;
    }

    // CDT preparation consumes this domain before per-cell clipping. Hash and
    // realization must use the same bounds, including other consuming cells.
    inline TMap<FString,FVector4d> PreparationBounds(const FProjectWorldCanonicalBundle& Bundle)
    {
        TMap<FString,FVector4d> Result;
        for (const auto& Cell : Bundle.Cells)
            for (const auto& Id : FeatureIds(Bundle,Cell))
            {
                FVector4d* Bounds=Result.Find(Id);
                if (!Bounds) {Result.Add(Id,Cell.Bounds); continue;}
                Bounds->X=FMath::Min(Bounds->X,Cell.Bounds.X);
                Bounds->Y=FMath::Min(Bounds->Y,Cell.Bounds.Y);
                Bounds->Z=FMath::Max(Bounds->Z,Cell.Bounds.Z);
                Bounds->W=FMath::Max(Bounds->W,Cell.Bounds.W);
            }
        return Result;
    }
}
