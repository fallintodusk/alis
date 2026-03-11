// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "IDefinitionApplicable.generated.h"

/**
 * Interface for actors that can apply definitions (data-driven updates).
 *
 * Implemented by actors spawned from JSON definitions (ObjectDefinition, etc.)
 * Used by DefinitionActorSyncSubsystem to update actors when JSON changes.
 *
 * Each actor type implements custom logic for its definition type:
 * - AInteractableActor: handles UObjectDefinition (meshes + capabilities)
 * - AProjectPickupItemActor: handles UObjectDefinition with pickup capability
 *
 * SOLID: Polymorphic behavior via interface, no coupling to specific definition types.
 */
UINTERFACE(MinimalAPI, Blueprintable)
class UDefinitionApplicable : public UInterface
{
	GENERATED_BODY()
};

class PROJECTWORLD_API IDefinitionApplicable
{
	GENERATED_BODY()

public:
	/**
	 * Apply definition to this actor (in-place property updates).
	 *
	 * Called by:
	 * - Mode 1: DefinitionActorSyncSubsystem when actor loads (World Partition streaming)
	 * - Mode 2: Actor PreSave hook during offline resave (WorldPartitionResaveActorsBuilder)
	 *
	 * Implementation should:
	 * - Cast Definition to expected type (UObjectDefinition or other custom definitions)
	 * - Update mesh assets, materials, transforms, properties
	 * - Update hash values (AppliedStructureHash, AppliedContentHash) on success
	 * - Return false if definition type mismatch or update failed
	 *
	 * @param Definition The definition asset to apply (UPrimaryDataAsset subclass)
	 * @return true if successfully applied, false if type mismatch or error
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Definition")
	bool ApplyDefinition(UPrimaryDataAsset* Definition);
};
