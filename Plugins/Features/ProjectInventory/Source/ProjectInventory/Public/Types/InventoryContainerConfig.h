// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "InventoryContainerConfig.generated.h"

USTRUCT(BlueprintType)
struct PROJECTINVENTORY_API FInventoryContainerConfig
{
    GENERATED_BODY()

    /** Container identifier (Item.Container.*). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
    FGameplayTag ContainerId;

    /** Grid size in cells (X=width, Y=height). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
    FIntPoint GridSize = FIntPoint(5, 4);

    /** Optional max weight for this container (0 = unlimited). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
    float MaxWeight = 0.0f;

    /** Optional max volume for this container (0 = unlimited). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
    float MaxVolume = 0.0f;

    /** Optional hard cap on total cells used (0 = unlimited). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
    int32 MaxCells = 0;

    /** Optional tag filter for items allowed in this container. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
    FGameplayTagContainer AllowedTags;

    /** Allow rotation when placing items in this container. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
    bool bAllowRotation = true;

    /** Slot-based container (ignores grid bounds, uses MaxCells for item count limit). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
    bool bSlotBased = false;

    /** Width-only validation (for hands: tall items can overflow visually). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
    bool bWidthOnlyValidation = false;
};

USTRUCT(BlueprintType)
struct PROJECTINVENTORY_API FEquipSlotContainerGrant
{
    GENERATED_BODY()

    /** Equipment slot tag that grants this container. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
    FGameplayTag EquipSlot;

    /** Container configuration granted while equipped. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
    FInventoryContainerConfig Container;
};
