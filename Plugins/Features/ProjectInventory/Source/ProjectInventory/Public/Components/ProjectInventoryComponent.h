// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Inventory/InventoryTypes.h"
#include "Inventory/InventorySaveData.h"
#include "Types/WeightState.h"
#include "Types/EquippedItemData.h"
#include "Types/InventoryContainerConfig.h"
#include "Interfaces/IInventoryCommands.h"
#include "Interfaces/IInventoryReadOnly.h"
#include "Interfaces/IInventoryWorldContainerTransferBridge.h"
#include "Interfaces/IItemDataProvider.h"
#include "Interfaces/IProjectActionReceiver.h"
#include "Loot/LootTypes.h"
#include "Types/LootEntryTypes.h"
#include "ProjectInventoryComponent.generated.h"

class UAbilitySystemComponent;
class UObjectDefinitionCache;
class UProjectSaveSubsystem;
class UProjectContainerSessionSubsystem;
class UProjectWorldContainerAuthoritySubsystem;

enum class EInventoryItemDataResolveState : uint8
{
	Invalid,
	Missing,
	Loading,
	Loaded,
	InvalidProvider,
	InvalidData
};

enum class EInventoryMoveRejectReason : uint8
{
	None,
	InvalidRequest,
	ItemDataMissing,
	ItemDataLoading,
	TargetContainerMissing,
	ItemRejectedByContainer,
	QuantityExceedsTargetStack,
	OutOfBounds,
	SplitSourceOverlap,
	MultipleTargetOverlaps,
	StackRejected,
	TargetWeightExceeded,
	TargetVolumeExceeded
};

/**
 * Terminal failure reason for the internal add path.
 * Distinct from the "deferred" state, which is not a failure.
 */
enum class EInventoryAddFailReason : uint8
{
	None,
	InvalidRequest,        // ObjectId invalid or Quantity <= 0
	CacheUnavailable,      // ObjectDefinition cache not bound; hard fail (not deferrable)
	InvalidProvider,       // Loaded object does not implement IItemDataProvider
	InvalidData,           // Loaded item data view is not valid
	NoCapacity,            // Weight/volume capacity rejected
	NoContainersAvailable, // No containers could accept any quantity
	QuantityRejected       // Placement produced zero accepted quantity
};

/**
 * Internal outcome struct for the detailed add path.
 *
 * Consumed only by InventoryInteractionHandler and related server-side callers.
 * Public Blueprint API (TryAddItem) returns only AddedQuantity for backward compatibility.
 *
 * Semantics:
 * - AddedQuantity > 0 && !bDeferred && Fail == None -> authoritative add succeeded.
 * - AddedQuantity == 0 && bDeferred == true         -> async load pending; caller must re-enter.
 * - AddedQuantity == 0 && Fail != None              -> terminal failure; no retry.
 */
struct FInventoryAddOutcome
{
	int32 AddedQuantity = 0;
	bool bDeferred = false;
	EInventoryAddFailReason Fail = EInventoryAddFailReason::None;
};

// -------------------------------------------------------------------------
// Inventory Component
// -------------------------------------------------------------------------

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInventoryChanged, UProjectInventoryComponent*, Inventory);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemAdded, FPrimaryAssetId, ObjectId, int32, Quantity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemRemoved, FPrimaryAssetId, ObjectId, int32, Quantity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnItemUsed, FPrimaryAssetId, ObjectId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInventoryError, FText, ErrorMessage);

/**
 * Server-authoritative inventory component with delta replication.
 *
 * Key patterns:
 * - Uses FPrimaryAssetId to reference item data providers (ObjectDefinition assets)
 * - FFastArraySerializer for delta replication (only changes sent)
 * - Server RPCs for all mutations (AddItem, RemoveItem, UseItem, etc.)
 * - InstanceId for stable identity (not array index)
 *
 * SOLID: Component is thin facade. Computation extracted to helpers:
 *
 * | Helper                    | Responsibility                              |
 * |---------------------------|---------------------------------------------|
 * | FInventoryGridPlacement   | Grid math, rect overlap, free cell search   |
 * | FInventoryContainerHelper | Container index, cell count, slot queries   |
 * | FInventoryStackHelper     | Stack splitting, allowed quantity calc      |
 * | FInventoryAddHelper       | Add/stack targets, new placement logic      |
 * | FInventoryMoveHelper      | Move validation, overlap detection          |
 * | FInventoryLootHelper      | CanFitItems simulation without mutation     |
 * | FInventoryViewHelper      | InventoryView building for UI               |
 * | FInventoryWeightHelper    | Weight/volume state calculations            |
 * | FInventorySaveHelper      | Save data building and restoration          |
 */
