// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Engine/StreamableManager.h"
#include "ObjectDefinitionCache.generated.h"

DECLARE_DELEGATE_OneParam(FOnObjectDefinitionLoaded, UObject* /*Object*/);
DECLARE_DELEGATE(FOnWarmupComplete);

/**
 * Cache/loader for ObjectDefinition assets.
 * Uses UAssetManager::LoadPrimaryAssets() for async loading.
 * GetPrimaryAssetObject() is used as fast-path for in-memory objects only.
 *
 * Owned by Inventory feature, created during feature init.
 * Consumers should query IItemDataProvider on the returned object.
 */
UCLASS()
class PROJECTINVENTORY_API UObjectDefinitionCache : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Get loaded object definition (fast-path).
	 * Returns nullptr if not loaded - does NOT trigger a load.
	 */
	UFUNCTION(BlueprintPure, Category = "Inventory|Cache")
	UObject* GetLoaded(FPrimaryAssetId ObjectId) const;

	/**
	 * Request async load with callback.
	 * If already loaded, callback fires immediately.
	 * Multiple requests for same ObjectId share the load handle.
	 */
	void RequestLoad(FPrimaryAssetId ObjectId, FOnObjectDefinitionLoaded OnLoaded);

	/**
	 * Batch warmup from catalog.
	 * Loads all objects asynchronously, calls OnComplete when done.
	 */
	void Warmup(const TArray<FPrimaryAssetId>& ObjectIds, FOnWarmupComplete OnComplete = FOnWarmupComplete());

	/** Check if object is loaded (in memory). */
	UFUNCTION(BlueprintPure, Category = "Inventory|Cache")
	bool IsLoaded(FPrimaryAssetId ObjectId) const;

	/** Get number of pending loads. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Cache")
	int32 GetPendingLoadCount() const { return PendingLoads.Num(); }

private:
	/** Pending load state for deduplication. */
	struct FPendingLoad
	{
		TSharedPtr<FStreamableHandle> Handle;
		TArray<FOnObjectDefinitionLoaded> Callbacks;
	};

	/** Active loads by ObjectId. */
	TMap<FPrimaryAssetId, FPendingLoad> PendingLoads;

	/** Handle single object load completion. */
	void OnObjectLoaded(FPrimaryAssetId ObjectId);

	/** Active warmup handle. */
	TSharedPtr<FStreamableHandle> WarmupHandle;
};
