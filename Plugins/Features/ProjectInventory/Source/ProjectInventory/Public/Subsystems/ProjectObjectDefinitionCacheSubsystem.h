// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ProjectObjectDefinitionCacheSubsystem.generated.h"

class UObjectDefinitionCache;

/**
 * Game-instance scoped owner for inventory object-definition resolution.
 *
 * Components bind to this subsystem; they never create their own runtime
 * definition caches. This keeps residency, diagnostics, and async state shared
 * across all inventory components in one game instance.
 */
UCLASS()
class PROJECTINVENTORY_API UProjectObjectDefinitionCacheSubsystem final : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UObjectDefinitionCache* GetCache() const { return Cache; }

	/**
	 * Test-only: swap the live cache for a test double. Subsequent component
	 * rebinds will pick up the replacement. Restoring the original cache after
	 * the test is the caller's responsibility.
	 *
	 * Consumed by: inventory deferred-pickup integration tests. Contract
	 * + pitfall entries: Plugins/Features/ProjectInventory/docs/pitfalls.md
	 * (object definition cache / deferred-pickup).
	 */
	void OverrideCacheForTests(UObjectDefinitionCache* NewCache) { Cache = NewCache; }

private:
	UPROPERTY()
	TObjectPtr<UObjectDefinitionCache> Cache = nullptr;
};
