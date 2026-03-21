// Copyright ALIS. All Rights Reserved.

#include "Interaction/InventoryInteractionHandler.h"
#include "ProjectInventory.h"
#include "Interfaces/IInteractionService.h"
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
