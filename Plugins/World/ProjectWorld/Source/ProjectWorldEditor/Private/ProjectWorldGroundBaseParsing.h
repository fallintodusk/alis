// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "CoreMinimal.h"
class FJsonObject;
struct FProjectWorldCanonicalBundle;
struct FProjectWorldCanonicalCell;
struct FProjectWorldCanonicalValidation;
namespace ProjectWorldGroundBaseParsing
{
    struct FRetainedInput
    {
        bool Present=false,KnownAccuracy=false;
        FString InputHash,ConstructionHash,AdmissionHash,GridHash,ProcessingHash,SourceHash;
        FString Profile,CanonicalCrs,SourceCrs,Confidence,Transform;
        FVector2D Origin,Spacing,Lattice,CellSpacing,Georeference;
        FIntPoint Size,Quads;
        int32 Halo=0;
        double Accuracy=0,HeightStep=0,CoordinateStep=0,HeightOrigin=0;
        TArray<double> Samples;
        TSet<FIntPoint> Targets,Used;
    };
    bool ReadInput(const TSharedPtr<FJsonObject>& Operation,const TSharedPtr<FJsonObject>& Input,
        const FString& Hash,FRetainedInput& Out,FString& Error);
    bool ReadTerrain(const TSharedPtr<FJsonObject>& Terrain,const TSharedPtr<FJsonObject>& Manifest,
        const TSharedPtr<FJsonObject>& CoverageCell,const FProjectWorldCanonicalBundle& Bundle,
        const FProjectWorldCanonicalCell& Cell,const FString& BaseHash,FRetainedInput& Input,
        FProjectWorldCanonicalValidation& Validation,const TSharedPtr<FJsonObject>& Refinement=nullptr);
    bool ValidateUsage(const FRetainedInput& Input,FProjectWorldCanonicalValidation& Validation);
}
