// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Engine/StreamableManager.h"
#include "ObjectDefinitionCache.generated.h"

DECLARE_DELEGATE_OneParam(FOnObjectDefinitionLoaded, UObject* /*Object*/);
DECLARE_DELEGATE(FOnWarmupComplete);

enum class EObjectDefinitionLoadState : uint8
{
	Missing,
	Loading,
	Loaded
};

struct FObjectDefinitionCacheEntryDiagnostic
{
	FPrimaryAssetId ObjectId;
	EObjectDefinitionLoadState State = EObjectDefinitionLoadState::Missing;
	bool bHasResolvedObject = false;
	bool bHasResidentHandle = false;
	bool bHasPendingLoad = false;
};

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
	 *
	 * Virtual so test doubles can stage "already resident" objects without
	 * hitting AssetManager.
	 */
	UFUNCTION(BlueprintPure, Category = "Inventory|Cache")
	virtual UObject* GetLoaded(FPrimaryAssetId ObjectId) const;

	/**
	 * Resolve current cache state without mutating gameplay callers.
	 *
	 * Virtual so test doubles can report Missing/Loading deterministically.
	 */
	virtual EObjectDefinitionLoadState GetLoadState(FPrimaryAssetId ObjectId) const;

	/**
	 * Request async load with callback.
	 * If already loaded, callback fires immediately.
	 * Multiple requests for same ObjectId share the load handle.
	 *
	 * Virtual so tests can substitute a deterministic test double that
	 * controls callback timing (see DeterministicObjectDefinitionCacheFixture).
	 * Production callers always hit the real implementation.
	 */
	virtual void RequestLoad(FPrimaryAssetId ObjectId, FOnObjectDefinitionLoaded OnLoaded);

	/**
	 * Batch warmup from catalog.
	 * Loads all objects asynchronously, calls OnComplete when done.
	 */
	void Warmup(const TArray<FPrimaryAssetId>& ObjectIds, FOnWarmupComplete OnComplete = FOnWarmupComplete());

	/** Check if object is loaded (in memory). */
	UFUNCTION(BlueprintPure, Category = "Inventory|Cache")
	bool IsLoaded(FPrimaryAssetId ObjectId) const;

	/** Snapshot current cache state for diagnostics/tests. */
	void GetDiagnostics(TArray<FObjectDefinitionCacheEntryDiagnostic>& OutDiagnostics) const;

	/** Get number of pending loads. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Cache")
	int32 GetPendingLoadCount() const { return PendingLoads.Num(); }

	/** Get number of resolved objects kept resident by the cache. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Cache")
	int32 GetResidentDefinitionCount() const { return ResolvedDefinitions.Num(); }

private:
	/** Pending load state for deduplication. */
	struct FPendingLoad
	{
		TSharedPtr<FStreamableHandle> Handle;
		TArray<FOnObjectDefinitionLoaded> Callbacks;
	};

	/** Active loads by ObjectId. */
	TMap<FPrimaryAssetId, FPendingLoad> PendingLoads;

	/** Strong object references retained for the cache lifetime. */
	UPROPERTY()
	TMap<FPrimaryAssetId, TObjectPtr<UObject>> ResolvedDefinitions;

	/** Load handles retained for the cache lifetime. */
	TMap<FPrimaryAssetId, TSharedPtr<FStreamableHandle>> ResidentHandles;

	/** Handle single object load completion. */
	void OnObjectLoaded(FPrimaryAssetId ObjectId);

	/** Find any currently resident asset-manager object without creating a new load. */
	UObject* FindInMemoryObject(FPrimaryAssetId ObjectId) const;

	/** Promote a loaded object into the cache's long-lived residency tables. */
	void CaptureResolvedObject(FPrimaryAssetId ObjectId, TSharedPtr<FStreamableHandle> ResidentHandle);

	/** Active warmup handle. */
	TSharedPtr<FStreamableHandle> WarmupHandle;

	/** Active warmup object ids for post-load promotion. */
	TArray<FPrimaryAssetId> WarmupObjectIds;
};
