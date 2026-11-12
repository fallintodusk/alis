#pragma once
#include "CoreMinimal.h"
#include "ProjectWorldTerrainSurface.h"

struct FProjectWorldCanonicalBundle;
struct FProjectWorldCanonicalConstructedSurface;
struct FProjectWorldRealizationLayer;

namespace ProjectWorldConstructedRealization
{
    struct FSettings { double MaximumJoinStepMeters=0; };
    struct FDependencyInputs
    {
        FSettings Options;
        TMap<FString,TArray<FProjectWorldSurfaceTriangle>> Fields;
        TMap<FString,FString> CellHashes;
        int64 ValidatedSourceFaces=0;
        int64 SemanticIdentityBytes=0;
    };
    bool Settings(const FString& Json,FSettings& Out,FString& Error);
    FString UnitIdentity(const FString& FeatureId,const FString& CellId);
    bool ValidateSourcePolicy(const FProjectWorldCanonicalConstructedSurface& Surface,TArray<int32>& RegionBySource,FString& Error);
    // Dependency identity is not certification of a future native float output.
    bool DependencyInputs(const FProjectWorldCanonicalBundle& Bundle,const FProjectWorldRealizationLayer& Layer,
        FDependencyInputs& Out,FString& Error);
}
