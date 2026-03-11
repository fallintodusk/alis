// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WeightState.generated.h"

/** Weight/encumbrance state thresholds. */
UENUM(BlueprintType)
enum class EWeightState : uint8
{
    Light,      // < 60%
    Medium,     // 60-80%
    Heavy,      // 80-100%
    Overweight  // > 100%
};
