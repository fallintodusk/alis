// Copyright ALIS. All Rights Reserved.

#include "Services/ObjectDefinitionCache.h"
#include "Engine/AssetManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogObjectDefinitionCache, Log, All);

UObject* UObjectDefinitionCache::GetLoaded(FPrimaryAssetId ObjectId) const
{
	if (!ObjectId.IsValid())
	{
		return nullptr;
	}

	UAssetManager& AM = UAssetManager::Get();
	return AM.GetPrimaryAssetObject<UObject>(ObjectId);
}

bool UObjectDefinitionCache::IsLoaded(FPrimaryAssetId ObjectId) const
{
	return GetLoaded(ObjectId) != nullptr;
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
		if (ObjectId.IsValid() && !IsLoaded(ObjectId))
		{
			ToLoad.Add(ObjectId);
		}
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

	FStreamableDelegate WarmupDelegate = FStreamableDelegate::CreateLambda(
		[this, OnComplete, NumObjects = ToLoad.Num()]()
		{
			UE_LOG(LogObjectDefinitionCache, Log, TEXT("Warmup complete: %d objects loaded"), NumObjects);
			WarmupHandle.Reset();
			OnComplete.ExecuteIfBound();
		}
	);

	WarmupHandle = AM.LoadPrimaryAssets(ToLoad, TArray<FName>(), WarmupDelegate);

	if (!WarmupHandle.IsValid())
	{
		UE_LOG(LogObjectDefinitionCache, Warning, TEXT("Warmup: Failed to start batch load"));
		OnComplete.ExecuteIfBound();
	}
}
