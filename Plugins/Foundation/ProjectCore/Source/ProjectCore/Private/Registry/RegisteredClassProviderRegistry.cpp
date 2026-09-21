// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Registry/RegisteredClassProviderRegistry.h"

#include "Interfaces/IPluginManager.h"
#include "Modules/ModuleManager.h"
#include "ProjectLogging.h"

namespace
{
	struct FProviderSet
	{
		TArray<FName> Modules;
		uint32 Generation = 0;
	};

	TMap<FPrimaryAssetType, FProviderSet> ProviderSets;

	void SetReason(FString* OutReason, const FString& Reason)
	{
		if (OutReason)
		{
			*OutReason = Reason;
		}
	}
}

void FRegisteredClassProviderRegistry::RegisterProviderModule(
	const FPrimaryAssetType AssetType,
	const FName ModuleName)
{
	check(IsInGameThread());
	check(AssetType.IsValid());
	check(!ModuleName.IsNone());

	FProviderSet& Set = ProviderSets.FindOrAdd(AssetType);
	if (!Set.Modules.Contains(ModuleName))
	{
		Set.Modules.Add(ModuleName);
		++Set.Generation;
		UE_LOG(LogProjectCore, Verbose,
			TEXT("Registered class provider '%s' for asset type '%s'"),
			*ModuleName.ToString(),
			*AssetType.ToString());
	}
}

bool FRegisteredClassProviderRegistry::AreProviderModulesReady(
	const FPrimaryAssetType AssetType,
	FString* OutReason)
{
	check(IsInGameThread());

	if (!AssetType.IsValid())
	{
		SetReason(OutReason, TEXT("Registry asset type is empty"));
		return false;
	}

	if (IPluginManager::Get().GetLastCompletedLoadingPhase() < ELoadingPhase::Default)
	{
		SetReason(OutReason, FString::Printf(
			TEXT("Enabled plugin modules have not completed the Default loading phase for '%s'"),
			*AssetType.ToString()));
		return false;
	}

	const FProviderSet* Set = ProviderSets.Find(AssetType);
	if (!Set || Set->Modules.IsEmpty())
	{
		SetReason(OutReason, FString::Printf(
			TEXT("No provider modules registered for '%s'"),
			*AssetType.ToString()));
		return false;
	}

	for (const FName ModuleName : Set->Modules)
	{
		if (!FModuleManager::Get().IsModuleLoaded(ModuleName))
		{
			SetReason(OutReason, FString::Printf(
				TEXT("Registered provider module '%s' is not loaded for '%s'"),
				*ModuleName.ToString(),
				*AssetType.ToString()));
			return false;
		}
	}

	SetReason(OutReason, FString());
	return true;
}

TArray<FName> FRegisteredClassProviderRegistry::GetProviderModules(
	const FPrimaryAssetType AssetType)
{
	check(IsInGameThread());
	if (const FProviderSet* Set = ProviderSets.Find(AssetType))
	{
		return Set->Modules;
	}
	return {};
}

uint32 FRegisteredClassProviderRegistry::GetGeneration(
	const FPrimaryAssetType AssetType)
{
	check(IsInGameThread());
	if (const FProviderSet* Set = ProviderSets.Find(AssetType))
	{
		return Set->Generation;
	}
	return 0;
}
