// Copyright ALIS. All Rights Reserved.

#include "Pickup/PickupCapabilityComponent.h"
#include "CapabilityValidationRegistry.h"
#include "Interfaces/IDefinitionIdProvider.h"
#include "Net/UnrealNetwork.h"
#include "ProjectObjectCapabilitiesModule.h"

// Self-registration: component owns its validation registration
#if WITH_EDITOR
static struct FPickupValidationAutoReg
{
	FPickupValidationAutoReg()
	{
		FCapabilityValidationRegistry::RegisterValidation(
			FName(TEXT("Pickup")),
			&UPickupCapabilityComponent::ValidateCapabilityProperties);
	}
} GPickupValidationAutoReg;
#endif

UPickupCapabilityComponent::UPickupCapabilityComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UPickupCapabilityComponent::BeginPlay()
{
	Super::BeginPlay();

	// Initialize runtime quantity only on authority (server)
	// Clients receive replicated value - don't overwrite it
	if (AActor* Owner = GetOwner())
	{
		if (Owner->HasAuthority())
		{
			Quantity = InitialQuantity;
		}
	}
}

FPrimaryAssetId UPickupCapabilityComponent::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(FPrimaryAssetType(TEXT("CapabilityComponent")), FName(TEXT("Pickup")));
}

FPrimaryAssetId UPickupCapabilityComponent::GetObjectDefinitionId_Implementation() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return FPrimaryAssetId();
	}

	if (Owner->Implements<UDefinitionIdProvider>())
	{
		const FPrimaryAssetId ObjectId = IDefinitionIdProvider::Execute_GetObjectDefinitionId(Owner);
		if (!ObjectId.IsValid() && !bLoggedMissingDefinitionId)
		{
			UE_LOG(LogProjectObjectCapabilities, Warning,
				TEXT("Pickup: Invalid ObjectDefinitionId on %s (IDefinitionIdProvider returned empty)"),
				*Owner->GetName());
			ensureMsgf(false, TEXT("Pickup missing ObjectDefinitionId on %s"), *Owner->GetName());
			bLoggedMissingDefinitionId = true;
		}

		return ObjectId;
	}

	// Generic fallback: owner may host definition metadata via a component.
	for (UActorComponent* Comp : Owner->GetComponents())
	{
		if (Comp && Comp->Implements<UDefinitionIdProvider>())
		{
			const FPrimaryAssetId ObjectId = IDefinitionIdProvider::Execute_GetObjectDefinitionId(Comp);
			if (!ObjectId.IsValid() && !bLoggedMissingDefinitionId)
			{
				UE_LOG(LogProjectObjectCapabilities, Warning,
					TEXT("Pickup: Invalid ObjectDefinitionId on component %s (owner %s)"),
					*Comp->GetName(),
					*Owner->GetName());
				bLoggedMissingDefinitionId = true;
			}

			return ObjectId;
		}
	}

	if (!bLoggedMissingDefinitionId)
	{
		UE_LOG(LogProjectObjectCapabilities, Warning,
			TEXT("Pickup: Owner %s and its components do not implement IDefinitionIdProvider"),
			*Owner->GetName());
		ensureMsgf(false, TEXT("Pickup owner/components missing IDefinitionIdProvider: %s"), *Owner->GetName());
		bLoggedMissingDefinitionId = true;
	}

	return FPrimaryAssetId();
}

int32 UPickupCapabilityComponent::GetQuantity_Implementation() const
{
	return Quantity;
}

void UPickupCapabilityComponent::SetQuantity_Implementation(int32 NewQuantity)
{
	AActor* Owner = GetOwner();
	if (Owner && !Owner->HasAuthority())
	{
		UE_LOG(LogProjectObjectCapabilities, Warning,
			TEXT("SetQuantity called without authority on %s"), *Owner->GetName());
		return;
	}

	Quantity = FMath::Max(0, NewQuantity);
	InitialQuantity = FMath::Max(0, NewQuantity);

	if (Quantity <= 0 && Owner)
	{
		Owner->Destroy();
	}
}

void UPickupCapabilityComponent::Consume_Implementation(int32 ConsumeQuantity)
{
	if (ConsumeQuantity <= 0)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (Owner && !Owner->HasAuthority())
	{
		UE_LOG(LogProjectObjectCapabilities, Warning,
			TEXT("Consume called without authority on %s"), *Owner->GetName());
		return;
	}

	Quantity = FMath::Max(0, Quantity - ConsumeQuantity);

	if (Quantity <= 0 && Owner)
	{
		Owner->Destroy();
	}
}

TArray<FCapabilityValidationResult> UPickupCapabilityComponent::ValidateCapabilityProperties(
	const TMap<FName, FString>& Properties)
{
	TArray<FCapabilityValidationResult> Results;

	// InitialQuantity validation (optional, defaults to 1)
	const FString* QuantityValue = Properties.Find(TEXT("InitialQuantity"));
	if (QuantityValue && !QuantityValue->IsEmpty())
	{
		int32 Qty = FCString::Atoi(**QuantityValue);
		if (Qty < 1)
		{
			Results.Add({TEXT("InitialQuantity"), TEXT("Must be >= 1"), true}); // Warning
		}
	}

	// PickupTime validation (optional, defaults to 0)
	const FString* TimeValue = Properties.Find(TEXT("PickupTime"));
	if (TimeValue && !TimeValue->IsEmpty())
	{
		float Time = FCString::Atof(**TimeValue);
		if (Time < 0.0f)
		{
			Results.Add({TEXT("PickupTime"), TEXT("Cannot be negative"), true}); // Warning
		}
	}

	// No required fields - pickup is a simple "can be picked up" marker
	// Item data validation happens at the ObjectDefinition.Item level

	return Results;
}

bool UPickupCapabilityComponent::OnComponentInteract_Implementation(AActor* Instigator)
{
	if (!Instigator)
	{
		return false;
	}

	// Pickup logic is handled by inventory system via delegate
	// This component just:
	// 1. Marks the object as pickupable (existence of component)
	// 2. Broadcasts pickup attempt for inventory system to handle
	//
	// Inventory system will:
	// 1. Read ObjectDefinition.Item for item data (NOT from this component)
	// 2. Check if player has space
	// 3. Add to inventory
	// 4. Destroy actor on success

	// Broadcast attempt only - success is determined by inventory system
	OnPickupAttempted.Broadcast(Instigator);

	UE_LOG(LogProjectObjectCapabilities, Verbose,
		TEXT("Pickup interaction on %s (Qty: %d)"),
		*GetOwner()->GetName(),
		Quantity);

	return true;
}

FText UPickupCapabilityComponent::GetInteractionLabel_Implementation() const
{
	return NSLOCTEXT("PickupCapability", "PickupLabel", "Pick up");
}

void UPickupCapabilityComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UPickupCapabilityComponent, Quantity);
}