UCLASS(ClassGroup=(Inventory), meta=(BlueprintSpawnableComponent))
class PROJECTINVENTORY_API UProjectInventoryComponent : public UActorComponent, public IInventoryReadOnly, public IInventoryCommands, public IProjectActionReceiver, public IInventoryWorldContainerTransferBridge
{
	GENERATED_BODY()

	// Authority subsystem calls protected Resolved helpers. The client-side
	// ULocalPlayerSubsystem never touches protected state (it is a pure UI
	// view cache).
	friend class UProjectWorldContainerAuthoritySubsystem;

public:
	UProjectInventoryComponent();

	// -------------------------------------------------------------------------
	// UActorComponent Interface
	// -------------------------------------------------------------------------

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PostInitProperties() override;
	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// -------------------------------------------------------------------------
	// Public API (Local - call these, they route to Server RPCs)
	// -------------------------------------------------------------------------

	/** Request to add item. Routes to Server_AddItem. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	virtual void RequestAddItem(FPrimaryAssetId ObjectId, int32 Quantity = 1) override;

	/** Request to remove item. Routes to Server_RemoveItem. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	virtual void RequestRemoveItem(int32 InstanceId, int32 Quantity = 1) override;

	/** Request to move item within/between containers. Routes to Server_MoveItem. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	virtual void RequestMoveItem(int32 InstanceId, FGameplayTag FromContainer, FIntPoint FromPos, FGameplayTag ToContainer, FIntPoint ToPos, int32 Quantity, bool bRotated) override;

	/** Request to use item (consumable). Routes to Server_UseItem. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	virtual void RequestUseItem(int32 InstanceId) override;

	/** Request to equip item. Routes to Server_EquipItem. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	virtual void RequestEquipItem(int32 InstanceId, FGameplayTag EquipSlot) override;

	/** Request to unequip item. Routes to Server_UnequipItem. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	virtual void RequestUnequipItem(FGameplayTag EquipSlot) override;

	/** Request to drop item. Routes to Server_DropItem. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	virtual void RequestDropItem(int32 InstanceId, int32 Quantity = 1) override;

	/** Request to swap items between hand slots. Routes to Server_SwapHands. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	virtual void RequestSwapHands() override;

	/** Request to split a stack. Finds empty cell in same container, moves SplitQuantity there. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	virtual void RequestSplitStack(int32 InstanceId, int32 SplitQuantity) override;

	// -------------------------------------------------------------------------
	// Persistence (SaveGame/backend)
	// -------------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Inventory|Save")
	void GetSaveData(FInventorySaveData& OutData) const;

	UFUNCTION(BlueprintCallable, Category = "Inventory|Save")
	void LoadFromSaveData(const FInventorySaveData& InData, bool bApplyEquippedAbilities = false);

	/** Write inventory data into the current save (ProjectSaveSubsystem). */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Save")
	bool SaveToSaveSubsystem(UProjectSaveSubsystem* SaveSubsystem, FName SaveKey);

	/** Load inventory data from the current save (ProjectSaveSubsystem). */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Save")
	bool LoadFromSaveSubsystem(UProjectSaveSubsystem* SaveSubsystem, FName SaveKey, bool bApplyEquippedAbilities = false);

	// -------------------------------------------------------------------------
	// Server-Only Direct API (for pickups, loot, etc.)
	// -------------------------------------------------------------------------

	/**
	 * Directly add item on server. Returns quantity actually added.
	 * Use this when you need to know if the add succeeded (e.g., pickup).
	 * @return Quantity actually added (0 = failed, may be less than requested if partial)
	 *
	 * NOTE: Deferred loads (item data still streaming) appear as 0 here for
	 * backward compatibility. Callers that need to distinguish deferred from
	 * hard-failed (for retry bookkeeping) must use TryAddItemDetailed.
	 */
	int32 TryAddItem(FPrimaryAssetId ObjectId, int32 Quantity);

