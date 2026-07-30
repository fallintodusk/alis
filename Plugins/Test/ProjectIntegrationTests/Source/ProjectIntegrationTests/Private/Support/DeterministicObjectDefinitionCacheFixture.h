// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IItemDataProvider.h"
#include "Services/ObjectDefinitionCache.h"
#include "DeterministicObjectDefinitionCacheFixture.generated.h"

/**
 * Test-only UObject that implements IItemDataProvider so we can stage a
 * valid FItemDataView into the deterministic cache without needing a real
 * UObjectDefinition asset on disk.
 */
UCLASS()
class UTestItemDataProvider : public UObject, public IItemDataProvider
{
	GENERATED_BODY()

public:
	void SetItemData(const FItemDataView& InData) { Data = InData; }

	virtual FItemDataView GetItemDataView_Implementation() const override { return Data; }

private:
	FItemDataView Data;
};

/**
 * Test-only UObject that does NOT implement IItemDataProvider. Used by the
 * "terminal hard fail after load" test to exercise the InvalidProvider branch.
 */
UCLASS()
class UTestPlainObject : public UObject
{
	GENERATED_BODY()
};

/**
 * Deterministic subclass of UObjectDefinitionCache for the deferred-pickup
 * race tests. Lets the test control callback timing exactly:
 *
 *   - StageObjectAsResident(Id, Obj) registers Obj as the loaded payload
 *     but does NOT fire any pending callbacks. GetLoaded/GetLoadState keep
 *     reporting Missing until the test calls ResolvePendingLoads.
 *   - RequestLoad captures the callback instead of kicking AssetManager.
 *   - ResolvePendingLoads fires all captured callbacks with either the
 *     staged resident payload (success case) or nullptr (terminal fail).
 *
 * Why virtual subclass instead of a parallel mock class:
 *   - The inventory component already holds UObjectDefinitionCache* and
 *     queries it via GetLoaded / GetLoadState / RequestLoad. Keeping the
 *     same base type means production code stays untouched.
 *   - The UCLASS macro lets tests NewObject<>() it like any UObject.
 */
UCLASS()
class UDeterministicObjectDefinitionCache : public UObjectDefinitionCache
{
	GENERATED_BODY()

public:
	/**
	 * Stage a concrete object as the resolved payload for an id. Does not
	 * change GetLoaded / GetLoadState until ResolvePendingLoads fires.
	 */
	void StageObjectAsResident(const FPrimaryAssetId& Id, UObject* Object)
	{
		StagedObjects.Add(Id, Object);
	}

	/** Explicitly mark an id so it will resolve with nullptr (terminal fail). */
	void StageObjectAsUnloadable(const FPrimaryAssetId& Id)
	{
		StagedObjects.Add(Id, nullptr);
	}

	/** Inspect how many callbacks are queued for an id (0 if no pending load). */
	int32 GetCapturedCallbackCount(const FPrimaryAssetId& Id) const
	{
		const TArray<FOnObjectDefinitionLoaded>* Found = CapturedLoads.Find(Id);
		return Found ? Found->Num() : 0;
	}

	int32 GetDistinctRequestCount() const { return CapturedLoads.Num(); }

	/**
	 * Fire all captured callbacks. For each id, picks up the staged object
	 * (resident or nullptr). After firing, "resolved" ids behave as Loaded
	 * if their staged object was non-null.
	 */
	void ResolvePendingLoads()
	{
		for (auto It = CapturedLoads.CreateIterator(); It; ++It)
		{
			const FPrimaryAssetId Id = It.Key();
			TArray<FOnObjectDefinitionLoaded> Callbacks = MoveTemp(It.Value());
			UObject* const* Staged = StagedObjects.Find(Id);
			UObject* Payload = Staged ? *Staged : nullptr;
			if (Payload)
			{
				ResolvedObjects.Add(Id, Payload);
			}
			for (FOnObjectDefinitionLoaded& Callback : Callbacks)
			{
				Callback.ExecuteIfBound(Payload);
			}
		}
		CapturedLoads.Reset();
	}

	// UObjectDefinitionCache overrides
	virtual UObject* GetLoaded(FPrimaryAssetId ObjectId) const override
	{
		if (TObjectPtr<UObject> const* Found = ResolvedObjects.Find(ObjectId))
		{
			return Found->Get();
		}
		return nullptr;
	}

	virtual EObjectDefinitionLoadState GetLoadState(FPrimaryAssetId ObjectId) const override
	{
		if (ResolvedObjects.Contains(ObjectId))
		{
			return EObjectDefinitionLoadState::Loaded;
		}
		if (CapturedLoads.Contains(ObjectId))
		{
			return EObjectDefinitionLoadState::Loading;
		}
		return EObjectDefinitionLoadState::Missing;
	}

	virtual void RequestLoad(FPrimaryAssetId ObjectId, FOnObjectDefinitionLoaded OnLoaded) override
	{
		if (!ObjectId.IsValid())
		{
			OnLoaded.ExecuteIfBound(nullptr);
			return;
		}

		// Fast path: already resolved.
		if (TObjectPtr<UObject> const* Found = ResolvedObjects.Find(ObjectId))
		{
			OnLoaded.ExecuteIfBound(Found->Get());
			return;
		}

		// Capture for later resolution.
		CapturedLoads.FindOrAdd(ObjectId).Add(OnLoaded);
	}

private:
	TMap<FPrimaryAssetId, UObject*> StagedObjects;
	UPROPERTY()
	TMap<FPrimaryAssetId, TObjectPtr<UObject>> ResolvedObjects;
	TMap<FPrimaryAssetId, TArray<FOnObjectDefinitionLoaded>> CapturedLoads;
};
