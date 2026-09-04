// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Experience/ProjectExperienceDefinitionRegistrar.h"

#include "ProjectLoadingLog.h"
#include "Engine/AssetManager.h"
#include "UObject/ObjectKey.h"
#include "Engine/StreamableManager.h"
#include "Experience/ProjectExperienceDefinition.h"
#include "Experience/ProjectExperienceDefinitionDescriptor.h"
#include "Experience/ProjectExperienceRegistry.h"

namespace
{
	/**
	 * Primary asset type for configured experiences. The directories carrying these assets
	 * are declared once in project config (PrimaryAssetTypesToScan), so no plugin content
	 * path is hardcoded in this generic owner.
	 */
	const FPrimaryAssetType ExperienceDefinitionType(TEXT("ExperienceDefinition"));

	/**
	 * Registries already populated, not a single global flag.
	 *
	 * Discovery is idempotent per registry, not per process: a global flag would let the first
	 * registry that ran discovery silently suppress it for every other registry, leaving that
	 * registry permanently empty. Production has one registry, but the guard must describe what
	 * the operation actually guarantees.
	 */
	TSet<FObjectKey> PopulatedRegistries;
}

int32 ProjectExperienceDefinitions::EnsureRegistered(UProjectExperienceRegistry& Registry)
{
	const FObjectKey RegistryKey(&Registry);
	if (PopulatedRegistries.Contains(RegistryKey))
	{
		return 0;
	}

	if (!UAssetManager::IsInitialized())
	{
		// Called too early. Stay silent-but-observable and allow a later retry rather than
		// latching a permanent empty result.
		UE_LOG(LogProjectLoading, Warning,
			TEXT("ExperienceDefinitions: AssetManager not initialized; deferring definition discovery."));
		return 0;
	}

	UAssetManager& AssetManager = UAssetManager::Get();

	TArray<FPrimaryAssetId> DefinitionIds;
	AssetManager.GetPrimaryAssetIdList(ExperienceDefinitionType, DefinitionIds);

	if (DefinitionIds.Num() == 0)
	{
		UE_LOG(LogProjectLoading, Warning,
			TEXT("ExperienceDefinitions: No '%s' assets discovered. Check PrimaryAssetTypesToScan."),
			*ExperienceDefinitionType.ToString());
		PopulatedRegistries.Add(RegistryKey);
		return 0;
	}

	// Synchronous: the caller is resolving a missing descriptor right now, and these records
	// are a handful of tiny data assets. A null handle means nothing needed streaming.
	if (const TSharedPtr<FStreamableHandle> Handle = AssetManager.LoadPrimaryAssets(DefinitionIds))
	{
		Handle->WaitUntilComplete();
	}

	int32 RegisteredCount = 0;
	TSet<FName> SeenExperienceNames;

	for (const FPrimaryAssetId& DefinitionId : DefinitionIds)
	{
		const UProjectExperienceDefinition* Definition =
			Cast<UProjectExperienceDefinition>(AssetManager.GetPrimaryAssetObject(DefinitionId));
		if (Definition == nullptr)
		{
			UE_LOG(LogProjectLoading, Error,
				TEXT("ExperienceDefinitions: '%s' failed to load; skipping."), *DefinitionId.ToString());
			continue;
		}

		// Defence in depth only. The authority for id uniqueness is generation time:
		// UDefinitionGeneratorSubsystem rejects two source records sharing an id, because the
		// id names the asset and becomes its FPrimaryAssetId. If a duplicate still reaches
		// here, skip it loudly rather than letting enumeration order decide silently.
		if (SeenExperienceNames.Contains(Definition->ExperienceName))
		{
			UE_LOG(LogProjectLoading, Error,
				TEXT("ExperienceDefinitions: Duplicate experience id '%s' from '%s'; skipping."),
				*Definition->ExperienceName.ToString(), *DefinitionId.ToString());
			continue;
		}

		// Something already owns this id - a plugin-owned CDO descriptor, or an earlier
		// discovery pass. The existing owner wins either way: configured data never silently
		// replaces compiled behavior, and rediscovery must not churn the registry.
		if (Registry.FindDescriptor(Definition->ExperienceName) != nullptr)
		{
			UE_LOG(LogProjectLoading, Verbose,
				TEXT("ExperienceDefinitions: '%s' is already registered; keeping the existing descriptor."),
				*Definition->ExperienceName.ToString());
			SeenExperienceNames.Add(Definition->ExperienceName);
			continue;
		}

		UProjectExperienceDefinitionDescriptor* Descriptor =
			NewObject<UProjectExperienceDefinitionDescriptor>(GetTransientPackage());
		if (Descriptor == nullptr || !Descriptor->InitializeFromDefinition(*Definition))
		{
			UE_LOG(LogProjectLoading, Error,
				TEXT("ExperienceDefinitions: '%s' is malformed (missing id or map); rejected."),
				*DefinitionId.ToString());
			continue;
		}

		Registry.RegisterDescriptor(Descriptor);
		SeenExperienceNames.Add(Definition->ExperienceName);
		++RegisteredCount;

		UE_LOG(LogProjectLoading, Display,
			TEXT("ExperienceDefinitions: Registered configured experience '%s'."),
			*Definition->ExperienceName.ToString());
	}

	PopulatedRegistries.Add(RegistryKey);
	return RegisteredCount;
}

void ProjectExperienceDefinitions::ResetForTests()
{
	PopulatedRegistries.Reset();
}