	/**
	 * Server-side detailed add path used by pickup orchestration (interaction
	 * handler) to distinguish deferred from terminal outcomes.
	 *
	 * The returned outcome carries one of three states:
	 * - AddedQuantity > 0            -> authoritative add succeeded
	 * - bDeferred == true            -> async load dispatched; caller must re-enter
	 * - Fail != None                 -> terminal failure; do not retry
	 *
	 * Consumed by: FInventoryInteractionHandler (pending-pickup bookkeeping).
	 */
	FInventoryAddOutcome TryAddItemDetailed(FPrimaryAssetId ObjectId, int32 Quantity);

	/**
	 * Add item with magnitude overrides (unique instance, won't stack).
	 * Use for pickups with custom magnitudes.
	 * @return InstanceId of created entry (0 = failed)
	 */
	uint32 TryAddItemWithOverrides(FPrimaryAssetId ObjectId, int32 Quantity, const TArray<FMagnitudeOverride>& Overrides);

	// -------------------------------------------------------------------------
	// Query API (Read-only)
	// -------------------------------------------------------------------------

	/** Get all inventory entries. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	const TArray<FInventoryEntry>& GetEntries() const { return Inventory.Entries; }

	/** Get read-only entry view for UI consumers. */
	virtual void GetEntriesView(TArray<FInventoryEntryView>& OutEntries) const override;

	/** Get read-only container view for UI consumers. */
	virtual void GetContainersView(TArray<FInventoryContainerView>& OutContainers) const override;

	/** Zero-allocation item check (direct scan of internal entries). */
	virtual bool ContainsItem(FPrimaryAssetId ItemId, int32 MinQuantity = 1) const override;

	/** Find entry by InstanceId. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool FindEntry(int32 InstanceId, FInventoryEntry& OutEntry) const;

	/** Find entry by ObjectId (first match). */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool FindEntryByItemId(FPrimaryAssetId ObjectId, FInventoryEntry& OutEntry) const;

	/** Get item data view for an entry. Returns false if not loaded or not an item. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool GetItemDataView(FPrimaryAssetId ObjectId, FItemDataView& OutData) const;

	/** Resolve item data state without allowing gameplay callers to bypass the cache. */
	EInventoryItemDataResolveState ResolveItemDataView(FPrimaryAssetId ObjectId, FItemDataView& OutData) const;

	/** Check if inventory has space for a 1x1 item. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool HasSpace() const;

	/** Get number of items (unique stacks). */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetItemCount() const { return Inventory.Entries.Num(); }

	/** Check if inventory contains any item with matching gameplay tag. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Query")
	bool HasItemWithTag(FGameplayTag Tag) const;

	/** Check if item is equipped. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool IsItemEquipped(int32 InstanceId) const;

	/** Get current total weight of all items in inventory. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	virtual float GetCurrentWeight() const override;

	/** Get current total volume of all items in inventory. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	virtual float GetCurrentVolume() const override;

	/** Get maximum slots for this inventory. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	virtual int32 GetMaxSlots() const override;

	/** Get maximum weight capacity (includes GAS state multiplier). */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	virtual float GetMaxWeight() const override;

	/**
	 * Get capacity multiplier based on owner's GAS state tags.
	 * Fatigue/Condition state reduces effective carrying capacity.
	 * Returns: 1.0 (healthy), 0.8 (tired), 0.6 (injured/exhausted), 0.4 (critical)
	 */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	float GetCapacityMultiplier() const;

	/** Get maximum volume capacity. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	virtual float GetMaxVolume() const override;

	/** Read-only view change delegate. */
	virtual FOnInventoryViewChanged& OnInventoryViewChanged() override { return InventoryViewChanged; }

	/** Error delegate for UI feedback (implements IInventoryReadOnly). */
	virtual FOnInventoryErrorNative& OnInventoryErrorNative() override { return InventoryErrorNative; }

	// -------------------------------------------------------------------------
	// IInventoryWorldContainerTransferBridge
	// -------------------------------------------------------------------------

	virtual bool RequestOpenWorldContainerSession_Implementation(
		AActor* TargetActor,
		EContainerSessionMode Mode,
		FText& OutError) override;

