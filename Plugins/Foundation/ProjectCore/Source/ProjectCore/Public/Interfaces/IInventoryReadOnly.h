// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "UObject/PrimaryAssetId.h"
#include "IInventoryReadOnly.generated.h"

/**
 * Lightweight inventory entry view for UI and read-only consumers.
 * Does NOT expose internal storage or replication details.
 */
USTRUCT(BlueprintType)
struct PROJECTCORE_API FInventoryEntryView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FPrimaryAssetId ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 InstanceId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FText Description;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FString IconCode;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 Quantity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 SlotIndex = -1;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FGameplayTag EquipSlotTag;

	/** Weight per item (kg). */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	float Weight = 0.0f;

	/** Volume per item (L). */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	float Volume = 0.0f;

	/** Grid size (X=width, Y=height). */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FIntPoint GridSize = FIntPoint(1, 1);

	/** Maximum stack size (1 = non-stackable). */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 MaxStack = 1;

	/** Whether item can be dropped. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	bool bCanBeDropped = true;

	/** Whether item supports "Use" action (consumable behavior). */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	bool bIsConsumable = false;

	/**
	 * Effective consumable magnitudes applied on use.
	 * Includes definition defaults plus per-instance overrides.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item")
	TMap<FGameplayTag, float> UseMagnitudes;

	/** Explicit capability flag for presenting "Use" action in UI. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	bool bCanUse = false;

	/** Explicit capability flag for presenting "Equip" action in UI. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	bool bCanEquip = false;

	/**
	 * Migration marker: producer sets true when bCanUse/bCanEquip are intentionally
	 * populated (including explicit false).
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	bool bActionCapsPopulated = false;

	/** Container/grid placement (used by grid UI). */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FGameplayTag ContainerId;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FIntPoint GridPos = FIntPoint(-1, -1);

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	bool bRotated = false;

	/** Equipped slot if currently equipped (invalid if not equipped). */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FGameplayTag EquippedSlot;

	/** Containers granted while equipped (empty if none or not equipped). */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	TArray<FGameplayTag> GrantedContainerIds;

	// -------------------------------------------------------------------------
	// Instance Data (mutable per-item state)
	// -------------------------------------------------------------------------

	/** Durability (0-1000 scaled, 0 = broken). */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Instance")
	int32 Durability = 1000;

	/** Maximum durability (for calculating %). */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Instance")
	int32 MaxDurability = 1000;

	/** Current ammo count (for weapons). */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Instance")
	int32 Ammo = 0;

	/** Item modifiers (attachments, enchantments). */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Instance")
	TArray<FGameplayTag> Modifiers;
};

/**
 * Lightweight container view for UI and read-only consumers.
 * Exposes grid size and capacity without revealing inventory internals.
 */
USTRUCT(BlueprintType)
struct PROJECTCORE_API FInventoryContainerView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FGameplayTag ContainerId;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FIntPoint GridSize = FIntPoint(0, 0);

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	float CurrentWeight = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	float MaxWeight = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	float CurrentVolume = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	float MaxVolume = 0.0f;

	/** Optional hard cap on total cells used (0 = unlimited). */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 MaxCells = 0;

	/** Slot-based container (ignores item grid size, uses slots instead). */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	bool bSlotBased = false;
};

/** Fired when the inventory view changes (C++ only). */
DECLARE_MULTICAST_DELEGATE(FOnInventoryViewChanged);

/** Fired when an inventory error occurs (for UI feedback). */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnInventoryErrorNative, const FText&);

UINTERFACE(MinimalAPI, BlueprintType)
class UInventoryReadOnly : public UInterface
{
	GENERATED_BODY()
};

/**
 * Read-only inventory interface for UI consumers.
 * Inventory implementation owns storage and rules; UI only reads snapshots.
 */
class PROJECTCORE_API IInventoryReadOnly
{
	GENERATED_BODY()

public:
	virtual void GetEntriesView(TArray<FInventoryEntryView>& OutEntries) const = 0;
	virtual void GetContainersView(TArray<FInventoryContainerView>& OutContainers) const = 0;
	virtual float GetCurrentWeight() const = 0;
	virtual float GetMaxWeight() const = 0;
	virtual float GetCurrentVolume() const = 0;
	virtual float GetMaxVolume() const = 0;
	virtual int32 GetMaxSlots() const = 0;
	virtual FOnInventoryViewChanged& OnInventoryViewChanged() = 0;

	/** Get error delegate for UI feedback (e.g., "Too heavy", "No space"). */
	virtual FOnInventoryErrorNative& OnInventoryErrorNative() = 0;

	/**
	 * Check if inventory contains an item matching ItemId with at least MinQuantity.
	 * Default: linear scan via GetEntriesView. Override for zero-allocation efficiency.
	 */
	virtual bool ContainsItem(FPrimaryAssetId ItemId, int32 MinQuantity = 1) const
	{
		TArray<FInventoryEntryView> Entries;
		GetEntriesView(Entries);
		for (const FInventoryEntryView& Entry : Entries)
		{
			if (Entry.ItemId == ItemId && Entry.Quantity >= MinQuantity)
			{
				return true;
			}
		}
		return false;
	}
};
