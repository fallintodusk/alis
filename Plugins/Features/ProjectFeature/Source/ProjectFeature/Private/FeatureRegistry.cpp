// Copyright ALIS. All Rights Reserved.

#include "FeatureRegistry.h"

DEFINE_LOG_CATEGORY_STATIC(LogFeatureRegistry, Log, All);

// Static member definitions
FCriticalSection FFeatureRegistry::RegistryLock;
TMap<FName, FFeatureRegistry::FFeatureInitFunction> FFeatureRegistry::Registry;

void FFeatureRegistry::RegisterFeature(FName FeatureName, FFeatureInitFunction InitFn)
{
	FScopeLock Lock(&RegistryLock);

	if (Registry.Contains(FeatureName))
	{
		UE_LOG(LogFeatureRegistry, Warning,
			TEXT("Feature '%s' already registered, overwriting"),
			*FeatureName.ToString());
	}

	Registry.Add(FeatureName, MoveTemp(InitFn));

	UE_LOG(LogFeatureRegistry, Log,
		TEXT("Registered feature: %s (total: %d)"),
		*FeatureName.ToString(), Registry.Num());
}

bool FFeatureRegistry::InitializeFeature(FName FeatureName, const FFeatureInitContext& Context)
{
	FFeatureInitFunction InitFn;

	// Lock only for lookup, release before calling user code
	{
		FScopeLock Lock(&RegistryLock);

		const FFeatureInitFunction* Found = Registry.Find(FeatureName);
		if (!Found)
		{
			UE_LOG(LogFeatureRegistry, Warning,
				TEXT("InitializeFeature: Feature '%s' not registered"),
				*FeatureName.ToString());
			return false;
		}

		InitFn = *Found;
	}

	// Call init function outside the lock
	UE_LOG(LogFeatureRegistry, Log,
		TEXT("Initializing feature: %s (Mode: %s)"),
		*FeatureName.ToString(), *Context.ModeName.ToString());

	InitFn(Context);

	return true;
}

bool FFeatureRegistry::IsFeatureRegistered(FName FeatureName)
{
	FScopeLock Lock(&RegistryLock);
	return Registry.Contains(FeatureName);
}

TArray<FName> FFeatureRegistry::GetRegisteredFeatures()
{
	FScopeLock Lock(&RegistryLock);

	TArray<FName> Features;
	Registry.GetKeys(Features);
	return Features;
}
