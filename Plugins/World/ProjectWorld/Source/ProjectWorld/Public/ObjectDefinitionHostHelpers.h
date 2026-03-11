// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"

class AActor;
class UObject;

namespace ProjectWorldDefinitionHost
{
	/** Returns host object implementing IObjectDefinitionHostInterface on actor or owned component. */
	PROJECTWORLD_API UObject* FindHostObject(AActor* Actor);

	/** Const overload for host lookup. */
	PROJECTWORLD_API const UObject* FindHostObject(const AActor* Actor);

	/**
	 * Ensures host exists on actor.
	 * If actor does not implement host interface and bAllowInjectComponent is true,
	 * injects UObjectDefinitionHostComponent.
	 */
	PROJECTWORLD_API UObject* EnsureHostObject(AActor* Actor, bool bAllowInjectComponent);

	/** Reads host state if host exists. */
	PROJECTWORLD_API bool ReadHostState(
		const AActor* Actor,
		FPrimaryAssetId& OutDefinitionId,
		FString& OutStructureHash,
		FString& OutContentHash);

	/** Writes host state, injecting host component when allowed and needed. */
	PROJECTWORLD_API bool WriteHostState(
		AActor* Actor,
		const FPrimaryAssetId& DefinitionId,
		const FString& StructureHash,
		const FString& ContentHash,
		bool bAllowInjectComponent);

	/** Convenience: read host DefinitionId only. */
	PROJECTWORLD_API FPrimaryAssetId GetDefinitionId(const AActor* Actor);
}

