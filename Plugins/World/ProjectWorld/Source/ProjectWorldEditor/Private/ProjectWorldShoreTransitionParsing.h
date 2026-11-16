// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "CoreMinimal.h"
class FJsonObject;
struct FProjectWorldCanonicalBundle;
struct FProjectWorldCanonicalTerrain;
struct FProjectWorldCanonicalFeature;
namespace ProjectWorldShoreTransitionParsing
{
    struct FInput
    {
        FString Grid,GroundHash,WaterId,BaseWaterHash,WaterHash;
        TMap<FString,double> Policy;
        TMap<FString,TSet<FIntPoint>> Selected;
        TSet<FString> Used;
        TSharedPtr<FJsonObject> WaterSnapshot;
        double MaximumWetHeight=-MAX_dbl;
    };
    using FInputs=TMap<FString,FInput>;
    bool ReadInput(const TSharedPtr<FJsonObject>& Operation,const TSharedPtr<FJsonObject>& Input,
        const FString& Hash,FInputs& Out);
    bool ReadTerrain(const TSharedPtr<FJsonObject>& Document,const FString& Grid,const FString& Cell,FInputs& Inputs);
    bool ValidateEncoded(const TSharedPtr<FJsonObject>& Document,const FProjectWorldCanonicalTerrain& Terrain);
    bool ValidateSources(const TSharedPtr<FJsonObject>& Features,const FInputs& Inputs);
    bool ValidateWetField(const FInput& Input,const FProjectWorldCanonicalFeature& Water,
        const FProjectWorldCanonicalBundle& Bundle,double& MaximumWetHeight);
    bool Dependency(const TSharedPtr<FJsonObject>& Document,const FString& GroundInputHash,FString& Out);
    bool ValidateUsage(const FInputs& Inputs,const FString& GroundHash,const FProjectWorldCanonicalBundle& Bundle);
}
