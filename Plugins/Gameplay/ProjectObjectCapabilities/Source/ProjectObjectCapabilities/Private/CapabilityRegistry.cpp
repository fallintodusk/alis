// Copyright ALIS. All Rights Reserved.

#include "CapabilityRegistry.h"
#include "Registry/RegisteredClassScan.h"
#include "Components/ActorComponent.h"
#include "ProjectObjectCapabilitiesModule.h"

TMap<FName, UClass*> FCapabilityRegistry::Registry;
TArray<FName> FCapabilityRegistry::ExternalCapabilityModules;
bool FCapabilityRegistry::bIsBuilt = false;

// Motion capability IDs (meshes with these need Movable mobility)
static const TArray<FName> MotionCapabilityIds = { FName(TEXT("Hinged")), FName(TEXT("Sliding")) };

void FCapabilityRegistry::RegisterCapabilityModule(FName ModuleName)
{
	check(IsInGameThread());

	if (!ExternalCapabilityModules.Contains(ModuleName))
	{
		ExternalCapabilityModules.Add(ModuleName);

		// Auto-invalidate so next lookup rescans with the new module
		if (bIsBuilt)
		{
			bIsBuilt = false;
			UE_LOG(LogProjectObjectCapabilities, Log,
				TEXT("CapabilityRegistry: registered external module '%s' (late registration, will rescan on next lookup)"),
				*ModuleName.ToString());
		}
		else
		{
			UE_LOG(LogProjectObjectCapabilities, Log,
				TEXT("CapabilityRegistry: registered external module '%s'"),
				*ModuleName.ToString());
		}
	}
}

UClass* FCapabilityRegistry::GetCapabilityClass(FName CapabilityId)
{
	EnsureBuilt();

	if (UClass** Found = Registry.Find(CapabilityId))
	{
		return *Found;
	}
	return nullptr;
}

bool FCapabilityRegistry::HasCapability(FName CapabilityId)
{
	EnsureBuilt();
	return Registry.Contains(CapabilityId);
}

void FCapabilityRegistry::RebuildRegistry()
{
	bIsBuilt = false;
	Registry.Empty();
	Build();
}

bool FCapabilityRegistry::IsMotionCapability(FName CapabilityId)
{
	return MotionCapabilityIds.Contains(CapabilityId);
}

void FCapabilityRegistry::ForEach(TFunctionRef<void(FName Id, UClass* Class)> Func)
{
	EnsureBuilt();
	for (const auto& Pair : Registry)
	{
		Func(Pair.Key, Pair.Value);
	}
}

void FCapabilityRegistry::DumpToLog()
{
	EnsureBuilt();
	UE_LOG(LogProjectObjectCapabilities, Log,
		TEXT("=== CapabilityRegistry dump (%d entries) ==="), Registry.Num());
	for (const auto& Pair : Registry)
	{
		UE_LOG(LogProjectObjectCapabilities, Log,
			TEXT("  %s -> %s"), *Pair.Key.ToString(), *Pair.Value->GetPathName());
	}
}

int32 FCapabilityRegistry::Num()
{
	EnsureBuilt();
	return Registry.Num();
}

void FCapabilityRegistry::EnsureBuilt()
{
	check(IsInGameThread());
	if (!bIsBuilt)
	{
		Build();
	}
}

void FCapabilityRegistry::Build()
{
	Registry.Empty();

	FRegistryScanConfig Config;
	Config.AssetType = FPrimaryAssetType(TEXT("CapabilityComponent"));
	// TObjectIterator only sees classes from loaded DLLs.
	// Preload core capability modules so the single scan discovers all
	// capability classes. These are always-enabled plugins that provide
	// built-in capabilities. No compile-time dependency -- just module name
	// strings for the kernel's preload step.
	// External optional plugins self-register via RegisterCapabilityModule().
	Config.RequiredModules = {
		FName(TEXT("ProjectObjectCapabilities")),
		FName(TEXT("ProjectMotionSystem")),
		FName(TEXT("ProjectSkeletalAssembly"))
	};
	Config.RequiredModules.Append(ExternalCapabilityModules);
	Config.RequiredBaseClass = UActorComponent::StaticClass();
	Config.DomainName = TEXT("CapabilityRegistry");

	FRegisteredClassScan::ScanByPrimaryAssetId(Config, Registry);

	bIsBuilt = true;

	UE_LOG(LogProjectObjectCapabilities, Log,
		TEXT("CapabilityRegistry built: %d capabilities registered"),
		Registry.Num());
}
