// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/IInteractableTarget.h"
#include "Interfaces/IWorldContainerSessionSource.h"
#include "CapabilityValidationRegistry.h"
#include "LootContainerCapabilityComponent.generated.h"

USTRUCT(BlueprintType)
struct PROJECTOBJECTCAPABILITIES_API FWorldContainerRuntimeEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LootContainer|Runtime")
	FPrimaryAssetId ObjectId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LootContainer|Runtime", meta = (ClampMin = 1))
	int32 Quantity = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LootContainer|Runtime")
	int32 InstanceId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LootContainer|Runtime")
	FIntPoint GridPos = FIntPoint(-1, -1);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LootContainer|Runtime")
	bool bRotated = false;

	bool IsValid() const
	{
		return ObjectId.IsValid() && Quantity > 0 && InstanceId > 0 && GridPos.X >= 0 && GridPos.Y >= 0;
	}
};

/**
 * Loot container capability - marks object as a lootable container.
 *
 * This is a WORLD INTERACTION capability only. It answers:
 * "Can the player loot items from this container?"
 *
 * Canonical authored contents come from ObjectDefinition.Sections["Storage"].
 * This component owns runtime world-container state and exposes it through the
 * world-container session contract only.
 *
 * When interacted:
 * - Inventory system reads loot entries
 * - Transfers items to player inventory
 * - Optionally destroys container after the session ends (OneTimeUse)
 *
 * Pattern: Passive data holder with validation (like PickupCapabilityComponent).
 * Priority: 0 (runs after access checks like Lockable)
 */
UCLASS(ClassGroup = (ProjectCapabilities), meta = (BlueprintSpawnableComponent))
class PROJECTOBJECTCAPABILITIES_API ULootContainerCapabilityComponent
	: public UActorComponent
	, public IInteractableComponentTargetInterface
	, public IWorldContainerSessionSource
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
	// IWorldContainerSessionSource
	// -------------------------------------------------------------------------

	virtual FText GetContainerDisplayLabel_Implementation() const override;
	virtual FInventoryContainerView GetContainerView_Implementation() const override;
	virtual TArray<FInventoryEntryView> GetContainerEntryViews_Implementation() const override;
	virtual FWorldContainerKey GetWorldContainerKey_Implementation() const override;
	virtual bool SupportsContainerSession_Implementation(EContainerSessionMode Mode) const override;
	virtual bool TryBeginContainerSession_Implementation(
		AActor* Instigator,
		FGuid SessionId,
		EContainerSessionMode Mode,
		FText& OutError) override;
	virtual bool ConsumeContainerEntries_Implementation(
		FGuid SessionId,
		const TArray<FContainerEntryTransfer>& Entries,
		FText& OutError) override;
	virtual bool CanStoreContainerEntries_Implementation(
		FGuid SessionId,
		const TArray<FContainerEntryTransfer>& Entries,
		FText& OutError) const override;
	virtual bool StoreContainerEntries_Implementation(
		FGuid SessionId,
		const TArray<FContainerEntryTransfer>& Entries,
		FText& OutError) override;
	virtual bool EndContainerSession_Implementation(FGuid SessionId) override;
	virtual bool HasActiveFullOpenSession_Implementation() const override;

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
	bool bOneTimeUse = false;

	// -------------------------------------------------------------------------
	// Per-Instance Loot (Designer Editable)
	// -------------------------------------------------------------------------

	/**
	 * Per-instance loot entries set by designer in editor.
	 * PRIORITY: If set (non-empty), overrides seeded definition contents.
	 *
	 * Use cases:
	 * - Designer places generic crate, sets specific loot for this instance
	 * - Unique containers that don't need a separate JSON definition
	 *
	 * Leave empty to use seeded definition contents from sections.storage.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LootContainer|Contents", meta = (TitleProperty = "ObjectId"))
	TArray<FLootEntryView> InstanceLoot;

	// -------------------------------------------------------------------------
	// Runtime State (replicated)
	// -------------------------------------------------------------------------

	/** True if container has been looted (runtime state). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "LootContainer|Runtime")
	bool bLooted = false;

	/** Runtime container entries - initialized from InstanceLoot. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "LootContainer|Runtime")
	TArray<FWorldContainerRuntimeEntry> RuntimeEntries;

	/** Stable runtime key for this world container instance. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "LootContainer|Runtime")
	FWorldContainerKey ContainerKey;

	/** Runtime storage grid spec bridged from sections.storage by spawn code. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "LootContainer|Runtime")
	FIntPoint RuntimeGridSize = FIntPoint::ZeroValue;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "LootContainer|Runtime")
	float RuntimeMaxWeight = 0.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "LootContainer|Runtime")
	float RuntimeMaxVolume = 0.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "LootContainer|Runtime")
	int32 RuntimeMaxCells = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "LootContainer|Runtime")
	int32 RuntimeCellDepthUnits = 1;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "LootContainer|Runtime")
	FGameplayTagContainer RuntimeAllowedTags;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "LootContainer|Runtime")
	bool bRuntimeAllowRotation = true;

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
	/** Copy InstanceLoot seed entries into runtime container entries on BeginPlay. */
	void InitializeRuntimeLoot();

	/** Initialize stable runtime key for this world container instance. */
	void InitializeContainerKey();

	int32 AllocateRuntimeEntryInstanceId();

	/** Log once when ObjectDefinitionId cannot be resolved. */
	mutable bool bLoggedMissingDefinitionId = false;

	/** Next runtime entry id assigned on the authority side. */
	int32 NextRuntimeEntryInstanceId = 1;

	/** Active full-open session id on the authority side. */
	FGuid ActiveFullOpenSessionId;

	/** Current full-open session instigator on the authority side. */
	TWeakObjectPtr<AActor> ActiveFullOpenInstigator;
};
