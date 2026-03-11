// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/IInteractableTarget.h"
#include "Interfaces/ILootSource.h"
#include "CapabilityValidationRegistry.h"
#include "LootContainerCapabilityComponent.generated.h"

/**
 * Loot container capability - marks object as a lootable container.
 *
 * This is a WORLD INTERACTION capability only. It answers:
 * "Can the player loot items from this container?"
 *
 * Loot entries are stored in ObjectDefinition.Sections["Loot"].
 * This component reads from that section and exposes it via ILootSource.
 *
 * When interacted:
 * - Inventory system reads loot entries
 * - Transfers items to player inventory
 * - Optionally destroys container (OneTimeUse)
 *
 * Pattern: Passive data holder with validation (like PickupCapabilityComponent).
 * Priority: 0 (runs after access checks like Lockable)
 */
UCLASS(ClassGroup = (ProjectCapabilities), meta = (BlueprintSpawnableComponent))
class PROJECTOBJECTCAPABILITIES_API ULootContainerCapabilityComponent
	: public UActorComponent
	, public IInteractableComponentTargetInterface
	, public ILootSource
{
	GENERATED_BODY()

public:
	ULootContainerCapabilityComponent();

	// --- Stable ID for Capability Registry ---
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	// --- IInteractableComponentTargetInterface ---
	virtual int32 GetInteractPriority_Implementation() const override { return 0; }
	virtual bool OnComponentInteract_Implementation(AActor* Instigator) override;
	virtual FText GetInteractionLabel_Implementation() const override;

	// -------------------------------------------------------------------------
	// ILootSource
	// -------------------------------------------------------------------------

	virtual TArray<FLootEntryView> GetLootEntries_Implementation() const override;
	virtual bool IsLootEmpty_Implementation() const override;
	virtual void ConsumeLootSource_Implementation() override;

	// -------------------------------------------------------------------------
	// Validation (static function for registry)
	// -------------------------------------------------------------------------

	/** Validates capability properties. Registered with FCapabilityValidationRegistry. */
	static TArray<FCapabilityValidationResult> ValidateCapabilityProperties(
		const TMap<FName, FString>& Properties);

	// -------------------------------------------------------------------------
	// Container Properties (world interaction only)
	// -------------------------------------------------------------------------

	/**
	 * If true, container is destroyed after looting.
	 * If false, container remains (can be re-opened but empty).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LootContainer")
	bool bOneTimeUse = true;

	// -------------------------------------------------------------------------
	// Per-Instance Loot (Designer Editable)
	// -------------------------------------------------------------------------

	/**
	 * Per-instance loot entries set by designer in editor.
	 * PRIORITY: If set (non-empty), overrides sections.loot from ObjectDefinition.
	 *
	 * Use cases:
	 * - Designer places generic crate, sets specific loot for this instance
	 * - Unique containers that don't need a separate JSON definition
	 *
	 * Leave empty to use sections.loot from ObjectDefinition (for constant containers
	 * like MedCabinet that always have same loot).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LootContainer|Contents", meta = (TitleProperty = "ObjectId"))
	TArray<FLootEntryView> InstanceLoot;

	// -------------------------------------------------------------------------
	// Runtime State (replicated)
	// -------------------------------------------------------------------------

	/** True if container has been looted (runtime state). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "LootContainer|Runtime")
	bool bLooted = false;

	/** Runtime loot entries - initialized from InstanceLoot or definition. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "LootContainer|Runtime")
	TArray<FLootEntryView> RuntimeLootEntries;

	// -------------------------------------------------------------------------
	// Runtime API (for procedural generation)
	// -------------------------------------------------------------------------

	/**
	 * Add loot entry at runtime (server only).
	 * Use this for procedural loot generation.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "LootContainer")
	void AddLootEntry(FPrimaryAssetId ObjectId, int32 Quantity = 1);

	/**
	 * Clear all loot entries (server only).
	 * Use before re-filling with procedural loot.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "LootContainer")
	void ClearLoot();

	// -------------------------------------------------------------------------
	// Delegates
	// -------------------------------------------------------------------------

	/** Called when container is opened. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnContainerOpened, AActor*, Instigator);

	UPROPERTY(BlueprintAssignable, Category = "LootContainer")
	FOnContainerOpened OnContainerOpened;

	/** Called when all loot is taken. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnContainerLooted);

	UPROPERTY(BlueprintAssignable, Category = "LootContainer")
	FOnContainerLooted OnContainerLooted;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	/** Copy InstanceLoot to RuntimeLootEntries on BeginPlay. */
	void InitializeRuntimeLoot();

	/** Log once when ObjectDefinitionId cannot be resolved. */
	mutable bool bLoggedMissingDefinitionId = false;
};
