// Copyright ALIS. All Rights Reserved.
//
// ProjectInventoryComponent add/remove/move mutation methods.
//
// Split out of ProjectInventoryComponent.cpp on 2026-04-23 under the
// FILE SIZE GUARDRAIL rule (AGENTS.md). The class declaration is unchanged;
// this TU defines the server-authority mutation surface:
//
//   Exact-position add:
//     - TryAddItemAtPosition
//   Public server-only add API:
//     - TryAddItem / TryAddItemDetailed / TryAddItemWithOverrides
//   Internal mutation ops:
//     - Internal_AddItem  (detailed outcome: added/deferred/failed)
//     - Internal_RemoveItem
//     - Internal_MoveItem
//   Server RPC impls (client -> authority for the mutation path):
//     - Server_AddItem_Implementation
//     - Server_RemoveItem_Implementation
//     - Server_MoveItem_Implementation
//     - Server_DropItem_Implementation
//
// Helpers shared with other split TUs live in
// Private/Components/ProjectInventoryComponentInternals.h.

#include "Components/ProjectInventoryComponent.h"
#include "Components/ProjectInventoryComponentInternals.h"

#include "Abilities/ProjectAbilitySet.h"
#include "AbilitySystemComponent.h"
#include "Helpers/InventoryAddHelper.h"
#include "Helpers/InventoryMoveHelper.h"
#include "Helpers/InventoryStackHelper.h"
#include "Interfaces/IPickupSource.h"
#include "ProjectGameplayTags.h"
#include "ProjectInventory.h"
#include "ProjectServiceLocator.h"
#include "Services/IObjectSpawnService.h"
#include "Subsystems/ProjectObjectDefinitionCacheSubsystem.h"

#include "Modules/ModuleManager.h"

using namespace ProjectInventoryInternal;

// -------------------------------------------------------------------------
// Server RPC impls (mutation)
// -------------------------------------------------------------------------

void UProjectInventoryComponent::Server_AddItem_Implementation(FPrimaryAssetId ObjectId, int32 Quantity)
{
	Internal_AddItem(ObjectId, Quantity);
	// FFastArraySerializer callbacks only fire on receiving clients, not on
	// the authority (listen server / standalone). Broadcast explicitly so
	// local ViewModel listeners refresh immediately.
	UE_LOG(LogProjectInventory, Verbose, TEXT("Server_AddItem: authority broadcast"));
	InventoryViewChanged.Broadcast();
}

void UProjectInventoryComponent::Server_RemoveItem_Implementation(int32 InstanceId, int32 Quantity)
{
	Internal_RemoveItem(InstanceId, Quantity);
	UE_LOG(LogProjectInventory, Verbose, TEXT("Server_RemoveItem: authority broadcast"));
	InventoryViewChanged.Broadcast();
}

void UProjectInventoryComponent::Server_MoveItem_Implementation(int32 InstanceId, FGameplayTag FromContainer, FIntPoint FromPos, FGameplayTag ToContainer, FIntPoint ToPos, int32 Quantity, bool bRotated)
{
	Internal_MoveItem(InstanceId, FromContainer, FromPos, ToContainer, ToPos, Quantity, bRotated);
	UE_LOG(LogProjectInventory, Verbose, TEXT("Server_MoveItem: authority broadcast"));
	InventoryViewChanged.Broadcast();
}

bool UProjectInventoryComponent::Internal_CanRevokeEquipSlotForRemoval(
	int32 InstanceId, const FItemDataView& ItemData, FGameplayTag& OutEquipSlot)
{
	OutEquipSlot = FGameplayTag();

	// Locate the equip slot (if any) currently holding this instance.
	for (const auto& Pair : EquippedItems)
	{
		if (Pair.Value.InstanceId == InstanceId)
		{
			OutEquipSlot = Pair.Key;
			break;
		}
	}
	if (!OutEquipSlot.IsValid())
	{
		// Not equipped - caller proceeds with normal removal, no commit needed.
		return true;
	}

	// Storage-empty validation. Mirrors Internal_UnequipItem so the
	// removal path can't orphan items inside a granted container
	// (e.g. backpack contents). Item-level grants take precedence over
	// slot-level fallback grants - same precedence used by the equip
	// path in GetEffectiveContainers.
	TArray<FGameplayTag> GrantedContainerIds;
	if (ItemData.ContainerGrants.Num() > 0)
	{
		for (const FInventoryContainerGrantView& Grant : ItemData.ContainerGrants)
		{
			if (Grant.ContainerId.IsValid())
			{
				GrantedContainerIds.Add(Grant.ContainerId);
			}
		}
	}
	else
	{
		TArray<FInventoryContainerConfig> SlotGrants;
		if (GetEquipSlotContainerGrants(OutEquipSlot, SlotGrants))
		{
			for (const FInventoryContainerConfig& Cfg : SlotGrants)
			{
				if (Cfg.ContainerId.IsValid())
				{
					GrantedContainerIds.Add(Cfg.ContainerId);
				}
			}
		}
	}

	for (const FGameplayTag& Cid : GrantedContainerIds)
	{
		// Hand-destination grants are routes for the unequipped item, not
		// extension storage that owns its own occupancy - same skip used
		// by Internal_UnequipItem.
		if (IsHandDestinationContainer(Cid))
		{
			continue;
		}
		if (!IsContainerEmpty(Cid))
		{
			UE_LOG(LogProjectInventory, Warning,
				TEXT("Internal_CanRevokeEquipSlotForRemoval: granted storage %s not empty - blocking removal of equipped InstanceId=%d in slot %s"),
				*Cid.ToString(), InstanceId, *OutEquipSlot.ToString());
			BroadcastError(NSLOCTEXT("Inventory", "DropEquippedBlockedByStorage",
				"Cannot drop - empty the equipped item's storage first"));
			return false;
		}
	}

	return true;
}

