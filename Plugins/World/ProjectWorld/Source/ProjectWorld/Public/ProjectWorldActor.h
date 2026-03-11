#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/IDefinitionIdProvider.h"
#include "Interfaces/IObjectDefinitionHost.h"

#include "ProjectWorldActor.generated.h"

/**
 * Base actor for world-placed objects that link to the data-driven system.
 *
 * Provides a stable UUID for matching actors to world data entries.
 * Feature plugins derive from this to add specific behavior (pickups, NPCs, etc.)
 * while keeping the data link in a common base.
 *
 * This actor knows nothing about specific features - it only provides the link.
 */
UCLASS(Abstract, Blueprintable)
class PROJECTWORLD_API AProjectWorldActor : public AActor, public IDefinitionIdProvider, public IObjectDefinitionHostInterface
{
	GENERATED_BODY()

public:
	AProjectWorldActor();

	/** Unique identifier linking this actor to world data. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Data", meta = (ExposeOnSpawn = true))
	FGuid DataId;

	/** Check if this actor has a valid data link. */
	UFUNCTION(BlueprintPure, Category = "World Data")
	bool HasValidDataId() const { return DataId.IsValid(); }

	// -------------------------------------------------------------------------
	// Definition Tracking (metadata + interface)
	// LEGACY_OBJECT_PARENT_GENERALIZATION(L004): Definition metadata hosting is still inheritance-based on
	// AProjectWorldActor. Remove when host ownership moves to IObjectDefinitionHostInterface/UObjectDefinitionHostComponent.
	// -------------------------------------------------------------------------
	// Metadata: ObjectDefinitionId, AppliedStructureHash, AppliedContentHash
	// Interface: IDefinitionApplicable (implemented by derived classes)
	//
	// SOC:
	// - Actor: Knows HOW to apply its definition type (ApplyDefinition implementation)
	// - Subsystem: Knows WHEN to update (definition regenerated, actor loaded, orchestration)

	/**
	 * Links this actor to its source ObjectDefinition (metadata only).
	 * Set by ProjectObjectActorFactory during spawn.
	 * Used by DefinitionActorSyncSubsystem to find actors when definition changes.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Definition")
	FPrimaryAssetId ObjectDefinitionId;

	// --- IDefinitionIdProvider ---
	virtual FPrimaryAssetId GetObjectDefinitionId_Implementation() const override { return ObjectDefinitionId; }

	// --- IObjectDefinitionHostInterface ---
	virtual FPrimaryAssetId GetHostedObjectDefinitionId_Implementation() const override { return ObjectDefinitionId; }
	virtual void SetHostedObjectDefinitionId_Implementation(const FPrimaryAssetId& DefinitionId) override { ObjectDefinitionId = DefinitionId; }
	virtual FString GetHostedAppliedStructureHash_Implementation() const override { return AppliedStructureHash; }
	virtual void SetHostedAppliedStructureHash_Implementation(const FString& StructureHash) override { AppliedStructureHash = StructureHash; }
	virtual FString GetHostedAppliedContentHash_Implementation() const override { return AppliedContentHash; }
	virtual void SetHostedAppliedContentHash_Implementation(const FString& ContentHash) override { AppliedContentHash = ContentHash; }

	/**
	 * Hash of structural elements at time of spawn/last update.
	 * Compared against UObjectDefinition::DefinitionStructureHash.
	 * Mismatch = structural change = needs Replace.
	 */
	UPROPERTY()
	FString AppliedStructureHash;

	/**
	 * Hash of full definition content at time of spawn/last update.
	 * Compared against UObjectDefinition::DefinitionContentHash.
	 * Mismatch = property change = needs Reapply.
	 */
	UPROPERTY()
	FString AppliedContentHash;

	/**
	 * NOTE: ApplyDefinition() is defined via IDefinitionApplicable interface.
	 * Derived classes (AInteractableActor, AProjectPickupItemActor) implement the interface
	 * to handle their specific definition types (UObjectDefinition, etc.)
	 */

#if WITH_EDITOR
	//~ Begin AActor Interface
	virtual void PreSave(FObjectPreSaveContext SaveContext) override;
	//~ End AActor Interface

	/** Generates a new DataId. Use sparingly - only for newly authored actors. */
	UFUNCTION(CallInEditor, Category = "World Data")
	void GenerateNewDataId();
#endif
};
