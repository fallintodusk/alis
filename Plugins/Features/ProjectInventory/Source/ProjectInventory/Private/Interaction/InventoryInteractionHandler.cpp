// Copyright ALIS. All Rights Reserved.

#include "Interaction/InventoryInteractionHandler.h"
#include "ProjectInventory.h"
#include "Interfaces/IInteractionService.h"
#include "Interfaces/ILootSource.h"
#include "Interfaces/IPickupSource.h"
#include "ProjectServiceLocator.h"
#include "Components/ProjectInventoryComponent.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Pawn.h"

FInventoryInteractionHandler::~FInventoryInteractionHandler()
{
	Unsubscribe();
}

void FInventoryInteractionHandler::Subscribe()
{
	TSharedPtr<IInteractionService> Service = FProjectServiceLocator::Resolve<IInteractionService>();
	if (!Service.IsValid())
	{
		return;
	}

	InteractionHandle = Service->OnInteraction().AddSP(AsShared(), &FInventoryInteractionHandler::HandleInteraction);
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

	// Loot containers
	if (UObject* LootSource = FindLootSource(Target))
	{
		HandleLootSource(LootSource, Pawn);
		return;
	}

	// NOTE: Doors and other interactables handle themselves via IInteractableTargetInterface
	// No door-specific code here
}

void FInventoryInteractionHandler::HandlePickupSource(UObject* PickupSource, APawn* Pawn)
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

	// Try to add item using ObjectDefinitionId
	int32 Added = Inventory->TryAddItem(ObjectDefId, AvailableQuantity);
	if (Added > 0)
	{
		IPickupSource::Execute_Consume(PickupSource, Added);
	}
}

void FInventoryInteractionHandler::HandleLootSource(UObject* LootSource, APawn* Pawn)
{
	if (!LootSource)
	{
		return;
	}

	if (ILootSource::Execute_IsLootEmpty(LootSource))
	{
		return;
	}

	TArray<FLootEntryView> LootEntries = ILootSource::Execute_GetLootEntries(LootSource);
	if (LootEntries.Num() == 0)
	{
		return;
	}

	TArray<FLootEntry> Items;
	Items.Reserve(LootEntries.Num());

	int32 InvalidCount = 0;
	for (const FLootEntryView& EntryView : LootEntries)
	{
		if (!EntryView.IsValid())
		{
			++InvalidCount;
			continue;
		}

		Items.Add(FLootEntry(EntryView.ObjectId, EntryView.Quantity));
	}

	if (InvalidCount > 0)
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("HandleLootSource: Skipped %d invalid loot entries in %s"),
			InvalidCount, *GetNameSafe(LootSource));
	}

	UProjectInventoryComponent* Inventory = GetInventory(Pawn);
	if (!Inventory)
	{
		return;
	}

	if (Items.Num() == 0)
	{
		return;
	}

	if (!Inventory->CanFitItems(Items))
	{
		return;
	}

	Inventory->AddItemsBatch(Items);
	ILootSource::Execute_ConsumeLootSource(LootSource);
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

UObject* FInventoryInteractionHandler::FindLootSource(AActor* Target) const
{
	if (!Target)
	{
		return nullptr;
	}

	if (Target->Implements<ULootSource>())
	{
		return Target;
	}

	TInlineComponentArray<UActorComponent*> Components;
	Target->GetComponents(Components);

	for (UActorComponent* Component : Components)
	{
		if (Component && Component->Implements<ULootSource>())
		{
			return Component;
		}
	}

	return nullptr;
}
