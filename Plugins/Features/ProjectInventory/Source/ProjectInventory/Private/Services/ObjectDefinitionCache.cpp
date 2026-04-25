// Copyright ALIS. All Rights Reserved.

#include "Services/ObjectDefinitionCache.h"
#include "Engine/AssetManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogObjectDefinitionCache, Log, All);

UObject* UObjectDefinitionCache::FindInMemoryObject(FPrimaryAssetId ObjectId) const
{
	if (!ObjectId.IsValid())
	{
		return nullptr;
	}

	UAssetManager& AM = UAssetManager::Get();
	return AM.GetPrimaryAssetObject<UObject>(ObjectId);
}

void UObjectDefinitionCache::CaptureResolvedObject(FPrimaryAssetId ObjectId, TSharedPtr<FStreamableHandle> ResidentHandle)
{
	if (!ObjectId.IsValid())
	{
		return;
	}

	if (ResidentHandle.IsValid())
	{
		ResidentHandles.Add(ObjectId, ResidentHandle);
	}

	if (UObject* LoadedObject = FindInMemoryObject(ObjectId))
	{
		ResolvedDefinitions.Add(ObjectId, LoadedObject);
	}
}

UObject* UObjectDefinitionCache::GetLoaded(FPrimaryAssetId ObjectId) const
{
	if (!ObjectId.IsValid())
	{
		return nullptr;
	}

	if (const TObjectPtr<UObject>* ExistingObject = ResolvedDefinitions.Find(ObjectId))
	{
		return ExistingObject->Get();
	}

	if (UObject* LoadedObject = FindInMemoryObject(ObjectId))
	{
		const_cast<UObjectDefinitionCache*>(this)->CaptureResolvedObject(ObjectId, nullptr);
		return LoadedObject;
	}

	return nullptr;
}

EObjectDefinitionLoadState UObjectDefinitionCache::GetLoadState(FPrimaryAssetId ObjectId) const
{
	if (!ObjectId.IsValid())
	{
		return EObjectDefinitionLoadState::Missing;
	}

	if (GetLoaded(ObjectId))
	{
		return EObjectDefinitionLoadState::Loaded;
	}

	if (PendingLoads.Contains(ObjectId))
	{
		return EObjectDefinitionLoadState::Loading;
	}

	return EObjectDefinitionLoadState::Missing;
}

bool UObjectDefinitionCache::IsLoaded(FPrimaryAssetId ObjectId) const
{
	return GetLoadState(ObjectId) == EObjectDefinitionLoadState::Loaded;
}

void UObjectDefinitionCache::RequestLoad(FPrimaryAssetId ObjectId, FOnObjectDefinitionLoaded OnLoaded)
{
	if (!ObjectId.IsValid())
	{
		UE_LOG(LogObjectDefinitionCache, Warning, TEXT("RequestLoad: Invalid ObjectId"));
		OnLoaded.ExecuteIfBound(nullptr);
		return;
	}

	// Fast path: already loaded
	if (UObject* Loaded = GetLoaded(ObjectId))
	{
		OnLoaded.ExecuteIfBound(Loaded);
		return;
	}

	// Check for existing pending load
	if (FPendingLoad* Existing = PendingLoads.Find(ObjectId))
	{
		Existing->Callbacks.Add(OnLoaded);
		return;
	}

	// Start new load
	FPendingLoad NewLoad;
	NewLoad.Callbacks.Add(OnLoaded);

	UAssetManager& AM = UAssetManager::Get();
	FStreamableDelegate OnComplete = FStreamableDelegate::CreateUObject(
		const_cast<UObjectDefinitionCache*>(this),
		&UObjectDefinitionCache::OnObjectLoaded,
		ObjectId
	);

	NewLoad.Handle = AM.LoadPrimaryAsset(ObjectId, TArray<FName>(), OnComplete);

	if (NewLoad.Handle.IsValid())
	{
		PendingLoads.Add(ObjectId, MoveTemp(NewLoad));
		UE_LOG(LogObjectDefinitionCache, Verbose, TEXT("Started async load for %s"), *ObjectId.ToString());
	}
	else
	{
		UE_LOG(LogObjectDefinitionCache, Warning, TEXT("Failed to start load for %s"), *ObjectId.ToString());
		OnLoaded.ExecuteIfBound(nullptr);
	}
}

