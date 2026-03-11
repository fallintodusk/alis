// Copyright Fall.is. All Rights Reserved.

#include "Interfaces/IFeatureModuleRegistry.h"
#include "Interfaces/IProjectFeatureModule.h"
#include "ProjectLogging.h"

// Global registry storage (module name -> interface pointer)
static TMap<FName, IProjectFeatureModule*> GFeatureModuleRegistry;

void RegisterFeatureModule(FName ModuleName, IProjectFeatureModule* FeatureModule)
{
	if (FeatureModule == nullptr)
	{
		UE_LOG(LogProjectCore, Warning, TEXT("RegisterFeatureModule: Attempted to register null module: %s"), *ModuleName.ToString());
		return;
	}

	if (GFeatureModuleRegistry.Contains(ModuleName))
	{
		UE_LOG(LogProjectCore, Warning, TEXT("RegisterFeatureModule: Module '%s' already registered, replacing"), *ModuleName.ToString());
	}

	GFeatureModuleRegistry.Add(ModuleName, FeatureModule);
	UE_LOG(LogProjectCore, Log, TEXT("RegisterFeatureModule: Registered '%s' (total: %d)"), *ModuleName.ToString(), GFeatureModuleRegistry.Num());
}

void UnregisterFeatureModule(FName ModuleName)
{
	if (GFeatureModuleRegistry.Remove(ModuleName) > 0)
	{
		UE_LOG(LogProjectCore, Log, TEXT("UnregisterFeatureModule: Unregistered '%s' (remaining: %d)"), *ModuleName.ToString(), GFeatureModuleRegistry.Num());
	}
}

const TMap<FName, IProjectFeatureModule*>& GetFeatureModuleRegistry()
{
	return GFeatureModuleRegistry;
}

bool IsFeatureModuleRegistered(FName ModuleName)
{
	return GFeatureModuleRegistry.Contains(ModuleName);
}

IProjectFeatureModule* GetFeatureModule(FName ModuleName)
{
	IProjectFeatureModule** Found = GFeatureModuleRegistry.Find(ModuleName);
	return Found ? *Found : nullptr;
}