void UProjectInventoryComponent::Internal_CommitRevokeEquipSlotForRemoval(
	FGameplayTag EquipSlot, int32 ExpectedInstanceId)
{
	if (!EquipSlot.IsValid())
	{
		// No-op: validation reported the item wasn't equipped.
		return;
	}

	FEquippedItemData* EquipData = EquippedItems.Find(EquipSlot);
	if (!EquipData)
	{
		// Race or programming error: validation said this slot held our
		// item, but the map no longer has the entry. Nothing to clean up.
		UE_LOG(LogProjectInventory, Warning,
			TEXT("Internal_CommitRevokeEquipSlotForRemoval: slot %s no longer in EquippedItems (concurrent modification?)"),
			*EquipSlot.ToString());
		return;
	}

	// Re-entrancy guard: skip the commit if the slot has been swapped
	// to a different item between validate and commit. Server-authority
	// RPC dispatch is single-threaded so this should not normally
	// happen, but the check is cheap and prevents revoking a
	// stranger's equip slot if a future refactor introduces
	// re-entrant mutation.
	if (EquipData->InstanceId != ExpectedInstanceId)
	{
		UE_LOG(LogProjectInventory, Warning,
			TEXT("Internal_CommitRevokeEquipSlotForRemoval: slot %s now holds InstanceId=%d, expected=%d - skipping commit"),
			*EquipSlot.ToString(), EquipData->InstanceId, ExpectedInstanceId);
		return;
	}

	// GAS cleanup + remove from equipped map. Caller must invoke this
	// only AFTER all failure paths have passed so equip state and
	// inventory state stay consistent.
	if (UAbilitySystemComponent* ASC = GetOwnerASC())
	{
		UProjectAbilitySet::TakeFromAbilitySystem(ASC, &EquipData->GrantedHandles);
	}
	EquippedItems.Remove(EquipSlot);

	UE_LOG(LogProjectInventory, Log,
		TEXT("Committed revoke of equip slot %s (InstanceId=%d removed/dropped)"),
		*EquipSlot.ToString(), ExpectedInstanceId);
}

void UProjectInventoryComponent::Server_DropItem_Implementation(int32 InstanceId, int32 Quantity)
{
	FInventoryEntry* Entry = Inventory.FindEntry(InstanceId);
	if (!Entry)
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("Server_DropItem: InstanceId %d not found"), InstanceId);
		return;
	}

	FItemDataView ItemData;
	const EInventoryItemDataResolveState ResolveState = ResolveItemDataView(Entry->ItemId, ItemData);
	if (ResolveState != EInventoryItemDataResolveState::Loaded || !ItemData.bCanBeDropped)
	{
		UE_LOG(
			LogProjectInventory,
			Warning,
			TEXT("Server_DropItem: Item cannot be dropped (state=%s, canDrop=%d, ItemId=%s)"),
			LexToString(ResolveState),
			ItemData.bCanBeDropped ? 1 : 0,
			*Entry->ItemId.ToString());
		return;
	}

	// Clamp quantity to available
	const int32 DropQuantity = FMath::Min(Quantity, Entry->Quantity);
	if (DropQuantity <= 0)
	{
		return;
	}

	// Copy ItemId before any modification (Entry* may become invalid)
	const FPrimaryAssetId DroppedItemId = Entry->ItemId;

	// Calculate drop transform (in front of owner)
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// Equip-grant validate-only check BEFORE the spawn. If this item is
	// currently equipped, the helper validates its granted storage is
	// empty and stamps EquipSlotToRevoke. The actual COMMIT (GAS
	// cleanup + EquippedItems map removal) happens AFTER the spawn
	// succeeds, so a spawn failure cannot leave the equipped state
	// out of sync with the inventory state. Without this, dropping an
	// equipped backpack from the silhouette would leave its granted
	// container in the UI - the cells the equipped item added do not
	// disappear after the drop.
	FGameplayTag EquipSlotToRevoke;
	if (!Internal_CanRevokeEquipSlotForRemoval(InstanceId, ItemData, EquipSlotToRevoke))
	{
		// Storage non-empty; player-facing error already broadcast.
		return;
	}

	FVector DropLocation = Owner->GetActorLocation();
	FRotator DropRotation = Owner->GetActorRotation();

	// Offset forward and down
	const FVector ForwardOffset = Owner->GetActorForwardVector() * 100.f;
	const FVector DownOffset = FVector(0.f, 0.f, -50.f);
	DropLocation += ForwardOffset + DownOffset;

	const FTransform DropTransform(DropRotation, DropLocation);

	// Resolve spawn service (lazy-load module if needed)
	TSharedPtr<IObjectSpawnService> SpawnService = FProjectServiceLocator::Resolve<IObjectSpawnService>();
	if (!SpawnService)
	{
		// Try loading module (string-only, no compile-time dependency)
		FModuleManager::Get().LoadModule(TEXT("ProjectObject"));
		SpawnService = FProjectServiceLocator::Resolve<IObjectSpawnService>();
	}
	if (!SpawnService)
	{
		UE_LOG(LogProjectInventory, Error, TEXT("Server_DropItem: IObjectSpawnService not available after module load"));
		return;
	}

	// Spawn BEFORE removing (transactional - don't lose item on spawn failure)
	FText SpawnError;
	AActor* DroppedActor = SpawnService->SpawnFromDefinition(
		GetWorld(),
		DroppedItemId,
		DropTransform,
		&SpawnError
	);

	if (!DroppedActor)
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("Server_DropItem: Spawn failed - %s (item NOT removed)"), *SpawnError.ToString());
		return;
	}

	auto FindPickupSourceOnActor = [](AActor* Actor) -> UObject*
	{
		if (!Actor)
		{
			return nullptr;
		}

		if (Actor->Implements<UPickupSource>())
		{
			return Actor;
		}

		TInlineComponentArray<UActorComponent*> Components;
		Actor->GetComponents(Components);
		for (UActorComponent* Component : Components)
		{
			if (Component && Component->Implements<UPickupSource>())
			{
				return Component;
			}
		}

		return nullptr;
	};

	if (UObject* PickupSource = FindPickupSourceOnActor(DroppedActor))
	{
		IPickupSource::Execute_SetQuantity(PickupSource, DropQuantity);
	}

	// Spawn succeeded - now commit the equip-slot revoke (no-op if not
	// equipped) and remove from inventory. Doing the revoke commit here
	// (after the last failure path) keeps equipped state and inventory
	// state atomic: either both mutate or neither. Pass InstanceId as
	// the re-entrancy expectation - the slot must still hold the same
	// item we validated.
	Internal_CommitRevokeEquipSlotForRemoval(EquipSlotToRevoke, InstanceId);
	Internal_RemoveItem(InstanceId, DropQuantity);

	UE_LOG(LogProjectInventory, Log, TEXT("Dropped %d x %s at %s"),
		DropQuantity, *DroppedItemId.ToString(), *DropLocation.ToString());
	UE_LOG(LogProjectInventory, Verbose, TEXT("Server_DropItem: authority broadcast"));
	InventoryViewChanged.Broadcast();
}


