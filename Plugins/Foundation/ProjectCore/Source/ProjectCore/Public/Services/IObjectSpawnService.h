// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

/**
 * IObjectSpawnService
 *
 * Abstract interface for spawning world actors from ObjectDefinition assets.
 * Follows Dependency Inversion Principle - features depend on abstraction.
 *
 * Architecture:
 *   ProjectInventory (Feature, high-level)
 *       ↓ depends on abstraction
 *   IObjectSpawnService (ProjectCore interface)
 *       ↑ implemented by
 *   ProjectObject module (Resource, low-level)
 *
 * Benefits:
 * - Features can spawn objects without depending on ProjectObject
 * - Proper tier separation (Features don't depend on Resources)
 * - Can mock for testing
 *
 * Usage:
 * @code
 * TSharedPtr<IObjectSpawnService> SpawnService =
 *     FProjectServiceLocator::Resolve<IObjectSpawnService>();
 *
 * if (SpawnService)
 * {
 *     FText Error;
 *     AActor* Actor = SpawnService->SpawnFromDefinition(
 *         GetWorld(), ObjectId, Transform, &Error);
 * }
 * @endcode
 *
 * @see FProjectServiceLocator
 * @see ProjectObjectSpawn::SpawnFromDefinition
 */
class PROJECTCORE_API IObjectSpawnService
{
protected:
	IObjectSpawnService();

public:
	virtual ~IObjectSpawnService();

	/** DLL-stable key for ServiceLocator registration/lookup. */
	static FName ServiceKey()
	{
		static FName Key(TEXT("IObjectSpawnService"));
		return Key;
	}

	/**
	 * Spawn actor from ObjectDefinition by asset ID.
	 *
	 * @param World Target world
	 * @param ObjectId Asset ID of ObjectDefinition (must be loaded)
	 * @param Transform Spawn transform
	 * @param OutError Optional error output (set on failure)
	 * @return Spawned actor or nullptr on failure
	 */
	virtual AActor* SpawnFromDefinition(
		UWorld* World,
		FPrimaryAssetId ObjectId,
		const FTransform& Transform,
		FText* OutError = nullptr) = 0;

	#if WITH_EDITOR
	/** Spawn with deterministic editor identity for persistent generated actors. */
	virtual AActor* SpawnFromDefinitionWithIdentity(
		UWorld* World,
		FPrimaryAssetId ObjectId,
		const FTransform& Transform,
		FName ActorName,
		const FGuid& ActorGuid,
		FText* OutError = nullptr) = 0;
	#endif

	/** Return the provider-owned immutable definition identity used by generators. */
	virtual bool GetDefinitionIdentity(
		FPrimaryAssetId ObjectId,
		FString& OutIdentity,
		FText* OutError = nullptr) = 0;
};
