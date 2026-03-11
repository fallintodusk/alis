// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "GameplayTagContainer.h"
#include "Magnitudes/MagnitudeOverride.h"
#include "InventoryTypes.generated.h"

class UProjectInventoryComponent;

// -------------------------------------------------------------------------
// Inventory Instance Data (optional per-item mutable state)
// -------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct PROJECTINVENTORY_API FInventoryInstanceData
{
	GENERATED_BODY()

	/** Durability (0-1000 scaled, 0 = broken). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance")
	int32 Durability = 1000;

	/** Current ammo count (for weapons). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance")
	int32 Ammo = 0;

	/** Item modifiers (attachments, enchantments). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance")
	TArray<FGameplayTag> Modifiers;
};

// -------------------------------------------------------------------------
// Inventory Entry (FFastArraySerializerItem)
// -------------------------------------------------------------------------

/**
 * Single inventory entry - replicated via FFastArraySerializer.
 * Uses InstanceId for stable identity (not array index).
 */
USTRUCT(BlueprintType)
struct PROJECTINVENTORY_API FInventoryEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	FInventoryEntry() = default;
	FInventoryEntry(int32 InInstanceId, FPrimaryAssetId InItemId, int32 InQuantity, int32 InSlotIndex)
		: InstanceId(InInstanceId), ItemId(InItemId), Quantity(InQuantity), SlotIndex(InSlotIndex) {}

	FInventoryEntry(int32 InInstanceId, FPrimaryAssetId InItemId, int32 InQuantity, FGameplayTag InContainerId, FIntPoint InGridPos, bool bInRotated, int32 InSlotIndex)
		: InstanceId(InInstanceId)
		, ItemId(InItemId)
		, Quantity(InQuantity)
		, SlotIndex(InSlotIndex)
		, ContainerId(InContainerId)
		, GridPos(InGridPos)
		, bRotated(bInRotated)
	{
	}

	/** Unique instance ID (stable identity across moves/reorders). */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 InstanceId = 0;

	/** Reference to item definition (FPrimaryAssetId for lightweight replication). */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FPrimaryAssetId ItemId;

	/** Stack count. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 Quantity = 1;

	/** Legacy linear slot index (derived from container + grid). */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 SlotIndex = -1;

	/** Container ID for grid placement (Item.Container.*). */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FGameplayTag ContainerId;

	/** Grid position (top-left cell) within container. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FIntPoint GridPos = FIntPoint(-1, -1);

	/** Whether the item is rotated (swap GridSize X/Y). */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	bool bRotated = false;

	/** Optional per-instance mutable data (durability, ammo, mods). */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FInventoryInstanceData InstanceData;

	/**
	 * Instance-specific magnitude overrides (copied from pickup).
	 * Applied on top of ObjectDefinition item magnitudes when item is used.
	 * Uses TArray<FMagnitudeOverride> for replication compatibility.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	TArray<FMagnitudeOverride> OverrideMagnitudes;

	// -------------------------------------------------------------------------
	// FFastArraySerializerItem callbacks
	// -------------------------------------------------------------------------

	void PreReplicatedRemove(const struct FInventoryList& InArraySerializer);
	void PostReplicatedAdd(const struct FInventoryList& InArraySerializer);
	void PostReplicatedChange(const struct FInventoryList& InArraySerializer);
};

// -------------------------------------------------------------------------
// Inventory List (FFastArraySerializer)
// -------------------------------------------------------------------------

/**
 * Replicated inventory container using FFastArraySerializer.
 * Only sends delta changes (adds, removes, modifications).
 */
USTRUCT(BlueprintType)
struct PROJECTINVENTORY_API FInventoryList : public FFastArraySerializer
{
	GENERATED_BODY()

	/** Replicated entries. */
	UPROPERTY()
	TArray<FInventoryEntry> Entries;

	// -------------------------------------------------------------------------
	// Non-replicated state
	// -------------------------------------------------------------------------

	/** Owner component (set in PostInitProperties and OnRep). NOT replicated. */
	UPROPERTY(NotReplicated)
	TObjectPtr<UProjectInventoryComponent> OwnerComponent = nullptr;

	/** Next instance ID to assign. NOT replicated (server-only). */
	uint32 NextInstanceId = 1;

	// -------------------------------------------------------------------------
	// FFastArraySerializer interface
	// -------------------------------------------------------------------------

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FInventoryEntry, FInventoryList>(Entries, DeltaParms, *this);
	}

	// -------------------------------------------------------------------------
	// Entry management
	// -------------------------------------------------------------------------

	/** Add new entry, returns assigned InstanceId. */
	uint32 AddEntry(FPrimaryAssetId ItemId, int32 Quantity, FGameplayTag ContainerId, FIntPoint GridPos, bool bRotated, int32 SlotIndex);

	/** Add entry with explicit InstanceId (for save/load). */
	uint32 AddEntryWithInstanceId(uint32 InstanceId, FPrimaryAssetId ItemId, int32 Quantity, FGameplayTag ContainerId, FIntPoint GridPos, bool bRotated, int32 SlotIndex);

	/** Remove entry by InstanceId. Returns true if found and removed. */
	bool RemoveEntry(uint32 InstanceId);

	/** Find entry by InstanceId. Returns nullptr if not found. */
	FInventoryEntry* FindEntry(uint32 InstanceId);
	const FInventoryEntry* FindEntry(uint32 InstanceId) const;

	/** Find entry by ItemId (first match). Returns nullptr if not found. */
	FInventoryEntry* FindEntryByItemId(FPrimaryAssetId ItemId);

	/** Find entry at slot index. Returns nullptr if slot is empty. */
	FInventoryEntry* FindEntryAtSlot(int32 SlotIndex);

	/** Mark entry dirty for replication. */
	void MarkEntryDirty(FInventoryEntry& Entry);

	/** Get next available slot index. Returns -1 if full. */
	int32 GetNextFreeSlot(int32 MaxSlots) const;
};

// -------------------------------------------------------------------------
// TStructOpsTypeTraits (required for NetDeltaSerialize)
// -------------------------------------------------------------------------

template<>
struct TStructOpsTypeTraits<FInventoryList> : public TStructOpsTypeTraitsBase2<FInventoryList>
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};