	virtual bool RequestCloseWorldContainerSession_Implementation(
		const FContainerSessionHandle& SessionHandle,
		FText& OutError) override;

	virtual bool GetActiveWorldContainerSession_Implementation(
		AActor*& OutTargetActor,
		FContainerSessionHandle& OutSessionHandle) const override;

	virtual bool TransferWorldContainerEntryToInventory_Implementation(
		UObject* WorldContainerSource,
		const FContainerSessionHandle& SessionHandle,
		int32 EntryInstanceId,
		int32 Quantity,
		FGameplayTag TargetContainerId,
		FIntPoint TargetGridPos,
		bool bTargetRotated,
		FText& OutError) override;

	virtual bool StoreInventoryEntryInWorldContainer_Implementation(
		UObject* WorldContainerSource,
		const FContainerSessionHandle& SessionHandle,
		int32 InventoryInstanceId,
		int32 Quantity,
		FIntPoint TargetGridPos,
		bool bTargetRotated,
		FText& OutError) override;

	virtual bool MoveWithinWorldContainer_Implementation(
		UObject* WorldContainerSource,
		const FContainerSessionHandle& SessionHandle,
		int32 EntryInstanceId,
		int32 Quantity,
		FIntPoint TargetGridPos,
		bool bTargetRotated,
		FText& OutError) override;

	virtual bool TakeAllFromWorldContainer_Implementation(
		UObject* WorldContainerSource,
		const FContainerSessionHandle& SessionHandle,
		FText& OutError) override;

	virtual FOnInventoryWorldContainerSessionOpenedNative& OnWorldContainerSessionOpenedNative() override
	{
		return WorldContainerSessionOpenedNative;
	}

	virtual FOnInventoryWorldContainerSessionClosedNative& OnWorldContainerSessionClosedNative() override
	{
		return WorldContainerSessionClosedNative;
	}

	// -------------------------------------------------------------------------
	// Loot Container Support (batch operations)
	// -------------------------------------------------------------------------

	/**
	 * Check if ALL items in the loot array can fit in inventory.
	 * Validates weight, slots, and stacking rules.
	 * Use for all-or-nothing loot container pickup.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Loot")
	bool CanFitItems(const TArray<FLootEntry>& Items) const;

	/**
	 * Add all items from loot array to inventory.
	 * Assumes CanFitItems() already passed - does not re-validate.
	 * Use for loot container pickup after validation.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Loot")
	void AddItemsBatch(const TArray<FLootEntry>& Items);

	/**
	 * Extract an inventory entry into a loot-entry payload for world-container store.
	 * Authority only. Removes the requested quantity from inventory on success.
	 */
	bool TryExtractContainerTransferEntry(int32 InstanceId, int32 Quantity, FContainerEntryTransfer& OutEntry, FText& OutError);

	// -------------------------------------------------------------------------
	// Cache Management
	// -------------------------------------------------------------------------

	/** Set the object definition cache (called during feature init). */
	void SetObjectDefinitionCache(UObjectDefinitionCache* InCache) { ObjectDefinitionCache = InCache; }

	/** Get the object definition cache. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	UObjectDefinitionCache* GetObjectDefinitionCache() const { return ObjectDefinitionCache; }

	// -------------------------------------------------------------------------
	// IProjectActionReceiver
	// -------------------------------------------------------------------------

	virtual void HandleAction(const FString& Context, const FString& Action) override;

	// -------------------------------------------------------------------------
	// Events (UI/Audio/Telemetry only - gameplay uses GAS)
	// -------------------------------------------------------------------------

	/** Fired when inventory contents change. */
	UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
	FOnInventoryChanged OnInventoryChanged;

	/** Fired when item is added. */
	UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
	FOnItemAdded OnItemAdded;

	/** Fired when item is removed. */
	UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
	FOnItemRemoved OnItemRemoved;

	/** Fired when item is used (AFTER GAS effects applied). */
	UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
	FOnItemUsed OnItemUsed;

	/** Fired when an inventory operation fails (for UI feedback/toast). */
	UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
	FOnInventoryError OnInventoryError;

	// -------------------------------------------------------------------------
	// FFastArraySerializer Callbacks (called by FInventoryEntry)
	// -------------------------------------------------------------------------