// -------------------------------------------------------------------------
// Public server-only add API
// -------------------------------------------------------------------------

int32 UProjectInventoryComponent::TryAddItem(FPrimaryAssetId ObjectId, int32 Quantity)
{
	// Server-only: public API must enforce authority
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("TryAddItem: Called without authority"));
		return 0;
	}

	// Legacy int32 shape — deferred outcomes collapse to 0. Pickup orchestration
	// that needs deferred-vs-terminal discrimination uses TryAddItemDetailed.
	return Internal_AddItem(ObjectId, Quantity).AddedQuantity;
}

FInventoryAddOutcome UProjectInventoryComponent::TryAddItemDetailed(FPrimaryAssetId ObjectId, int32 Quantity)
{
	FInventoryAddOutcome Outcome;
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("TryAddItemDetailed: Called without authority"));
		Outcome.Fail = EInventoryAddFailReason::InvalidRequest;
		return Outcome;
	}

	return Internal_AddItem(ObjectId, Quantity);
}

// SOLID: Uses FInventoryStackHelper + FInventoryAddHelper for placement
uint32 UProjectInventoryComponent::TryAddItemWithOverrides(FPrimaryAssetId ObjectId, int32 Quantity, const TArray<FMagnitudeOverride>& Overrides)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return 0;
	}

	if (!ObjectId.IsValid() || Quantity <= 0)
	{
		return 0;
	}

	FItemDataView ItemData;
	if (!GetItemDataView(ObjectId, ItemData))
	{
		return 0;
	}

	// Calculate allowed quantity
	const int32 AllowedQuantity = FInventoryStackHelper::CalculateAllowedQuantity(
		ItemData, GetMaxWeight(), GetMaxVolume(), GetCurrentWeight(), GetCurrentVolume(), Quantity);
	if (AllowedQuantity <= 0)
	{
		return 0;
	}

	// Build container states and find placement (no stacking - items with overrides are unique)
	TArray<FInventoryContainerConfig> ContainerOrder;
	if (!GetContainerOrder(ContainerOrder))
	{
		return 0;
	}

	TMap<FPrimaryAssetId, FItemDataView> ItemDataCache;
	ItemDataCache.Add(ObjectId, ItemData);

	TArray<FInventoryAddHelper::FContainerState> ContainerStates;
	for (const FInventoryContainerConfig& Container : ContainerOrder)
	{
		FInventoryAddHelper::FContainerState State;
		State.Config = Container;
		State.CurrentWeight = GetContainerCurrentWeight(Container.ContainerId, ItemDataCache);
		State.CurrentVolume = GetContainerCurrentVolume(Container.ContainerId, ItemDataCache);
		ContainerStates.Add(State);
	}

	FInventoryAddHelper::FAddCallbacks Callbacks;
	Callbacks.FindFreeGridPos = [this](const FInventoryContainerConfig& C, FIntPoint S, int32 I, FIntPoint& P) {
		return FindFreeGridPos(C, S, I, P);
	};
	Callbacks.ContainerAllowsItem = [this](const FInventoryContainerConfig& C, const FItemDataView& D) {
		return ContainerAllowsItem(C, D);
	};
	Callbacks.GetEffectiveMaxStack = [this](const FInventoryContainerConfig& C, const FItemDataView& D) {
		return GetEffectiveMaxStackForContainer(C, D);
	};

	// Add entries one at a time to avoid overlapping placements
	uint32 FirstInstanceId = 0;
	int32 RemainingQuantity = AllowedQuantity;

	while (RemainingQuantity > 0)
	{
		TArray<FInventoryAddHelper::FNewStackPlacement> Placements;
		FInventoryAddHelper::CalculateNewPlacements(ItemData, RemainingQuantity, ContainerStates, Callbacks, Placements);

		if (Placements.Num() == 0)
		{
			break;
		}

		const FInventoryAddHelper::FNewStackPlacement& Placement = Placements[0];
		const int32 SlotIndex = ComputeSlotIndex(Placement.ContainerId, Placement.GridPos);
		const uint32 InstanceId = Inventory.AddEntry(ObjectId, Placement.Quantity, Placement.ContainerId, Placement.GridPos, Placement.bRotated, SlotIndex);

		if (FInventoryEntry* Entry = Inventory.FindEntry(InstanceId))
		{
			Entry->OverrideMagnitudes = Overrides;
			Inventory.MarkEntryDirty(*Entry);
		}

		if (FirstInstanceId == 0)
		{
			FirstInstanceId = InstanceId;
		}

		RemainingQuantity -= Placement.Quantity;
		UE_LOG(LogProjectInventory, Log, TEXT("Added item with overrides: %d x %s (InstanceId: %d, %d overrides)"),
			Placement.Quantity, *ObjectId.ToString(), InstanceId, Overrides.Num());
	}

	return FirstInstanceId;
}

