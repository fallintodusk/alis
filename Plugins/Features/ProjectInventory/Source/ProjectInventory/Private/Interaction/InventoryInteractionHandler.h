// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

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
 */
class FInventoryInteractionHandler : public TSharedFromThis<FInventoryInteractionHandler>
{
public:
	FInventoryInteractionHandler() = default;
	~FInventoryInteractionHandler();

	void Subscribe();
	void Unsubscribe();
	bool IsSubscribed() const { return InteractionHandle.IsValid(); }

private:
	void HandleInteraction(AActor* Target, AActor* Instigator);
	void HandlePickupSource(UObject* PickupSource, APawn* Pawn);
	UProjectInventoryComponent* GetInventory(APawn* Pawn) const;
	UObject* FindPickupSource(AActor* Target) const;

	FDelegateHandle InteractionHandle;
};