	void OnEntryAdded(const FInventoryEntry& Entry);
	void OnEntryRemoved(const FInventoryEntry& Entry);
	void OnEntryChanged(const FInventoryEntry& Entry);

protected:
	struct FInventoryStateSnapshot
	{
		TArray<FInventoryEntry> Entries;
		uint32 NextInstanceId = 1;
	};

	// -------------------------------------------------------------------------
	// Server RPCs
	// -------------------------------------------------------------------------

	UFUNCTION(Server, Reliable)
	void Server_AddItem(FPrimaryAssetId ObjectId, int32 Quantity);

	UFUNCTION(Server, Reliable)
	void Server_RemoveItem(int32 InstanceId, int32 Quantity);

	UFUNCTION(Server, Reliable)
	void Server_MoveItem(int32 InstanceId, FGameplayTag FromContainer, FIntPoint FromPos, FGameplayTag ToContainer, FIntPoint ToPos, int32 Quantity, bool bRotated);

	UFUNCTION(Server, Reliable)
	void Server_UseItem(int32 InstanceId);

	UFUNCTION(Server, Reliable)
	void Server_EquipItem(int32 InstanceId, FGameplayTag EquipSlot);

	UFUNCTION(Server, Reliable)
	void Server_UnequipItem(FGameplayTag EquipSlot);

	UFUNCTION(Server, Reliable)
	void Server_DropItem(int32 InstanceId, int32 Quantity);

	UFUNCTION(Server, Reliable)
	void Server_SwapHands();

	UFUNCTION(Server, Reliable)
	void Server_RequestOpenWorldContainerSession(AActor* TargetActor, EContainerSessionMode Mode);

	UFUNCTION(Server, Reliable)
	void Server_RequestCloseWorldContainerSession(AActor* TargetActor, FContainerSessionHandle SessionHandle);

	UFUNCTION(Server, Reliable)
	void Server_RequestTakeEntryFromWorldContainer(
		AActor* TargetActor,
		FContainerSessionHandle SessionHandle,
		int32 EntryInstanceId,
		int32 Quantity,
		FGameplayTag TargetContainerId,
		FIntPoint TargetGridPos,
		bool bTargetRotated);

	UFUNCTION(Server, Reliable)
	void Server_RequestStoreInventoryEntryInWorldContainer(
		AActor* TargetActor,
		FContainerSessionHandle SessionHandle,
		int32 InventoryInstanceId,
		int32 Quantity,
		FIntPoint TargetGridPos,
		bool bTargetRotated);

	UFUNCTION(Server, Reliable)
	void Server_RequestMoveWithinWorldContainer(
		AActor* TargetActor,
		FContainerSessionHandle SessionHandle,
		int32 EntryInstanceId,
		int32 Quantity,
		FIntPoint TargetGridPos,
		bool bTargetRotated);

	UFUNCTION(Server, Reliable)
	void Server_RequestTakeAllFromWorldContainer(AActor* TargetActor, FContainerSessionHandle SessionHandle);

	// -------------------------------------------------------------------------
	// Replication
	// -------------------------------------------------------------------------

	UFUNCTION()
	void OnRep_Inventory();

	// -------------------------------------------------------------------------
	// Internal Implementation
	// -------------------------------------------------------------------------

	/**
	 * Actually add item (server-side). Returns the detailed outcome.
	 *
	 * Outcome contract:
	 * - AddedQuantity > 0       -> authoritative add succeeded (possibly partial).
	 * - bDeferred == true       -> item data still streaming; caller must defer
	 *                              and re-enter after the load callback fires.
	 * - Fail != None            -> terminal failure; no retry.
	 *
	 * Callers that want the legacy "int32 added" shape can read outcome.AddedQuantity.
	 */
	FInventoryAddOutcome Internal_AddItem(FPrimaryAssetId ObjectId, int32 Quantity);

	/** Actually remove item (server-side). */
	bool Internal_RemoveItem(uint32 InstanceId, int32 Quantity);

	/** Actually move item (server-side). */
	bool Internal_MoveItem(uint32 InstanceId, FGameplayTag FromContainer, FIntPoint FromPos, FGameplayTag ToContainer, FIntPoint ToPos, int32 Quantity, bool bRotated);

	/** Actually use item (server-side). */
	bool Internal_UseItem(uint32 InstanceId);

