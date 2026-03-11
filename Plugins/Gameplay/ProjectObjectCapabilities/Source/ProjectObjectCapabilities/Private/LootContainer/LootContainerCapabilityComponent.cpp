// Copyright ALIS. All Rights Reserved.

#include "LootContainer/LootContainerCapabilityComponent.h"
#include "CapabilityValidationRegistry.h"
#include "Net/UnrealNetwork.h"
#include "ProjectObjectCapabilitiesModule.h"

// Self-registration: component owns its validation registration
#if WITH_EDITOR
static struct FLootContainerValidationAutoReg
{
	FLootContainerValidationAutoReg()
	{
		FCapabilityValidationRegistry::RegisterValidation(
			FName(TEXT("LootContainer")),
			&ULootContainerCapabilityComponent::ValidateCapabilityProperties);
	}
} GLootContainerValidationAutoReg;
#endif

ULootContainerCapabilityComponent::ULootContainerCapabilityComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void ULootContainerCapabilityComponent::BeginPlay()
{
	Super::BeginPlay();

	// Initialize runtime loot from InstanceLoot (server only)
	// InstanceLoot is set by:
	// 1. ObjectSpawnUtility reading sections.loot from ObjectDefinition
	// 2. Designer editing per-instance data in editor
	// 3. Procedural code via AddLootEntry()
	if (AActor* Owner = GetOwner())
	{
		if (Owner->HasAuthority())
		{
			InitializeRuntimeLoot();
		}
	}
}

FPrimaryAssetId ULootContainerCapabilityComponent::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(FPrimaryAssetType(TEXT("CapabilityComponent")), FName(TEXT("LootContainer")));
}

void ULootContainerCapabilityComponent::InitializeRuntimeLoot()
{
	// Copy InstanceLoot to RuntimeLootEntries
	// InstanceLoot is already populated by spawn utility or designer
	if (InstanceLoot.Num() > 0)
	{
		RuntimeLootEntries.Reserve(InstanceLoot.Num());
		for (const FLootEntryView& Entry : InstanceLoot)
		{
			if (Entry.IsValid())
			{
				RuntimeLootEntries.Add(Entry);
			}
		}

		UE_LOG(LogProjectObjectCapabilities, Log,
			TEXT("LootContainer: Initialized %d entries on %s"),
			RuntimeLootEntries.Num(),
			*GetOwner()->GetName());
	}
}

void ULootContainerCapabilityComponent::AddLootEntry(FPrimaryAssetId ObjectId, int32 Quantity)
{
	if (!ObjectId.IsValid() || Quantity < 1)
	{
		return;
	}

	FLootEntryView Entry;
	Entry.ObjectId = ObjectId;
	Entry.Quantity = Quantity;
	RuntimeLootEntries.Add(Entry);
}

void ULootContainerCapabilityComponent::ClearLoot()
{
	RuntimeLootEntries.Empty();
	bLooted = false;
}

TArray<FLootEntryView> ULootContainerCapabilityComponent::GetLootEntries_Implementation() const
{
	return RuntimeLootEntries;
}

bool ULootContainerCapabilityComponent::IsLootEmpty_Implementation() const
{
	return bLooted || RuntimeLootEntries.Num() == 0;
}

void ULootContainerCapabilityComponent::ConsumeLootSource_Implementation()
{
	AActor* Owner = GetOwner();
	if (Owner && !Owner->HasAuthority())
	{
		UE_LOG(LogProjectObjectCapabilities, Warning,
			TEXT("ConsumeLootSource called without authority on %s"), *Owner->GetName());
		return;
	}

	bLooted = true;
	RuntimeLootEntries.Empty();
	OnContainerLooted.Broadcast();

	if (bOneTimeUse && Owner)
	{
		Owner->Destroy();
	}
}

TArray<FCapabilityValidationResult> ULootContainerCapabilityComponent::ValidateCapabilityProperties(
	const TMap<FName, FString>& Properties)
{
	TArray<FCapabilityValidationResult> Results;
	// No required fields - loot entries come from InstanceLoot (set by spawn utility or designer)
	return Results;
}

bool ULootContainerCapabilityComponent::OnComponentInteract_Implementation(AActor* Instigator)
{
	if (!Instigator)
	{
		return false;
	}

	if (bLooted)
	{
		UE_LOG(LogProjectObjectCapabilities, Verbose,
			TEXT("LootContainer: %s already looted"),
			*GetOwner()->GetName());
		return false;
	}

	// Broadcast container opened - inventory system handles loot transfer
	OnContainerOpened.Broadcast(Instigator);

	UE_LOG(LogProjectObjectCapabilities, Verbose,
		TEXT("LootContainer interaction on %s (%d entries)"),
		*GetOwner()->GetName(),
		RuntimeLootEntries.Num());

	// Inventory system handles loot transfer and calls ConsumeLootSource when done
	//
	// FUTURE: Hold-to-loot implementation
	// 1. Add OpenTime property to component
	// 2. Return false here, start timer/progress
	// 3. Interaction system shows progress UI
	// 4. On timer complete, broadcast OnContainerOpened
	// 5. Requires IInteractableTarget::GetInteractionDuration() or similar

	return true;
}

FText ULootContainerCapabilityComponent::GetInteractionLabel_Implementation() const
{
	if (bLooted)
	{
		return NSLOCTEXT("LootContainerCapability", "EmptyLabel", "Empty");
	}
	return NSLOCTEXT("LootContainerCapability", "SearchLabel", "Search");
}

void ULootContainerCapabilityComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ULootContainerCapabilityComponent, bLooted);
	DOREPLIFETIME(ULootContainerCapabilityComponent, RuntimeLootEntries);
}
