// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "CoreMinimal.h"

namespace ProjectWorldContainedStage
{
    inline bool IsJointBoundaryModel(const FString& FunctionId,int32 FunctionVersion)
    {
        return FunctionId==TEXT("identified_static_lined_basin") && FunctionVersion==1;
    }

    // Recognition precedes validity checks so malformed versions cannot fall
    // back to a presentation lift. This does not admit a canonical JSON model.
    inline bool Recognizes(const FString& FunctionId)
    {
        return FunctionId==TEXT("contained_ground_quantile") || FunctionId==TEXT("identified_static_lined_basin");
    }
}
