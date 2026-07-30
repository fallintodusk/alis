// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"
#include "UObject/WeakObjectPtr.h"

class APawn;
class UObjectDefinitionCache;
class UProjectInventoryComponent;

/**
 * Handles inventory-related interactions (pickup items).
 * Subscribes to IInteractionService::OnInteraction() and routes to inventory.
 *
 * Pattern:
 * - Pickup items: Resolve IPickupSource, read ObjectDefinitionId + Quantity from source
 *
 * NOTE: Doors and other interactables handle themselves via IInteractableTargetInterface.
 * This handler only processes inventory-specific objects.
 *
 * All operations are server-only (interaction is already server-authoritative).
 *
 * Deferred-pickup bookkeeping
 * ---------------------------
 * When the ObjectDefinition for a pickup source is not yet resident, the
 * inventory component returns a deferred outcome (see FInventoryAddOutcome).
 * The handler queues an intent keyed on the pickup source (weak) and the
 * ObjectId, kicks ObjectDefinitionCache::RequestLoad exactly once per unique
 * id, and re-enters the add path when the cache callback fires. Consume()
 * is driven only by the authoritative success branch (AddedQuantity > 0).
 *
 * Lifetime contract:
 * - Intents hold TWeakObjectPtr to both pickup source and instigating pawn.
 * - Callback entry with either weak-ptr null => silent drop (player moved,
 *   actor destroyed, race lost, world teardown).
 * - Duplicate submissions before callback merge into one intent (first wins).
 * - nullptr callback payload is terminal; pickup source is NOT consumed.
 *
 * No public async-pickup API is exposed; this bookkeeping is private to the
 * interaction handler.
 */
class PROJECTINVENTORY_API FInventoryInteractionHandler : public TSharedFromThis<FInventoryInteractionHandler>
{
public:
	FInventoryInteractionHandler() = default;
	~FInventoryInteractionHandler();

	void Subscribe();
	void Unsubscribe();
	bool IsSubscribed() const { return InteractionHandle.IsValid(); }

	/**
	 * Test-only: inspect the number of pending pickup intents.
	 * Lets integration tests assert merge-on-duplicate and teardown behavior
	 * without exposing intent internals.
	 */
	int32 GetPendingPickupIntentCount() const { return PendingPickupIntents.Num(); }

	/**
	 * Directly submit a pickup intent. Used by HandleInteraction and by tests
	 * that bypass the IInteractionService broadcast path. Behavior matches
	 * HandlePickupSource verbatim.
	 */
	void SubmitPickupIntent(UObject* PickupSource, APawn* Pawn);

private:
	struct FPendingPickupIntent
	{
		TWeakObjectPtr<UObject> PickupSource;
		TWeakObjectPtr<APawn> Pawn;
		TWeakObjectPtr<UProjectInventoryComponent> Inventory;
		FPrimaryAssetId ObjectId;
		int32 Quantity = 0;
	};

	struct FPendingPickupKey
	{
		FWeakObjectPtr PickupSourceKey;
		FPrimaryAssetId ObjectId;

		bool operator==(const FPendingPickupKey& Other) const
		{
			return PickupSourceKey == Other.PickupSourceKey && ObjectId == Other.ObjectId;
		}

		friend uint32 GetTypeHash(const FPendingPickupKey& Key)
		{
			return HashCombine(GetTypeHash(Key.PickupSourceKey), GetTypeHash(Key.ObjectId));
		}
	};

	void HandleInteraction(AActor* Target, AActor* Instigator);
	void HandlePickupSource(UObject* PickupSource, APawn* Pawn);
	void HandleWorldContainerSource(AActor* Target, UObject* WorldContainerSource, APawn* Pawn);
	UProjectInventoryComponent* GetInventory(APawn* Pawn) const;
	UObject* FindPickupSource(AActor* Target) const;
	UObject* FindWorldContainerSource(AActor* Target) const;

	/**
	 * Invoke the authoritative add path for a resolved intent. If the outcome
	 * is still deferred (e.g., cache transition not yet propagated), leave the
	 * intent in place; otherwise clear it.
	 */
	void CompletePickupIntent(const FPendingPickupKey& Key, UObject* LoadedObject);

	/**
	 * Drop entries whose pickup source or pawn has been garbage-collected.
	 * Cheap bookkeeping sweep; invoked on submission and after callback.
	 */
	void PruneExpiredIntents();

	FDelegateHandle InteractionHandle;

	TMap<FPendingPickupKey, FPendingPickupIntent> PendingPickupIntents;
};
