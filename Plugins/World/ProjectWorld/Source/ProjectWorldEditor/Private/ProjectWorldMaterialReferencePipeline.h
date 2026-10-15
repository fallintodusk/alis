// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "CoreMinimal.h"
class UWorld;
class UMaterialInterface;
struct FProjectWorldCanonicalBundle;
struct FProjectWorldRealizationProfile;
struct FProjectWorldAuthoredOverlaySet;
struct FProjectWorldLayerDirtyInput;
struct FProjectWorldRealizationResult;
namespace ProjectWorldMaterialReferencePipeline
{
bool Apply(UWorld& World, const FProjectWorldCanonicalBundle& Bundle,
    const FProjectWorldRealizationProfile& Profile, const FProjectWorldAuthoredOverlaySet& Overlays,
    const FProjectWorldLayerDirtyInput& DirtyInput, const TMap<FName,UMaterialInterface*>& Materials,
    FProjectWorldRealizationResult& Result, FString& Error);
}
