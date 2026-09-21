// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"

/**
 * Generic provider-module lifecycle for class registries.
 *
 * Provider modules self-register during StartupModule. Domain registries use
 * the resulting set only after the engine has completed the Default plugin
 * loading phase, so discovery never depends on a consumer-owned module list.
 */
class PROJECTCORE_API FRegisteredClassProviderRegistry
{
public:
	static void RegisterProviderModule(FPrimaryAssetType AssetType, FName ModuleName);

	static bool AreProviderModulesReady(
		FPrimaryAssetType AssetType,
		FString* OutReason = nullptr);

	static TArray<FName> GetProviderModules(FPrimaryAssetType AssetType);

	static uint32 GetGeneration(FPrimaryAssetType AssetType);
};
