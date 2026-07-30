// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Interaction/InventoryInteractionHandler.h"
#include "ProjectInventory.h"
#include "Interfaces/IInteractionService.h"
#include "Interfaces/IInventoryWorldContainerTransferBridge.h"
#include "Interfaces/IPickupSource.h"
#include "Interfaces/IWorldContainerSessionSource.h"
#include "ProjectServiceLocator.h"
#include "Components/ProjectInventoryComponent.h"
#include "Components/ActorComponent.h"
#include "Services/ObjectDefinitionCache.h"
#include "GameFramework/Pawn.h"

FInventoryInteractionHandler::~FInventoryInteractionHandler()
{
	Unsubscribe();
}

void FInventoryInteractionHandler::Subscribe()
{
	if (InteractionHandle.IsValid())
	{
		return;
	}

	TSharedPtr<IInteractionService> Service = FProjectServiceLocator::Resolve<IInteractionService>();
	if (!Service.IsValid())
	{
		UE_LOG(LogProjectInventory, Verbose, TEXT("[InventoryInteractionHandler::Subscribe] IInteractionService not yet available"));
		return;
	}

	InteractionHandle = Service->OnInteraction().AddSP(AsShared(), &FInventoryInteractionHandler::HandleInteraction);
	UE_LOG(LogProjectInventory, Log, TEXT("Inventory interaction handler subscribed"));
}

void FInventoryInteractionHandler::Unsubscribe()
{
	if (!InteractionHandle.IsValid())
	{
		return;
	}

	TSharedPtr<IInteractionService> Service = FProjectServiceLocator::Resolve<IInteractionService>();
	if (Service.IsValid())
	{
		Service->OnInteraction().Remove(InteractionHandle);
	}

	InteractionHandle.Reset();
}

void FInventoryInteractionHandler::HandleInteraction(AActor* Target, AActor* Instigator)
{
	if (!Target || !Instigator)
	{
		return;
	}

	if (!Instigator->HasAuthority())
	{
		return;
	}

	APawn* Pawn = Cast<APawn>(Instigator);
	if (!Pawn)
	{
		return;
	}

	// Inventory-specific interactions only

	// Pickup items (via IPickupSource)
	if (UObject* PickupSource = FindPickupSource(Target))
	{
		HandlePickupSource(PickupSource, Pawn);
		return;
	}

	if (UObject* WorldContainerSource = FindWorldContainerSource(Target))
	{
		HandleWorldContainerSource(Target, WorldContainerSource, Pawn);
		return;
	}

	// NOTE: Doors and other interactables handle themselves via IInteractableTargetInterface
	// No door-specific code here
}

void FInventoryInteractionHandler::HandlePickupSource(UObject* PickupSource, APawn* Pawn)
{
	SubmitPickupIntent(PickupSource, Pawn);
}

