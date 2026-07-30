// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
//
// Translation-unit-internal helpers shared by the ProjectInventoryComponent
// split .cpp files. Private - not part of the plugin's public API.
//
// This header exposes the inline helpers that multiple component TUs need
// (local-player-subsystem resolver, pure-logic helpers) without duplicating
// them per TU. Keep it private (Private/Components/ only). Do not include it
// from any other plugin or test target.

#pragma once

#include "CoreMinimal.h"
#include "Components/ProjectInventoryComponent.h"
#include "Inventory/InventoryTypes.h"
#include "Loot/LootTypes.h"
#include "ProjectGameplayTags.h"
#include "Subsystems/ProjectContainerSessionSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

namespace ProjectInventoryInternal
{
	/**
	 * Resolve the client-side session UI cache subsystem for the inventory's
	 * owner. Used by HandleWorldContainerSessionOpenedLocal / ClosedLocal to
	 * keep the local player's nearby-container UI in sync. Returns nullptr
	 * on dedicated servers (no local player); callers must no-op on null.
	 *
	 * Authoritative session state and dispatch live on
	 * UProjectWorldContainerAuthoritySubsystem (a server-gated UWorldSubsystem).
	 * This helper is NOT an authority path.
	 */
	inline UProjectContainerSessionSubsystem* ResolveLocalPlayerSessionSubsystem(const UProjectInventoryComponent* Inventory)
	{
		if (!Inventory)
		{
			return nullptr;
		}

		const APawn* OwnerPawn = Cast<APawn>(Inventory->GetOwner());
		APlayerController* PlayerController = OwnerPawn ? Cast<APlayerController>(OwnerPawn->GetController()) : nullptr;
		if (!PlayerController)
		{
			PlayerController = Cast<APlayerController>(Inventory->GetOwner());
		}

		if (!PlayerController)
		{
			return nullptr;
		}

		ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();
		return LocalPlayer ? LocalPlayer->GetSubsystem<UProjectContainerSessionSubsystem>() : nullptr;
	}

	/** Resolve the actor that owns a world-container session source (actor or component). */
	inline AActor* ResolveWorldContainerActorFromSource(UObject* WorldContainerSource)
	{
		if (AActor* Actor = Cast<AActor>(WorldContainerSource))
		{
			return Actor;
		}

		if (const UActorComponent* Component = Cast<UActorComponent>(WorldContainerSource))
		{
			return Component->GetOwner();
		}

		return nullptr;
	}

	/** Sum quantity across entries matching ObjectId. */
	inline int32 GetTotalItemQuantity(const TArray<FInventoryEntry>& Entries, const FPrimaryAssetId& ObjectId)
	{
		int32 TotalQuantity = 0;
		for (const FInventoryEntry& Entry : Entries)
		{
			if (Entry.ItemId == ObjectId)
			{
				TotalQuantity += Entry.Quantity;
			}
		}
		return TotalQuantity;
	}

	/** Group loot entries by ObjectId + sum quantities, skipping invalid entries. */
	inline void BuildExpectedLootQuantities(const TArray<FLootEntry>& Items, TMap<FPrimaryAssetId, int32>& OutExpectedQuantities)
	{
		OutExpectedQuantities.Reset();
		for (const FLootEntry& Item : Items)
		{
			if (!Item.IsValid())
			{
				continue;
			}

			OutExpectedQuantities.FindOrAdd(Item.ObjectId) += Item.Quantity;
		}
	}

	/**
	 * Hand containers are DESTINATIONS where a held/unequipped item is rendered,
	 * not storage extensions that only exist while an armor item is equipped.
	 * This distinction matters for unequip: the "granted containers must be empty"
	 * rule applies only to storage extensions (pockets/backpack). Hand grants are
	 * destination slots for the unequipped item and their occupancy is handled
	 * separately by the free-hand-cell search.
	 */
	inline bool IsHandDestinationContainer(const FGameplayTag& ContainerId)
	{
		return ContainerId == ProjectTags::Item_Container_LeftHand
			|| ContainerId == ProjectTags::Item_Container_RightHand;
	}
}