void UObjectDefinitionCache::OnObjectLoaded(FPrimaryAssetId ObjectId)
{
	FPendingLoad PendingLoad;
	if (!PendingLoads.RemoveAndCopyValue(ObjectId, PendingLoad))
	{
		return;
	}

	CaptureResolvedObject(ObjectId, PendingLoad.Handle);
	UObject* LoadedObject = GetLoaded(ObjectId);
	UE_LOG(LogObjectDefinitionCache, Verbose, TEXT("Loaded %s: %s"),
		*ObjectId.ToString(), LoadedObject ? TEXT("Success") : TEXT("Failed"));

	for (FOnObjectDefinitionLoaded& Callback : PendingLoad.Callbacks)
	{
		Callback.ExecuteIfBound(LoadedObject);
	}
}

void UObjectDefinitionCache::Warmup(const TArray<FPrimaryAssetId>& ObjectIds, FOnWarmupComplete OnComplete)
{
	if (ObjectIds.Num() == 0)
	{
		UE_LOG(LogObjectDefinitionCache, Log, TEXT("Warmup: No objects to load"));
		OnComplete.ExecuteIfBound();
		return;
	}

	// Filter to only unloaded objects
	TArray<FPrimaryAssetId> ToLoad;
	for (const FPrimaryAssetId& ObjectId : ObjectIds)
	{
		if (!ObjectId.IsValid())
		{
			continue;
		}

		if (GetLoadState(ObjectId) == EObjectDefinitionLoadState::Loaded)
		{
			continue;
		}

		if (PendingLoads.Contains(ObjectId))
		{
			continue;
		}

		ToLoad.Add(ObjectId);
	}

	if (ToLoad.Num() == 0)
	{
		UE_LOG(LogObjectDefinitionCache, Log, TEXT("Warmup: All %d objects already loaded"), ObjectIds.Num());
		OnComplete.ExecuteIfBound();
		return;
	}

	UE_LOG(LogObjectDefinitionCache, Log, TEXT("Warmup: Loading %d objects (of %d requested)"),
		ToLoad.Num(), ObjectIds.Num());

	UAssetManager& AM = UAssetManager::Get();
	WarmupObjectIds = ToLoad;

	FStreamableDelegate WarmupDelegate = FStreamableDelegate::CreateLambda(
		[this, OnComplete, NumObjects = ToLoad.Num()]()
		{
			for (const FPrimaryAssetId& ObjectId : WarmupObjectIds)
			{
				CaptureResolvedObject(ObjectId, WarmupHandle);
			}

			WarmupObjectIds.Reset();
			UE_LOG(LogObjectDefinitionCache, Log, TEXT("Warmup complete: %d objects loaded"), NumObjects);
			WarmupHandle.Reset();
			OnComplete.ExecuteIfBound();
		}
	);

	WarmupHandle = AM.LoadPrimaryAssets(ToLoad, TArray<FName>(), WarmupDelegate);

	if (!WarmupHandle.IsValid())
	{
		WarmupObjectIds.Reset();
		UE_LOG(LogObjectDefinitionCache, Warning, TEXT("Warmup: Failed to start batch load"));
		OnComplete.ExecuteIfBound();
	}
}

void UObjectDefinitionCache::GetDiagnostics(TArray<FObjectDefinitionCacheEntryDiagnostic>& OutDiagnostics) const
{
	TSet<FPrimaryAssetId> KnownIds;
	for (const TPair<FPrimaryAssetId, TObjectPtr<UObject>>& Pair : ResolvedDefinitions)
	{
		KnownIds.Add(Pair.Key);
	}
	for (const TPair<FPrimaryAssetId, TSharedPtr<FStreamableHandle>>& Pair : ResidentHandles)
	{
		KnownIds.Add(Pair.Key);
	}
	for (const TPair<FPrimaryAssetId, FPendingLoad>& Pair : PendingLoads)
	{
		KnownIds.Add(Pair.Key);
	}

	TArray<FPrimaryAssetId> SortedIds = KnownIds.Array();
	SortedIds.Sort([](const FPrimaryAssetId& A, const FPrimaryAssetId& B)
	{
		return A.ToString() < B.ToString();
	});

	OutDiagnostics.Reset();
	OutDiagnostics.Reserve(SortedIds.Num());

	for (const FPrimaryAssetId& ObjectId : SortedIds)
	{
		FObjectDefinitionCacheEntryDiagnostic Entry;
		Entry.ObjectId = ObjectId;
		Entry.State = GetLoadState(ObjectId);
		Entry.bHasResolvedObject = ResolvedDefinitions.Contains(ObjectId);
		Entry.bHasResidentHandle = ResidentHandles.Contains(ObjectId);
		Entry.bHasPendingLoad = PendingLoads.Contains(ObjectId);
		OutDiagnostics.Add(Entry);
	}
}
