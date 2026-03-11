// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UObjectDefinition;
class AActor;
class UWorld;
class UActorComponent;
class UStaticMeshComponent;

/**
 * Runtime spawn utility for ObjectDefinition.
 *
 * Shared by editor (ProjectObjectActorFactory) and runtime (inventory drops).
 * Extracts spawn logic from editor-only factory to enable runtime spawning.
 *
 * Architecture: Phase 2 of merge_pickups_into_objects.md
 */
namespace ProjectObjectSpawn
{
	/**
	 * Spawn actor from ObjectDefinition.
	 *
	 * Creates actor with:
	 * - Mesh components from definition
	 * - Capability components (resolved via FCapabilityRegistry)
	 * - Property overrides applied via ImportText
	 * - Definition tracking (ObjectDefinitionId, hashes)
	 *
	 * @param World Target world
	 * @param Def Source definition (must be valid)
	 * @param Transform Spawn transform
	 * @param SpawnParams Optional spawn parameters (for editor level placement)
	 * @param OutError Optional error output (set on failure)
	 * @param ActorClass Optional actor class override (defaults to AInteractableActor)
	 * @return Spawned actor or nullptr on failure
	 */
	PROJECTOBJECT_API AActor* SpawnFromDefinition(
		UWorld* World,
		const UObjectDefinition* Def,
		const FTransform& Transform,
		const FActorSpawnParameters& SpawnParams = FActorSpawnParameters(),
		FText* OutError = nullptr,
		TSubclassOf<AActor> ActorClass = nullptr);

	/**
	 * Spawn actor from ObjectDefinition by asset ID.
	 *
	 * Convenience overload that handles loading. Uses fast path if already
	 * in memory, otherwise performs synchronous load from AssetManager.
	 *
	 * @param World Target world
	 * @param ObjectId Asset ID of ObjectDefinition (will be loaded if needed)
	 * @param Transform Spawn transform
	 * @param SpawnParams Optional spawn parameters
	 * @param OutError Optional error output (set on failure)
	 * @param ActorClass Optional actor class override (defaults to AInteractableActor)
	 * @return Spawned actor or nullptr on failure
	 */
	PROJECTOBJECT_API AActor* SpawnFromDefinition(
		UWorld* World,
		FPrimaryAssetId ObjectId,
		const FTransform& Transform,
		const FActorSpawnParameters& SpawnParams = FActorSpawnParameters(),
		FText* OutError = nullptr,
		TSubclassOf<AActor> ActorClass = nullptr);

	/**
	 * Compute structure hash for definition tracking.
	 *
	 * Hash includes mesh IDs and capability types (sorted for determinism).
	 * Used to detect structural changes requiring actor Replace.
	 *
	 * @param Def Source definition
	 * @return Structure hash (0 if Def is null)
	 */
	PROJECTOBJECT_API uint32 ComputeStructureHash(const UObjectDefinition* Def);

	/**
	 * Set property on object using ImportText.
	 *
	 * Handles special cases:
	 * - FGameplayTag: Parses "Tag.Name" format
	 * - FVector: Parses "(X,Y,Z)" format
	 * - FRotator: Parses "(P,Y,R)" format
	 *
	 * @param Object Target object
	 * @param PropertyName UPROPERTY name
	 * @param Value String representation
	 * @return true if property was set
	 */
	PROJECTOBJECT_API bool SetPropertyByName(UObject* Object, FName PropertyName, const FString& Value);

	/**
	 * Set property using object pointer.
	 *
	 * Used for object reference assignment when applying definition properties.
	 *
	 * @param Object Target object
	 * @param PropertyName UPROPERTY name
	 * @param ObjectValue Object pointer value
	 * @return true if property was set
	 */
	PROJECTOBJECT_API bool SetPropertyByName(UObject* Object, FName PropertyName, UObject* ObjectValue);
}
