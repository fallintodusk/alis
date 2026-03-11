// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/IInteractableTarget.h"
#include "Interfaces/IPickupSource.h"
#include "CapabilityValidationRegistry.h"
#include "PickupCapabilityComponent.generated.h"

/**
 * World pickup capability - marks object as pickupable.
 *
 * This is a WORLD INTERACTION capability only. It answers:
 * "Can the player pick up this object from the world?"
 *
 * Item identity data (DisplayName, Weight, Tags, etc.) lives in
 * ObjectDefinition.Item block - NOT here. See Layer Contract in
 * merge_pickups_into_objects.md for architecture details.
 *
 * When interacted:
 * - Inventory system reads ObjectDefinition.Item for item data
 * - Creates inventory entry
 * - Destroys world object (if pickup successful)
 *
 * Pattern: Passive data holder with validation (like LockableComponent).
 * Priority: 0 (runs after access checks like Lockable)
 */
UCLASS(ClassGroup = (ProjectCapabilities), meta = (BlueprintSpawnableComponent))
class PROJECTOBJECTCAPABILITIES_API UPickupCapabilityComponent
	: public UActorComponent
	, public IInteractableComponentTargetInterface
	, public IPickupSource
{
	GENERATED_BODY()

public:
	UPickupCapabilityComponent();

	// --- Stable ID for Capability Registry ---
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	// --- IInteractableComponentTargetInterface ---
	virtual int32 GetInteractPriority_Implementation() const override { return 0; }
	virtual bool OnComponentInteract_Implementation(AActor* Instigator) override;
	virtual FText GetInteractionLabel_Implementation() const override;

	// -------------------------------------------------------------------------
	// IPickupSource
	// -------------------------------------------------------------------------

	virtual FPrimaryAssetId GetObjectDefinitionId_Implementation() const override;
	virtual int32 GetQuantity_Implementation() const override;
	virtual void SetQuantity_Implementation(int32 Quantity) override;
	virtual void Consume_Implementation(int32 Quantity) override;

	// -------------------------------------------------------------------------
	// Validation (static function for registry)
	// -------------------------------------------------------------------------

	/** Validates capability properties. Registered with FCapabilityValidationRegistry. */
	static TArray<FCapabilityValidationResult> ValidateCapabilityProperties(
		const TMap<FName, FString>& Properties);

	// -------------------------------------------------------------------------
	// Pickup Interaction Properties (world interaction only)
	// -------------------------------------------------------------------------

	/**
	 * Initial quantity when spawned in world.
	 * For stackable items, this is how many spawn together.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup")
	int32 InitialQuantity = 1;

	/**
	 * Time in seconds to complete pickup (0 = instant).
	 * For items that require holding interact.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup")
	float PickupTime = 0.0f;

	// -------------------------------------------------------------------------
	// Runtime State (replicated)
	// -------------------------------------------------------------------------

	/** Current quantity in world (runtime state, can change). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Pickup|Runtime")
	int32 Quantity = 1;

	// -------------------------------------------------------------------------
	// Delegates
	// -------------------------------------------------------------------------

	/** Called when pickup is attempted. Inventory system handles success/failure separately. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPickupAttempted, AActor*, Instigator);

	UPROPERTY(BlueprintAssignable, Category = "Pickup")
	FOnPickupAttempted OnPickupAttempted;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	/** Log once when ObjectDefinitionId cannot be resolved. */
	mutable bool bLoggedMissingDefinitionId = false;
};
