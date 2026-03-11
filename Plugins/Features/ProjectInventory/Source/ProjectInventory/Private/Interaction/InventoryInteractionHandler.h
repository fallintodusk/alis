// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UProjectInventoryComponent;

/**
 * Handles inventory-related interactions (pickup items, loot containers).
 * Subscribes to IInteractionService::OnInteraction() and routes to inventory.
 *
 * Pattern:
 * - Pickup items: Resolve IPickupSource, read ObjectDefinitionId + Quantity from source
 * - Loot containers: Resolve ILootSource, call CanFitItems/AddItemsBatch
 *
 * NOTE: Doors and other interactables handle themselves via IInteractableTargetInterface.
 * This handler only processes inventory-specific objects.
 *
 * All operations are server-only (interaction is already server-authoritative).
 */
class FInventoryInteractionHandler : public TSharedFromThis<FInventoryInteractionHandler>
{
public:
	FInventoryInteractionHandler() = default;
	~FInventoryInteractionHandler();

	void Subscribe();
	void Unsubscribe();

private:
	void HandleInteraction(AActor* Target, AActor* Instigator);
	void HandlePickupSource(UObject* PickupSource, APawn* Pawn);
	void HandleLootSource(UObject* LootSource, APawn* Pawn);
	UProjectInventoryComponent* GetInventory(APawn* Pawn) const;
	UObject* FindPickupSource(AActor* Target) const;
	UObject* FindLootSource(AActor* Target) const;

	FDelegateHandle InteractionHandle;
};