	/** Actually equip item (server-side). */
	bool Internal_EquipItem(uint32 InstanceId, FGameplayTag EquipSlot);

	/** Actually unequip item (server-side). */
	bool Internal_UnequipItem(FGameplayTag EquipSlot);

	/**
	 * Validate-only check: can InstanceId be revoked from its equip slot
	 * prior to a removal/drop? Read-only - does NOT mutate
	 * EquippedItems or release GAS handles. Pair with
	 * Internal_CommitRevokeEquipSlotForRemoval AFTER all other failure
	 * paths (spawn service resolve, world spawn, etc.) have succeeded
	 * so the equipped state and inventory state stay atomic on failure.
	 *
	 * Mirrors Internal_UnequipItem's storage-empty validation but
	 * skips the rehome-to-hand step (the caller is destroying the
	 * item, not stowing it).
	 *
	 * @param InstanceId       item being removed/dropped
	 * @param ItemData         resolved item data for the same instance
	 * @param OutEquipSlot     populated with the slot to revoke when
	 *                         the item is currently equipped; left
	 *                         invalid when the item is not equipped
	 *                         (caller proceeds with no commit needed)
	 * @return true  - not equipped (OutEquipSlot invalid), or equipped
	 *                 AND validation passed (OutEquipSlot set; caller
	 *                 must call Internal_CommitRevokeEquipSlotForRemoval
	 *                 after success)
	 * @return false - equipped but blocked (granted storage non-empty);
	 *                 caller must abort. Player-facing error already
	 *                 broadcast via BroadcastError.
	 */
	bool Internal_CanRevokeEquipSlotForRemoval(int32 InstanceId, const FItemDataView& ItemData, FGameplayTag& OutEquipSlot);

	/**
	 * Commit step: actually revoke the equip slot. Pair with
	 * Internal_CanRevokeEquipSlotForRemoval. Pure mutation -
	 * releases GAS abilities and removes the EquippedItems map entry.
	 * No-op when EquipSlot is invalid (i.e. the item was not equipped).
	 *
	 * Without this commit step, dropping an equipped backpack from the
	 * silhouette leaves its granted container in the UI - the cells
	 * the equipped item added do not disappear after the drop.
	 *
	 * Re-entrancy guard: ExpectedInstanceId is the InstanceId the
	 * caller validated against. If the slot has been swapped to a
	 * different item between validate and commit (unusual on the
	 * server-authority RPC path, but cheap to guard), the commit is
	 * skipped with a warning rather than revoking a stranger's slot.
	 *
	 * @param EquipSlot           slot to revoke; obtained from
	 *                            Internal_CanRevokeEquipSlotForRemoval
	 * @param ExpectedInstanceId  the instance the caller validated for;
	 *                            commit is skipped if the slot now
	 *                            holds a different InstanceId
	 */
	void Internal_CommitRevokeEquipSlotForRemoval(FGameplayTag EquipSlot, int32 ExpectedInstanceId);

	/** Get ASC from owner. */
	UAbilitySystemComponent* GetOwnerASC() const;

	// -------------------------------------------------------------------------
	// Container/Grid Helpers
	// -------------------------------------------------------------------------

