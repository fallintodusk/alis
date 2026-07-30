// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/ProjectAbilitySet.h"
#include "EquippedItemData.generated.h"

USTRUCT()
struct PROJECTINVENTORY_API FEquippedItemData
{
    GENERATED_BODY()

    /** Instance ID of equipped item. */
    uint32 InstanceId = 0;

    /** Handles for granted abilities/effects (for cleanup on unequip). */
    FProjectAbilitySetHandles GrantedHandles;
};