// -------------------------------------------------------------------------
// Internal mutation ops
// -------------------------------------------------------------------------

FInventoryAddOutcome UProjectInventoryComponent::Internal_AddItem(FPrimaryAssetId ObjectId, int32 Quantity)
{
	FInventoryAddOutcome Outcome;

	if (!ObjectId.IsValid() || Quantity <= 0)
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("Internal_AddItem: Invalid ObjectId or Quantity"));
		Outcome.Fail = EInventoryAddFailReason::InvalidRequest;
		// Hard, non-deferrable: broadcast so pickups never silently skip.
		BroadcastError(NSLOCTEXT("Inventory", "InvalidRequest", "Invalid pickup request"));
		return Outcome;
	}

	FItemDataView ItemData;
	const EInventoryItemDataResolveState ResolveState = ResolveItemDataView(ObjectId, ItemData);
	if (ResolveState != EInventoryItemDataResolveState::Loaded)
	{
		// CRITICAL: do NOT silently return 0 on Missing/Loading — the pickup
		// orchestrator must see that the add is deferrable vs. terminal.
		switch (ResolveState)
		{
			case EInventoryItemDataResolveState::Missing:
			case EInventoryItemDataResolveState::Loading:
				// Deferrable: the item data is not yet resident. The interaction
				// handler will call ObjectDefinitionCache::RequestLoad and re-enter
				// this path once the cache resolves.
				UE_LOG(
					LogProjectInventory,
					Log,
					TEXT("Internal_AddItem: deferred (%s) for %s — handler will RequestLoad"),
					LexToString(ResolveState),
					*ObjectId.ToString());
				Outcome.bDeferred = true;
				return Outcome;

			case EInventoryItemDataResolveState::InvalidProvider:
				UE_LOG(
					LogProjectInventory,
					Warning,
					TEXT("Internal_AddItem: InvalidProvider for %s (loaded object does not implement IItemDataProvider)"),
					*ObjectId.ToString());
				Outcome.Fail = EInventoryAddFailReason::InvalidProvider;
				BroadcastError(NSLOCTEXT("Inventory", "InvalidProvider", "That item is unavailable"));
				return Outcome;

			case EInventoryItemDataResolveState::InvalidData:
				UE_LOG(
					LogProjectInventory,
					Warning,
					TEXT("Internal_AddItem: InvalidData for %s"),
					*ObjectId.ToString());
				Outcome.Fail = EInventoryAddFailReason::InvalidData;
				BroadcastError(NSLOCTEXT("Inventory", "InvalidData", "That item is unavailable"));
				return Outcome;

			case EInventoryItemDataResolveState::Invalid:
			default:
				UE_LOG(
					LogProjectInventory,
					Warning,
					TEXT("Internal_AddItem: Cache unavailable / invalid state (%s) for %s"),
					LexToString(ResolveState),
					*ObjectId.ToString());
				Outcome.Fail = EInventoryAddFailReason::CacheUnavailable;
				BroadcastError(NSLOCTEXT("Inventory", "CacheUnavailable", "Cannot pick up right now"));
				return Outcome;
		}
	}

	// Calculate allowed quantity based on weight/volume
	const int32 AllowedQuantity = FInventoryStackHelper::CalculateAllowedQuantity(
		ItemData, GetMaxWeight(), GetMaxVolume(), GetCurrentWeight(), GetCurrentVolume(), Quantity);

	UE_LOG(LogProjectInventory, Log, TEXT("Internal_AddItem: %s x%d (size: %dx%d, weight: %.2f) -> allowed: %d"),
		*ObjectId.ToString(), Quantity, ItemData.GridSize.X, ItemData.GridSize.Y, ItemData.Weight, AllowedQuantity);

	if (AllowedQuantity <= 0)
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("Internal_AddItem: No capacity for %s (weight: %.2f/%.2f, volume: %.2f/%.2f)"),
			*ObjectId.ToString(), GetCurrentWeight(), GetMaxWeight(), GetCurrentVolume(), GetMaxVolume());
		BroadcastError(NSLOCTEXT("Inventory", "NoCapacity", "Cannot carry more"));
		Outcome.Fail = EInventoryAddFailReason::NoCapacity;
		return Outcome;
	}

	TArray<FInventoryContainerConfig> ContainerOrder;
	if (!GetContainerOrder(ContainerOrder))
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("Internal_AddItem: No containers available"));
		BroadcastError(NSLOCTEXT("Inventory", "NoContainers", "No containers available"));
		Outcome.Fail = EInventoryAddFailReason::NoContainersAvailable;
		return Outcome;
	}

	// Build container states for helper
	TMap<FPrimaryAssetId, FItemDataView> ItemDataCache;
	ItemDataCache.Add(ObjectId, ItemData);

	TArray<FInventoryAddHelper::FContainerState> ContainerStates;
	ContainerStates.Reserve(ContainerOrder.Num());
	for (const FInventoryContainerConfig& Container : ContainerOrder)
	{
		FInventoryAddHelper::FContainerState State;
		State.Config = Container;
		State.CurrentWeight = GetContainerCurrentWeight(Container.ContainerId, ItemDataCache);
		State.CurrentVolume = GetContainerCurrentVolume(Container.ContainerId, ItemDataCache);
		ContainerStates.Add(State);

		UE_LOG(LogProjectInventory, Log, TEXT("  Container: %s (%dx%d, slot:%d, w:%.1f/%.1f, v:%.1f/%.1f)"),
			*Container.ContainerId.ToString(), Container.GridSize.X, Container.GridSize.Y,
			Container.bSlotBased ? 1 : 0,
			State.CurrentWeight, Container.MaxWeight, State.CurrentVolume, Container.MaxVolume);
	}

	// Setup callbacks
	FInventoryAddHelper::FAddCallbacks Callbacks;
	Callbacks.GetEffectivePlacement = [this](const FInventoryEntry& E, FGameplayTag& C, FIntPoint& P, bool& R) {
		return GetEffectiveEntryPlacement(E, C, P, R);
	};
	Callbacks.FindFreeGridPos = [this](const FInventoryContainerConfig& C, FIntPoint S, int32 I, FIntPoint& P) {
		return FindFreeGridPos(C, S, I, P);
	};
	Callbacks.ContainerAllowsItem = [this](const FInventoryContainerConfig& C, const FItemDataView& D) {
		return ContainerAllowsItem(C, D);
	};
	Callbacks.GetEffectiveMaxStack = [this](const FInventoryContainerConfig& C, const FItemDataView& D) {
		return GetEffectiveMaxStackForContainer(C, D);
	};

	const bool bIsStackable = FMath::Max(1, ItemData.MaxStack) > 1;
	int32 RemainingQuantity = AllowedQuantity;

	// Phase 1: Stack with existing entries
	if (bIsStackable)
	{
		TArray<FInventoryAddHelper::FStackTarget> StackTargets;
		RemainingQuantity = FInventoryAddHelper::CalculateStackTargets(
			ObjectId, ItemData, AllowedQuantity, Inventory.Entries, ContainerStates, Callbacks, StackTargets);

		// Apply stack targets to state
		for (const FInventoryAddHelper::FStackTarget& Target : StackTargets)
		{
			if (FInventoryEntry* Entry = Inventory.FindEntry(Target.InstanceId))
			{
				Entry->Quantity += Target.Quantity;
				Inventory.MarkEntryDirty(*Entry);
				UE_LOG(LogProjectInventory, Log, TEXT("Stacked %d x %s (Total: %d)"),
					Target.Quantity, *ItemData.DisplayName.ToString(), Entry->Quantity);
			}
		}
	}

	// Phase 2: Create new stacks one at a time
	// Add entries immediately so FindFreeGridPos sees them for next iteration
	while (RemainingQuantity > 0)
	{
		TArray<FInventoryAddHelper::FNewStackPlacement> Placements;
		const int32 BeforeRemaining = RemainingQuantity;
		RemainingQuantity = FInventoryAddHelper::CalculateNewPlacements(
			ItemData, RemainingQuantity, ContainerStates, Callbacks, Placements);

		if (Placements.Num() == 0)
		{
			break; // No more placements possible
		}

		// Add first placement immediately so next iteration sees it
		const FInventoryAddHelper::FNewStackPlacement& Placement = Placements[0];
		const int32 SlotIndex = ComputeSlotIndex(Placement.ContainerId, Placement.GridPos);
		const uint32 InstanceId = Inventory.AddEntry(
			ObjectId, Placement.Quantity, Placement.ContainerId, Placement.GridPos, Placement.bRotated, SlotIndex);
		UE_LOG(LogProjectInventory, Log, TEXT("Placed %d x %s -> %s at (%d,%d) rot:%d (InstanceId: %d)"),
			Placement.Quantity, *ItemData.DisplayName.ToString(), *Placement.ContainerId.ToString(),
			Placement.GridPos.X, Placement.GridPos.Y, Placement.bRotated ? 1 : 0, InstanceId);

		// Only process one placement per iteration to avoid overlaps
		RemainingQuantity = BeforeRemaining - Placement.Quantity;
	}

	const int32 QuantityAdded = AllowedQuantity - RemainingQuantity;
	if (RemainingQuantity > 0)
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("Internal_AddItem: Could not add %d remaining items"), RemainingQuantity);
	}

	Outcome.AddedQuantity = QuantityAdded;
	if (QuantityAdded == 0)
	{
		// Placement returned no acceptance despite non-zero AllowedQuantity (e.g., no
		// compatible container). Hard failure with toast so pickup is never silent.
		UE_LOG(LogProjectInventory, Warning, TEXT("Internal_AddItem: Zero quantity accepted for %s"),
			*ObjectId.ToString());
		Outcome.Fail = EInventoryAddFailReason::QuantityRejected;
		BroadcastError(NSLOCTEXT("Inventory", "QuantityRejected", "Cannot place that item"));
	}
	return Outcome;
}

