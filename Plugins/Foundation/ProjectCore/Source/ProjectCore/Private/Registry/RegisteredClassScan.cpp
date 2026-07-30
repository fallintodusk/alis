// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Registry/RegisteredClassScan.h"
#include "ProjectLogging.h"
#include "UObject/UObjectIterator.h"

void FRegisteredClassScan::EnsureModulesLoaded(const TArray<FName>& Modules, const FString& DomainName)
{
	for (const FName& ModuleName : Modules)
	{
		if (FModuleManager::Get().IsModuleLoaded(ModuleName))
		{
			continue;
		}

		FModuleManager::Get().LoadModule(ModuleName);

		if (FModuleManager::Get().IsModuleLoaded(ModuleName))
		{
			UE_LOG(LogProjectCore, Verbose,
				TEXT("[%s] Loaded required module: %s"),
				*DomainName, *ModuleName.ToString());
		}
		else
		{
			UE_LOG(LogProjectCore, Warning,
				TEXT("[%s] Failed to load required module: %s. Some entries may be missing."),
				*DomainName, *ModuleName.ToString());
		}
	}
}

int32 FRegisteredClassScan::ScanByPrimaryAssetId(const FRegistryScanConfig& Config, TMap<FName, UClass*>& OutMap)
{
	check(IsInGameThread());

	if (!Config.AssetType.IsValid())
	{
		UE_LOG(LogProjectCore, Warning,
			TEXT("[%s] ScanByPrimaryAssetId called with empty AssetType. Returning 0."),
			*Config.DomainName);
		return 0;
	}

	// Modules must be loaded before TObjectIterator can discover their classes
	EnsureModulesLoaded(Config.RequiredModules, Config.DomainName);

	int32 ScannedClasses = 0;
	int32 DuplicatesSkipped = 0;

	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;

		if (Class->HasAnyClassFlags(CLASS_Abstract))
		{
			continue;
		}

		if (Config.RequiredBaseClass && !Class->IsChildOf(Config.RequiredBaseClass))
		{
			continue;
		}

		// true = create CDO if not yet visited, so scan results are deterministic
		// regardless of class load order
		UObject* CDO = Class->GetDefaultObject(true);
		if (!CDO)
		{
			continue;
		}

		FPrimaryAssetId AssetId = CDO->GetPrimaryAssetId();
		if (!AssetId.IsValid())
		{
			continue;
		}

		if (AssetId.PrimaryAssetType != Config.AssetType)
		{
			continue;
		}

		const FName EntryId = AssetId.PrimaryAssetName;

		// Duplicate detection: non-fatal authoring error. Keeps first entry,
		// skips duplicate. Fires ensure in non-shipping builds so the callstack
		// is captured, but does not crash. TObjectIterator order is not a stable
		// contract, so duplicates must be resolved at authoring time.
		if (UClass** Existing = OutMap.Find(EntryId))
		{
			UE_LOG(LogProjectCore, Error,
				TEXT("[%s] Duplicate ID '%s': %s conflicts with existing %s. Keeping first, skipping duplicate."),
				*Config.DomainName,
				*EntryId.ToString(),
				*Class->GetPathName(),
				*(*Existing)->GetPathName());

#if !UE_BUILD_SHIPPING
			ensureMsgf(false,
				TEXT("[%s] Duplicate registry ID '%s'. Fix the authoring conflict."),
				*Config.DomainName, *EntryId.ToString());
#endif

			++DuplicatesSkipped;
			continue;
		}

		OutMap.Add(EntryId, Class);
		++ScannedClasses;

		UE_LOG(LogProjectCore, Verbose,
			TEXT("[%s] Registered '%s' -> %s"),
			*Config.DomainName,
			*EntryId.ToString(),
			*Class->GetName());
	}

	// Scan summary
	if (DuplicatesSkipped > 0)
	{
		UE_LOG(LogProjectCore, Warning,
			TEXT("[%s] Scan complete: %d registered, %d duplicates skipped."),
			*Config.DomainName, ScannedClasses, DuplicatesSkipped);
	}
	else
	{
		UE_LOG(LogProjectCore, Log,
			TEXT("[%s] Scan complete: %d registered."),
			*Config.DomainName, ScannedClasses);
	}

	return ScannedClasses;
}
