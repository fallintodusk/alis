// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "UObject/SoftObjectPath.h"
#include "IItemDataProvider.generated.h"

/**
 * Lightweight item data view for inventory/UI logic.
 * Kept in ProjectCore to avoid hard dependencies on ProjectObject data types.
 */
USTRUCT(BlueprintType)
struct PROJECTCORE_API FInventoryContainerGrantView
{
	GENERATED_BODY()

	/** Container identifier (Item.Container.*). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FGameplayTag ContainerId;

	/** Grid size in cells (X=width, Y=height). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FIntPoint GridSize = FIntPoint(0, 0);

	/** Optional max weight for this container (0 = unlimited). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	float MaxWeight = 0.0f;

	/** Optional max volume for this container (0 = unlimited). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	float MaxVolume = 0.0f;

	/** Optional hard cap on total cells used (0 = unlimited). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	int32 MaxCells = 0;

	/** Optional tag filter for items allowed in this container. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FGameplayTagContainer AllowedTags;

	/** Allow rotation when placing items in this container. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	bool bAllowRotation = true;
};

USTRUCT(BlueprintType)
struct PROJECTCORE_API FItemDataView
{
	GENERATED_BODY()

	/** True if item data is present. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	bool bIsValid = false;

	/** UI display name. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	/** UI description text. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FText Description;

	/** Icon font codepoint for UI (Game Icons font, e.g. "\uF88D"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FString IconCode;

	/** Item tags for filtering/queries. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FGameplayTagContainer Tags;

	/** Weight in kg. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	float Weight = 0.0f;

	/** Volume in liters. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	float Volume = 0.0f;

	/** Grid size (X=width, Y=height) for grid inventories. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FIntPoint GridSize = FIntPoint(1, 1);

	/** Maximum stack size (1 = non-stackable). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	int32 MaxStack = 1;

	/** Whether item can be dropped. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	bool bCanBeDropped = true;

	/** Whether item is consumed after use. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	bool bConsumeOnUse = true;

	/** Magnitudes for consumable effects. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	TMap<FGameplayTag, float> Magnitudes;

	/** Equipment slot tag (Item.EquipmentSlot.*). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FGameplayTag EquipSlotTag;

	/** Ability set to grant when equipped (soft path, loaded by inventory/GAS). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FSoftObjectPath EquipAbilitySetPath;

	/** Containers granted while equipped. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	TArray<FInventoryContainerGrantView> ContainerGrants;

	/** Cached flags for quick checks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	bool bIsConsumable = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	bool bIsEquipment = false;

	bool IsValid() const { return bIsValid; }
	bool IsConsumable() const { return bIsConsumable; }
	bool IsEquipment() const { return bIsEquipment; }
};

/**
 * IItemDataProvider
 *
 * Interface for assets that expose item data to systems like Inventory.
 * Implemented by ObjectDefinition in ProjectObject plugin.
 */
UINTERFACE(MinimalAPI, BlueprintType)
class UItemDataProvider : public UInterface
{
	GENERATED_BODY()
};

class PROJECTCORE_API IItemDataProvider
{
	GENERATED_BODY()

public:
	/** Get item data view (bIsValid = false if no item data). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Item")
	FItemDataView GetItemDataView() const;
};