void FInventoryInteractionHandler::SubmitPickupIntent(UObject* PickupSource, APawn* Pawn)
{
	if (!PickupSource)
	{
		return;
	}

	const FPrimaryAssetId ObjectDefId = IPickupSource::Execute_GetObjectDefinitionId(PickupSource);
	if (!ObjectDefId.IsValid())
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("HandlePickupSource: No valid ObjectDefinitionId"));
		return;
	}

	UProjectInventoryComponent* Inventory = GetInventory(Pawn);
	if (!Inventory)
	{
		return;
	}

	const int32 AvailableQuantity = IPickupSource::Execute_GetQuantity(PickupSource);
	if (AvailableQuantity <= 0)
	{
		return;
	}

	PruneExpiredIntents();

	// Fast path: try the authoritative add immediately. If the cache already
	// has the ObjectDefinition resident, the outcome returns AddedQuantity > 0
	// and we Consume synchronously — the common case once warmup has occurred.
	FInventoryAddOutcome Outcome = Inventory->TryAddItemDetailed(ObjectDefId, AvailableQuantity);

	if (Outcome.AddedQuantity > 0)
	{
		IPickupSource::Execute_Consume(PickupSource, Outcome.AddedQuantity);
		return;
	}

	if (Outcome.Fail != EInventoryAddFailReason::None)
	{
		// Terminal fail: toast already broadcast by Internal_AddItem; pickup is
		// NOT consumed and intent is NOT queued.
		UE_LOG(LogProjectInventory, Warning,
			TEXT("HandlePickupSource: terminal fail for %s (reason=%u)"),
			*ObjectDefId.ToString(), static_cast<uint32>(Outcome.Fail));
		return;
	}

	if (!Outcome.bDeferred)
	{
		// Safety: outcome shape should always be success/deferred/fail. If
		// not, treat as silent no-op but log loud in dev.
		UE_LOG(LogProjectInventory, Warning,
			TEXT("HandlePickupSource: indeterminate outcome for %s; no intent queued"),
			*ObjectDefId.ToString());
		return;
	}

	// Deferred: ObjectDefinition is still streaming. Queue the intent and kick
	// RequestLoad exactly once per unique id (RequestLoad itself deduplicates
	// so multiple intents for the same id share a single streamable handle).
	const FPendingPickupKey Key{ FWeakObjectPtr(PickupSource), ObjectDefId };

	if (FPendingPickupIntent* Existing = PendingPickupIntents.Find(Key))
	{
		// Anti-spam: duplicate submissions for the same (PickupSource, ObjectId)
		// merge into the first intent. Keep the first queued quantity; flag if a
		// later submission presents a different quantity (design drift).
		if (Existing->Quantity != AvailableQuantity)
		{
			UE_LOG(LogProjectInventory, Warning,
				TEXT("HandlePickupSource: quantity drift on merge for %s (%d vs. %d); keeping first"),
				*ObjectDefId.ToString(), Existing->Quantity, AvailableQuantity);
			ensureMsgf(false,
				TEXT("Pickup source quantity changed between submissions (%d -> %d). "
				     "Pickups should not mutate quantity while a load is in flight."),
				Existing->Quantity, AvailableQuantity);
		}
		return;
	}

	FPendingPickupIntent Intent;
	Intent.PickupSource = PickupSource;
	Intent.Pawn = Pawn;
	Intent.Inventory = Inventory;
	Intent.ObjectId = ObjectDefId;
	Intent.Quantity = AvailableQuantity;
	PendingPickupIntents.Add(Key, Intent);

	UObjectDefinitionCache* Cache = Inventory->GetObjectDefinitionCache();
	if (!Cache)
	{
		// Cache missing AND outcome says deferred: this should not happen —
		// Internal_AddItem would have reported CacheUnavailable. Clear intent
		// and bail fail-closed.
		UE_LOG(LogProjectInventory, Error,
			TEXT("SubmitPickupIntent: deferred outcome but inventory has no cache; dropping intent for %s"),
			*ObjectDefId.ToString());
		PendingPickupIntents.Remove(Key);
		return;
	}

	TWeakPtr<FInventoryInteractionHandler> WeakSelf = AsShared();
	Cache->RequestLoad(ObjectDefId,
		FOnObjectDefinitionLoaded::CreateLambda(
			[WeakSelf, Key](UObject* LoadedObject)
			{
				TSharedPtr<FInventoryInteractionHandler> Self = WeakSelf.Pin();
				if (!Self.IsValid())
				{
					// Handler was shut down (module unload). Silent drop — teardown
					// is not an error.
					return;
				}
				Self->CompletePickupIntent(Key, LoadedObject);
			}));
}