	void GetEffectiveContainers(TArray<FInventoryContainerConfig>& OutContainers) const;
	bool GetContainerConfig(FGameplayTag ContainerId, FInventoryContainerConfig& OutConfig) const;
	FGameplayTag GetDefaultContainerId() const;
	int32 GetContainerIndex(FGameplayTag ContainerId, const TArray<FInventoryContainerConfig>& Containers) const;
	int32 GetContainerSlotOffset(FGameplayTag ContainerId, const TArray<FInventoryContainerConfig>& Containers) const;
	int32 GetContainerCellCount(const FInventoryContainerConfig& Container) const;
	int32 ComputeSlotIndex(FGameplayTag ContainerId, FIntPoint GridPos) const;
	bool TryGetGridPosFromSlot(FGameplayTag ContainerId, int32 SlotIndex, FIntPoint& OutGridPos) const;
	FIntPoint SanitizeGridSize(FIntPoint InSize) const;
	FIntPoint GetItemGridSize(const FItemDataView& ItemData, bool bRotated) const;
	int32 GetContainerCellDepthUnits(const FInventoryContainerConfig& Container) const;
	int32 GetEffectiveMaxStackForContainer(const FInventoryContainerConfig& Container, const FItemDataView& ItemData) const;
	FInventoryContainerConfig BuildContainerConfigFromGrant(const FInventoryContainerGrantView& Grant) const;
	void UpsertContainerConfig(const FInventoryContainerConfig& GrantConfig, TArray<FInventoryContainerConfig>& OutContainers) const;
	bool IsRectWithinContainer(const FInventoryContainerConfig& Container, FIntPoint GridPos, FIntPoint ItemSize) const;
	bool DoesRectOverlap(FGameplayTag ContainerId, FIntPoint GridPos, FIntPoint ItemSize, int32 IgnoreInstanceId) const;
	bool FindFreeGridPos(const FInventoryContainerConfig& Container, FIntPoint ItemSize, int32 IgnoreInstanceId, FIntPoint& OutPos) const;
	bool GetEffectiveEntryPlacement(const FInventoryEntry& Entry, FGameplayTag& OutContainerId, FIntPoint& OutGridPos, bool& bOutRotated) const;
	float GetContainerCurrentWeight(FGameplayTag ContainerId, TMap<FPrimaryAssetId, FItemDataView>& ItemDataCache) const;
	float GetContainerCurrentVolume(FGameplayTag ContainerId, TMap<FPrimaryAssetId, FItemDataView>& ItemDataCache) const;
	bool ContainerAllowsItem(const FInventoryContainerConfig& Container, const FItemDataView& ItemData) const;
	bool GetContainerOrder(TArray<FInventoryContainerConfig>& OutContainers) const;
	bool GetEquipSlotContainerGrants(FGameplayTag EquipSlot, TArray<FInventoryContainerConfig>& OutConfigs) const;
	bool IsContainerEmpty(FGameplayTag ContainerId) const;
	bool ShouldUseLocalSave() const;
	void TryLoadInventoryFromSave();
	void TrySaveInventoryToSave();

	/** Broadcast error message to UI listeners. Routes to owning client if called on server. */
	void BroadcastError(const FText& ErrorMessage);

	/** Client RPC to deliver error message to owning client. */
	UFUNCTION(Client, Reliable)
	void Client_InventoryError(const FText& ErrorMessage);

	UFUNCTION(Client, Reliable)
	void Client_WorldContainerSessionOpened(AActor* TargetActor, FContainerSessionHandle SessionHandle);

	UFUNCTION(Client, Reliable)
	void Client_WorldContainerSessionClosed(FContainerSessionHandle SessionHandle);

	/** Actually broadcast error locally (fires delegates). */
	void BroadcastErrorLocal(const FText& ErrorMessage);

	UObject* ResolveWorldContainerSessionSource(AActor* TargetActor) const;
	AActor* ResolveWorldContainerActor(UObject* WorldContainerSource) const;
	void CaptureInventoryStateSnapshot(FInventoryStateSnapshot& OutSnapshot) const;
	void RestoreInventoryStateSnapshot(const FInventoryStateSnapshot& Snapshot);
	bool TakeEntryFromWorldContainerResolved(
		UObject* SourceObject,
		const FContainerSessionHandle& SessionHandle,
		int32 EntryInstanceId,
		int32 Quantity,
		FGameplayTag TargetContainerId,
		FIntPoint TargetGridPos,
		bool bTargetRotated,
		FText& OutError);
	bool StoreInventoryEntryInWorldContainerResolved(
		UObject* SourceObject,
		const FContainerSessionHandle& SessionHandle,
		int32 InventoryInstanceId,
		int32 Quantity,
		FIntPoint TargetGridPos,
		bool bTargetRotated,
		FText& OutError);
	bool TakeAllFromWorldContainerResolved(
		UObject* SourceObject,
		const FContainerSessionHandle& SessionHandle,
		FText& OutError);
	bool TryAddItemAtPosition(
		FPrimaryAssetId ObjectId,
		int32 Quantity,
		FGameplayTag TargetContainerId,
		FIntPoint TargetGridPos,
		bool bTargetRotated,
		FText& OutError);
	void HandleWorldContainerSessionOpenedLocal(AActor* TargetActor, const FContainerSessionHandle& SessionHandle);
	void HandleWorldContainerSessionClosedLocal(const FContainerSessionHandle& SessionHandle);
	void BindObjectDefinitionCache();
	void LogItemDataResolveState(FPrimaryAssetId ObjectId, EInventoryItemDataResolveState ResolveState) const;
	void LogMoveReject(const TCHAR* Context, EInventoryMoveRejectReason RejectReason, int32 InstanceId) const;
	void RejectMove(const TCHAR* Context, EInventoryMoveRejectReason RejectReason, int32 InstanceId);
	static const TCHAR* LexToString(EInventoryItemDataResolveState ResolveState);
	static const TCHAR* LexToString(EInventoryMoveRejectReason RejectReason);
	static FText MakeMoveRejectText(EInventoryMoveRejectReason RejectReason);