bool UProjectInventoryComponent::Internal_RemoveItem(uint32 InstanceId, int32 Quantity)
{
	FInventoryEntry* Entry = Inventory.FindEntry(InstanceId);
	if (!Entry)
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("Internal_RemoveItem: InstanceId %d not found"), InstanceId);
		return false;
	}

	Entry->Quantity -= Quantity;
	if (Entry->Quantity <= 0)
	{
		Inventory.RemoveEntry(InstanceId);
	}
	else
	{
		Inventory.MarkEntryDirty(*Entry);
	}

	return true;
}

bool UProjectInventoryComponent::Internal_MoveItem(uint32 InstanceId, FGameplayTag FromContainer, FIntPoint FromPos, FGameplayTag ToContainer, FIntPoint ToPos, int32 Quantity, bool bRotated)
{
	FInventoryEntry* Entry = Inventory.FindEntry(InstanceId);
	if (!Entry)
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("Internal_MoveItem: InstanceId %d not found"), InstanceId);
		BroadcastError(MakeMoveRejectText(EInventoryMoveRejectReason::InvalidRequest));
		return false;
	}

	if (Quantity <= 0 || Quantity > Entry->Quantity)
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("Internal_MoveItem: Invalid quantity %d for InstanceId %d"), Quantity, InstanceId);
		BroadcastError(MakeMoveRejectText(EInventoryMoveRejectReason::InvalidRequest));
		return false;
	}

	FGameplayTag CurrentContainerId;
	FIntPoint CurrentPos;
	bool bCurrentRotated = false;
	if (!GetEffectiveEntryPlacement(*Entry, CurrentContainerId, CurrentPos, bCurrentRotated))
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("Internal_MoveItem: Invalid current placement for InstanceId %d"), InstanceId);
		BroadcastError(MakeMoveRejectText(EInventoryMoveRejectReason::InvalidRequest));
		return false;
	}

	if (FromContainer.IsValid() && FromContainer != CurrentContainerId)
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("Internal_MoveItem: FromContainer mismatch for InstanceId %d"), InstanceId);
		BroadcastError(MakeMoveRejectText(EInventoryMoveRejectReason::InvalidRequest));
		return false;
	}

	if (FromPos.X >= 0 && FromPos.Y >= 0 && FromPos != CurrentPos)
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("Internal_MoveItem: FromPos mismatch for InstanceId %d"), InstanceId);
		BroadcastError(MakeMoveRejectText(EInventoryMoveRejectReason::InvalidRequest));
		return false;
	}

	FItemDataView ItemData;
	const EInventoryItemDataResolveState ResolveState = ResolveItemDataView(Entry->ItemId, ItemData);
	if (ResolveState != EInventoryItemDataResolveState::Loaded)
	{
		RejectMove(
			TEXT("Internal_MoveItem"),
			ResolveState == EInventoryItemDataResolveState::Loading
				? EInventoryMoveRejectReason::ItemDataLoading
				: EInventoryMoveRejectReason::ItemDataMissing,
			InstanceId);
		return false;
	}

	FInventoryContainerConfig TargetContainer;
	if (!GetContainerConfig(ToContainer, TargetContainer))
	{
		RejectMove(TEXT("Internal_MoveItem"), EInventoryMoveRejectReason::TargetContainerMissing, InstanceId);
		return false;
	}

	if (!ContainerAllowsItem(TargetContainer, ItemData))
	{
		RejectMove(TEXT("Internal_MoveItem"), EInventoryMoveRejectReason::ItemRejectedByContainer, InstanceId);
		return false;
	}

	const int32 EffectiveMaxStack = GetEffectiveMaxStackForContainer(TargetContainer, ItemData);
	if (Quantity > EffectiveMaxStack)
	{
		RejectMove(TEXT("Internal_MoveItem"), EInventoryMoveRejectReason::QuantityExceedsTargetStack, InstanceId);
		return false;
	}

	const bool bTargetRotated = bRotated && TargetContainer.bAllowRotation;
	const FIntPoint ItemSize = GetItemGridSize(ItemData, bTargetRotated);
	const FIntPoint CurrentItemSize = GetItemGridSize(ItemData, bCurrentRotated);

	if (!IsRectWithinContainer(TargetContainer, ToPos, ItemSize))
	{
		RejectMove(TEXT("Internal_MoveItem"), EInventoryMoveRejectReason::OutOfBounds, InstanceId);
		return false;
	}

	// SOLID: Delegated to FInventoryMoveHelper
	if (Quantity < Entry->Quantity && CurrentContainerId == ToContainer)
	{
		if (FInventoryMoveHelper::CheckSelfOverlap(CurrentPos, CurrentItemSize, ToPos, ItemSize))
		{
			RejectMove(TEXT("Internal_MoveItem"), EInventoryMoveRejectReason::SplitSourceOverlap, InstanceId);
			return false;
		}
	}

	// No-op move
	if (CurrentContainerId == ToContainer && CurrentPos == ToPos && bCurrentRotated == bTargetRotated && Quantity == Entry->Quantity)
	{
		return true;
	}

	TMap<FPrimaryAssetId, FItemDataView> ItemDataCache;
	ItemDataCache.Add(Entry->ItemId, ItemData);

	// Capacity check when moving across containers
	if (CurrentContainerId != ToContainer)
	{
		if (TargetContainer.MaxWeight > 0.f && ItemData.Weight > 0.f)
		{
			const float TargetWeight = GetContainerCurrentWeight(ToContainer, ItemDataCache);
			if (TargetWeight + (ItemData.Weight * Quantity) > TargetContainer.MaxWeight)
			{
				RejectMove(TEXT("Internal_MoveItem"), EInventoryMoveRejectReason::TargetWeightExceeded, InstanceId);
				return false;
			}
		}

		if (TargetContainer.MaxVolume > 0.f && ItemData.Volume > 0.f)
		{
			const float TargetVolume = GetContainerCurrentVolume(ToContainer, ItemDataCache);
			if (TargetVolume + (ItemData.Volume * Quantity) > TargetContainer.MaxVolume)
			{
				RejectMove(TEXT("Internal_MoveItem"), EInventoryMoveRejectReason::TargetVolumeExceeded, InstanceId);
				return false;
			}
		}
	}

	// SOLID: Overlap detection delegated to FInventoryMoveHelper
	FInventoryMoveHelper::FMoveCallbacks MoveCallbacks;
	MoveCallbacks.GetEffectivePlacement = [this](const FInventoryEntry& E, FGameplayTag& C, FIntPoint& P, bool& R) {
		return GetEffectiveEntryPlacement(E, C, P, R);
	};
	MoveCallbacks.GetItemDataView = [this](FPrimaryAssetId Id, FItemDataView& D) {
		return GetItemDataView(Id, D);
	};
	MoveCallbacks.GetItemGridSize = [this](const FItemDataView& D, bool R) {
		return GetItemGridSize(D, R);
	};

	const FInventoryMoveHelper::FOverlapResult OverlapResult = FInventoryMoveHelper::FindOverlapAtTarget(
		Inventory.Entries, ToContainer, ToPos, ItemSize, InstanceId, MoveCallbacks);

	if (OverlapResult.bMultipleOverlaps)
	{
		RejectMove(TEXT("Internal_MoveItem"), EInventoryMoveRejectReason::MultipleTargetOverlaps, InstanceId);
		return false;
	}

	FInventoryEntry* OverlapEntry = OverlapResult.bHasOverlap ? Inventory.FindEntry(OverlapResult.OverlapInstanceId) : nullptr;

	// SOLID: Stack validation delegated to FInventoryMoveHelper
	if (OverlapEntry)
	{
		if (!FInventoryMoveHelper::CanStackWith(*Entry, *OverlapEntry, EffectiveMaxStack, Quantity))
		{
			RejectMove(TEXT("Internal_MoveItem"), EInventoryMoveRejectReason::StackRejected, InstanceId);
			return false;
		}

		OverlapEntry->Quantity += Quantity;
		Inventory.MarkEntryDirty(*OverlapEntry);

		if (Quantity >= Entry->Quantity)
		{
			Inventory.RemoveEntry(InstanceId);
		}
		else
		{
			Entry->Quantity -= Quantity;
			Inventory.MarkEntryDirty(*Entry);
		}

		return true;
	}

	if (Quantity < Entry->Quantity)
	{
		// Split stack into new entry
		const int32 SlotIndex = ComputeSlotIndex(ToContainer, ToPos);
		const uint32 NewInstanceId = Inventory.AddEntry(Entry->ItemId, Quantity, ToContainer, ToPos, bTargetRotated, SlotIndex);
		FInventoryEntry* NewEntry = Inventory.FindEntry(NewInstanceId);
		if (NewEntry)
		{
			NewEntry->InstanceData = Entry->InstanceData;
			NewEntry->OverrideMagnitudes = Entry->OverrideMagnitudes;
			Inventory.MarkEntryDirty(*NewEntry);
		}

		Entry->Quantity -= Quantity;
		Inventory.MarkEntryDirty(*Entry);
		return true;
	}

	// Move entire entry
	Entry->ContainerId = ToContainer;
	Entry->GridPos = ToPos;
	Entry->bRotated = bTargetRotated;
	Entry->SlotIndex = ComputeSlotIndex(ToContainer, ToPos);
	Inventory.MarkEntryDirty(*Entry);
	return true;
}