void FInventoryInteractionHandler::CompletePickupIntent(
	const FPendingPickupKey& Key, UObject* LoadedObject)
{
	FPendingPickupIntent Intent;
	if (!PendingPickupIntents.RemoveAndCopyValue(Key, Intent))
	{
		// Already resolved (duplicate callback or manual prune). Nothing to do.
		return;
	}

	UObject* PickupSource = Intent.PickupSource.Get();
	APawn* Pawn = Intent.Pawn.Get();
	UProjectInventoryComponent* Inventory = Intent.Inventory.Get();

	if (!PickupSource || !Pawn || !Inventory)
	{
		// Expected drop: pickup actor destroyed, pawn despawned, inventory
		// component gone, or world tore down during load. Not an error.
		UE_LOG(LogProjectInventory, Verbose,
			TEXT("CompletePickupIntent: weak-ptr null for %s; silent drop"),
			*Intent.ObjectId.ToString());
		return;
	}

	if (!LoadedObject)
	{
		// Terminal hard failure from the cache. Toast + no consume.
		UE_LOG(LogProjectInventory, Warning,
			TEXT("CompletePickupIntent: cache returned null for %s; terminal fail"),
			*Intent.ObjectId.ToString());
		Inventory->OnInventoryErrorNative().Broadcast(
			NSLOCTEXT("Inventory", "PickupLoadFailed", "Could not load that item"));
		return;
	}

	// Re-enter the authoritative add path. By now ObjectDefinitionCache
	// reports Loaded, so Internal_AddItem will either succeed or hit a terminal
	// gameplay reason (no capacity, invalid provider, etc.).
	const FInventoryAddOutcome Outcome =
		Inventory->TryAddItemDetailed(Intent.ObjectId, Intent.Quantity);

	if (Outcome.AddedQuantity > 0)
	{
		IPickupSource::Execute_Consume(PickupSource, Outcome.AddedQuantity);
		return;
	}

	// On deferred-again (race with cache eviction) or fail: drop intent. We
	// never retry beyond the single deferred re-entry to avoid loops.
	if (Outcome.bDeferred)
	{
		UE_LOG(LogProjectInventory, Warning,
			TEXT("CompletePickupIntent: second-pass deferred for %s; not re-queueing"),
			*Intent.ObjectId.ToString());
	}

	PruneExpiredIntents();
}

void FInventoryInteractionHandler::PruneExpiredIntents()
{
	if (PendingPickupIntents.Num() == 0)
	{
		return;
	}

	for (auto It = PendingPickupIntents.CreateIterator(); It; ++It)
	{
		const FPendingPickupIntent& Intent = It.Value();
		if (!Intent.PickupSource.IsValid() || !Intent.Pawn.IsValid() || !Intent.Inventory.IsValid())
		{
			UE_LOG(LogProjectInventory, Verbose,
				TEXT("PruneExpiredIntents: dropping %s"), *Intent.ObjectId.ToString());
			It.RemoveCurrent();
		}
	}
}

void FInventoryInteractionHandler::HandleWorldContainerSource(AActor* Target, UObject* WorldContainerSource, APawn* Pawn)
{
	if (!Target || !WorldContainerSource || !Pawn)
	{
		return;
	}

	UProjectInventoryComponent* Inventory = GetInventory(Pawn);
	if (!Inventory || !Inventory->GetClass()->ImplementsInterface(UInventoryWorldContainerTransferBridge::StaticClass()))
	{
		return;
	}

	FText OpenError;
	if (!IInventoryWorldContainerTransferBridge::Execute_RequestOpenWorldContainerSession(
		Inventory,
		Target,
		EContainerSessionMode::FullOpen,
		OpenError))
	{
		if (!OpenError.IsEmpty())
		{
			UE_LOG(LogProjectInventory, Verbose, TEXT("HandleWorldContainerSource: open rejected - %s"),
				*OpenError.ToString());
		}
	}
}

UProjectInventoryComponent* FInventoryInteractionHandler::GetInventory(APawn* Pawn) const
{
	if (!Pawn)
	{
		return nullptr;
	}
	return Pawn->FindComponentByClass<UProjectInventoryComponent>();
}

UObject* FInventoryInteractionHandler::FindPickupSource(AActor* Target) const
{
	if (!Target)
	{
		return nullptr;
	}

	if (Target->Implements<UPickupSource>())
	{
		return Target;
	}

	TInlineComponentArray<UActorComponent*> Components;
	Target->GetComponents(Components);

	for (UActorComponent* Component : Components)
	{
		if (Component && Component->Implements<UPickupSource>())
		{
			return Component;
		}
	}

	return nullptr;
}

UObject* FInventoryInteractionHandler::FindWorldContainerSource(AActor* Target) const
{
	if (!Target)
	{
		return nullptr;
	}

	if (Target->Implements<UWorldContainerSessionSource>())
	{
		return Target;
	}

	TInlineComponentArray<UActorComponent*> Components;
	Target->GetComponents(Components);

	for (UActorComponent* Component : Components)
	{
		if (Component && Component->Implements<UWorldContainerSessionSource>())
		{
			return Component;
		}
	}

	return nullptr;
}
