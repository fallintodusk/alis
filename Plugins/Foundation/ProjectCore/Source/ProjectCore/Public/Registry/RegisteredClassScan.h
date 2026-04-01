// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"
#include "Modules/ModuleManager.h"

/**
 * Configuration for a registry class scan.
 *
 * Each domain registry (capabilities, skeletal features, etc.) provides
 * its own config to FRegisteredClassScan. The kernel uses this to filter
 * the CDO scan without knowing anything about the domain.
 */
struct PROJECTCORE_API FRegistryScanConfig
{
	/** PrimaryAssetType to match during CDO scan (e.g. "CapabilityComponent", "SkeletalFeature"). */
	FPrimaryAssetType AssetType;

	/** Modules to force-load before scanning so TObjectIterator sees their classes. */
	TArray<FName> RequiredModules;

	/** Required base class for scanned types (e.g. UActorComponent::StaticClass()). nullptr = no filter. */
	UClass* RequiredBaseClass = nullptr;

	/** Human-readable domain name for log messages. */
	FString DomainName;
};

/**
 * Shared registry mechanics kernel.
 *
 * Provides the CDO-scan-based class discovery that domain registries
 * (FCapabilityRegistry, etc.) delegate to internally.
 *
 * This is NOT a registry itself -- it has no storage, no lazy-build state,
 * no domain knowledge. It is a stateless utility that domain registries call
 * from their Build() methods.
 *
 * ## Responsibilities (kernel owns)
 *
 * - Module preloading before scan
 * - TObjectIterator class iteration
 * - Base class and PrimaryAssetType filtering
 * - GetPrimaryAssetId() extraction
 * - Duplicate ID detection with structured error logging
 * - Scan summary logging
 *
 * ## Responsibilities (domain registry owns)
 *
 * - Static TMap storage and bIsBuilt flag
 * - Public lookup API (FindClass, Contains, etc.)
 * - Lazy-build lifecycle (EnsureBuilt)
 * - Domain-specific helpers (e.g. IsMotionCapability)
 * - Domain-specific log category
 *
 * ## Usage
 *
 * ```cpp
 * void FMyDomainRegistry::Build()
 * {
 *     Registry.Empty();
 *     FRegistryScanConfig Config;
 *     Config.AssetType = FPrimaryAssetType("MyAssetType");
 *     Config.RequiredModules = { "MyModule" };
 *     Config.RequiredBaseClass = UActorComponent::StaticClass();
 *     Config.DomainName = TEXT("MyDomainRegistry");
 *     FRegisteredClassScan::ScanByPrimaryAssetId(Config, Registry);
 *     bIsBuilt = true;
 * }
 * ```
 *
 * ## Architecture
 *
 * Lives in ProjectCore (Foundation tier). Domain registries live in their
 * own plugins and depend only on ProjectCore for the kernel.
 * No domain registry depends on another domain registry.
 *
 * ## Threading
 *
 * All methods must be called from the game thread. TObjectIterator is not
 * thread-safe, and module loading must happen on the game thread.
 * ScanByPrimaryAssetId asserts check(IsInGameThread()).
 */
class PROJECTCORE_API FRegisteredClassScan
{
public:
	/**
	 * Force-load modules so TObjectIterator sees their classes.
	 *
	 * Call this before ScanByPrimaryAssetId if you need fine-grained control.
	 * ScanByPrimaryAssetId calls this internally, so direct use is optional.
	 *
	 * @param Modules Module names to load.
	 * @param DomainName Domain name for log context.
	 */
	static void EnsureModulesLoaded(const TArray<FName>& Modules, const FString& DomainName);

	/**
	 * Scan loaded classes by PrimaryAssetId and populate a registry map.
	 *
	 * Iterates all loaded UClass objects, filters by RequiredBaseClass and
	 * AssetType, extracts PrimaryAssetName as the registry key, and adds
	 * matching classes to OutMap.
	 *
	 * Handles:
	 * - Abstract class exclusion
	 * - Null CDO safety
	 * - Duplicate ID detection (keeps first, logs error for duplicates)
	 * - Per-entry registration logging (Verbose level)
	 * - Scan summary logging (Log level)
	 *
	 * @param Config Scan configuration (asset type, modules, base class, domain name).
	 * @param OutMap Map to populate. Caller should Empty() before calling if needed.
	 * @return Number of entries registered.
	 */
	static int32 ScanByPrimaryAssetId(const FRegistryScanConfig& Config, TMap<FName, UClass*>& OutMap);
};