// -------------------------------------------------------------------------
// Exact-position add helper (used by world-container take flow and
// anywhere else that needs to place a known quantity at a known cell).
// -------------------------------------------------------------------------

bool UProjectInventoryComponent::TryAddItemAtPosition(
	FPrimaryAssetId ObjectId,
	int32 Quantity,
	FGameplayTag TargetContainerId,
	FIntPoint TargetGridPos,
	bool bTargetRotated,
	FText& OutError)
{
	OutError = FText::GetEmpty();

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		OutError = NSLOCTEXT("ProjectInventory", "TryAddAtPositionAuthorityRequired", "Exact inventory placement requires authority.");
		return false;
	}

	if (!ObjectId.IsValid() || Quantity <= 0)
	{
		OutError = NSLOCTEXT("ProjectInventory", "TryAddAtPositionInvalidPayload", "Target item payload is invalid.");
		return false;
	}

	if (!TargetContainerId.IsValid() || TargetGridPos.X < 0 || TargetGridPos.Y < 0)
	{
		OutError = NSLOCTEXT("ProjectInventory", "TryAddAtPositionInvalidTarget", "Target inventory placement is invalid.");
		return false;
	}

	FItemDataView ItemData;
	const EInventoryItemDataResolveState ResolveState = ResolveItemDataView(ObjectId, ItemData);
	if (ResolveState != EInventoryItemDataResolveState::Loaded)
	{
		OutError = NSLOCTEXT("ProjectInventory", "TryAddAtPositionMissingItemData", "Target item data is missing.");
		return false;
	}

	FInventoryContainerConfig TargetContainer;
	if (!GetContainerConfig(TargetContainerId, TargetContainer))
	{
		OutError = NSLOCTEXT("ProjectInventory", "TryAddAtPositionMissingContainer", "Target inventory container is unavailable.");
		return false;
	}

	if (!ContainerAllowsItem(TargetContainer, ItemData))
	{
		OutError = NSLOCTEXT("ProjectInventory", "TryAddAtPositionRejectedByContainer", "Target inventory container does not accept this item.");
		return false;
	}

	const int32 EffectiveMaxStack = GetEffectiveMaxStackForContainer(TargetContainer, ItemData);
	if (Quantity > EffectiveMaxStack)
	{
		OutError = NSLOCTEXT("ProjectInventory", "TryAddAtPositionDepthRejected", "Target inventory cell cannot hold that many items.");
		return false;
	}

	const int32 AllowedQuantity = FInventoryStackHelper::CalculateAllowedQuantity(
		ItemData,
		GetMaxWeight(),
		GetMaxVolume(),
		GetCurrentWeight(),
		GetCurrentVolume(),
		Quantity);
	if (AllowedQuantity < Quantity)
	{
		OutError = NSLOCTEXT("ProjectInventory", "TryAddAtPositionCapacityExceeded", "Inventory does not have enough carrying capacity.");
		return false;
	}

	TMap<FPrimaryAssetId, FItemDataView> ItemDataCache;
	ItemDataCache.Add(ObjectId, ItemData);
	if (TargetContainer.MaxWeight > 0.f)
	{
		const float TargetWeight = GetContainerCurrentWeight(TargetContainerId, ItemDataCache);
		if (TargetWeight + (ItemData.Weight * Quantity) > TargetContainer.MaxWeight)
		{
			OutError = NSLOCTEXT("ProjectInventory", "TryAddAtPositionWeightExceeded", "Target inventory container would exceed its weight limit.");
			return false;
		}
	}

	if (TargetContainer.MaxVolume > 0.f)
	{
		const float TargetVolume = GetContainerCurrentVolume(TargetContainerId, ItemDataCache);
		if (TargetVolume + (ItemData.Volume * Quantity) > TargetContainer.MaxVolume)
		{
			OutError = NSLOCTEXT("ProjectInventory", "TryAddAtPositionVolumeExceeded", "Target inventory container would exceed its volume limit.");
			return false;
		}
	}

	const bool bEffectiveRotated = bTargetRotated && TargetContainer.bAllowRotation;
	const FIntPoint ItemSize = GetItemGridSize(ItemData, bEffectiveRotated);
	if (!IsRectWithinContainer(TargetContainer, TargetGridPos, ItemSize))
	{
		OutError = NSLOCTEXT("ProjectInventory", "TryAddAtPositionOutOfBounds", "Target inventory placement is outside the container grid.");
		return false;
	}

	FInventoryMoveHelper::FMoveCallbacks MoveCallbacks;
	MoveCallbacks.GetEffectivePlacement = [this](const FInventoryEntry& E, FGameplayTag& C, FIntPoint& P, bool& R) {
		return GetEffectiveEntryPlacement(E, C, P, R);
	};
	MoveCallbacks.GetItemDataView = [this](FPrimaryAssetId Id, FItemDataView& D) {
		return GetItemDataView(Id, D);
	};
	MoveCallbacks.GetItemGridSize = [this](const FItemDataView& D, bool R) {
		return GetItemGridSize(D, R);
	};

	const FInventoryMoveHelper::FOverlapResult OverlapResult = FInventoryMoveHelper::FindOverlapAtTarget(
		Inventory.Entries,
		TargetContainerId,
		TargetGridPos,
		ItemSize,
		INDEX_NONE,
		MoveCallbacks);
	if (OverlapResult.bMultipleOverlaps)
	{
		OutError = NSLOCTEXT("ProjectInventory", "TryAddAtPositionMultipleOverlaps", "Target inventory placement overlaps multiple entries.");
		return false;
	}

	if (OverlapResult.bHasOverlap)
	{
		FInventoryEntry* OverlapEntry = Inventory.FindEntry(OverlapResult.OverlapInstanceId);
		if (!OverlapEntry
			|| OverlapEntry->ItemId != ObjectId
			|| OverlapEntry->OverrideMagnitudes.Num() > 0
			|| EffectiveMaxStack <= 1
			|| OverlapEntry->Quantity + Quantity > EffectiveMaxStack)
		{
			OutError = NSLOCTEXT("ProjectInventory", "TryAddAtPositionOverlapRejected", "Target inventory entry cannot accept this stack.");
			return false;
		}

		OverlapEntry->Quantity += Quantity;
		Inventory.MarkEntryDirty(*OverlapEntry);
		return true;
	}

	const int32 SlotIndex = ComputeSlotIndex(TargetContainerId, TargetGridPos);
	Inventory.AddEntry(ObjectId, Quantity, TargetContainerId, TargetGridPos, bEffectiveRotated, SlotIndex);
	return true;
}