	/** Update weight state tag on ASC based on current weight ratio. */
	void UpdateWeightStateTag();

	/** Compute weight state from percentage. */
	EWeightState ComputeWeightState(float WeightPercent) const;

	/** Set a state tag on ASC (removes other children of parent tag first). */
	void SetStateTag(UAbilitySystemComponent* ASC, const FGameplayTag& NewTag, const FGameplayTag& ParentTag);

	/** Previous weight state (for hysteresis). */
	EWeightState PrevWeightState = EWeightState::Light;

	/** True after first weight tag update (ensures tag is set on init). */
	bool bWeightTagInitialized = false;

	/** Bind to ASC tag events for capacity-affecting tags (fatigue, condition). */
	void BindCapacityTagEvents();

	/** Called when fatigue/condition tags change - refreshes weight state. */
	void OnCapacityTagChanged(const FGameplayTag Tag, int32 NewCount);

	// -------------------------------------------------------------------------
	// Replicated State
	// -------------------------------------------------------------------------

	UPROPERTY(ReplicatedUsing = OnRep_Inventory)
	FInventoryList Inventory;

	// -------------------------------------------------------------------------
	// Configuration
	// -------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	int32 MaxSlots = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	float MaxWeight = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	float MaxVolume = 100.0f;

	// -------------------------------------------------------------------------
	// Container Configuration
	// -------------------------------------------------------------------------

	/** Optional container list. If empty, a default container is generated from MaxSlots/MaxWeight/MaxVolume. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Containers")
	TArray<FInventoryContainerConfig> Containers;

	/**
	 * Legacy compatibility toggle:
	 * - false (default): runtime uses hands/default + equipped grants only (vision-compliant).
	 * - true: include configured Containers as always-available base containers.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Containers")
	bool bIncludeConfiguredContainersAsBase = false;

	/** Default container ID when Containers is empty or when item placement has no explicit container. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Containers")
	FGameplayTag DefaultContainerId;

	/** Default grid width when Containers is empty (used to derive grid size from MaxSlots). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Containers")
	int32 DefaultContainerGridWidth = 2;

	/** Containers granted by equipped items (slot -> one or more containers). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Containers")
	TArray<FEquipSlotContainerGrant> EquipSlotContainerGrants;

	// -------------------------------------------------------------------------
	// Non-Replicated State
	// -------------------------------------------------------------------------

	/** Equipped items by slot tag (InstanceId + GrantedHandles). */
	UPROPERTY()
	TMap<FGameplayTag, FEquippedItemData> EquippedItems;

	/** Object definition cache (set during feature init). */
	UPROPERTY()
	UObjectDefinitionCache* ObjectDefinitionCache = nullptr;

	/** Read-only view change delegate for UI consumers. */
	FOnInventoryViewChanged InventoryViewChanged;

	/** Error delegate for C++ consumers (IInventoryReadOnly interface). */
	FOnInventoryErrorNative InventoryErrorNative;

	FOnInventoryWorldContainerSessionOpenedNative WorldContainerSessionOpenedNative;
	FOnInventoryWorldContainerSessionClosedNative WorldContainerSessionClosedNative;

	// Session state (active session handle, target actor, instigator) is
	// owned by UProjectWorldContainerAuthoritySubsystem on the server and
	// cached by UProjectContainerSessionSubsystem on the client. The
	// component does not hold a "current session" member -- it queries
	// the authority subsystem when it needs to answer session questions.

	mutable TMap<FPrimaryAssetId, EInventoryItemDataResolveState> LoggedItemResolveStates;
};
