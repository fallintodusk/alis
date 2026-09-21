// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "CapabilityRegistry.h"
#include "Registry/RegisteredClassScan.h"
#include "Registry/RegisteredClassProviderRegistry.h"
#include "Components/ActorComponent.h"
#include "ProjectObjectCapabilitiesModule.h"

TMap<FName, UClass*> FCapabilityRegistry::Registry;
bool FCapabilityRegistry::bIsBuilt = false;
uint32 FCapabilityRegistry::BuiltProviderGeneration = 0;

// Motion capability IDs (meshes with these need Movable mobility)
static const TArray<FName> MotionCapabilityIds = { FName(TEXT("Hinged")), FName(TEXT("Sliding")) };

UClass* FCapabilityRegistry::GetCapabilityClass(FName CapabilityId)
{
	if (!EnsureBuilt())
	{
		return nullptr;
	}

	if (UClass** Found = Registry.Find(CapabilityId))
	{
		return *Found;
	}
	return nullptr;
}

bool FCapabilityRegistry::HasCapability(FName CapabilityId)
{
	if (!EnsureBuilt())
	{
		return false;
	}
	return Registry.Contains(CapabilityId);
}

bool FCapabilityRegistry::IsReady(FString* OutReason)
{
	return FRegisteredClassProviderRegistry::AreProviderModulesReady(
		FPrimaryAssetType(TEXT("CapabilityComponent")),
		OutReason);
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
	if (!EnsureBuilt())
	{
		return;
	}
	for (const auto& Pair : Registry)
	{
		Func(Pair.Key, Pair.Value);
	}
}

void FCapabilityRegistry::DumpToLog()
{
	if (!EnsureBuilt())
	{
		return;
	}
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
	if (!EnsureBuilt())
	{
		return 0;
	}
	return Registry.Num();
}

bool FCapabilityRegistry::EnsureBuilt()
{
	check(IsInGameThread());
	const FPrimaryAssetType AssetType(TEXT("CapabilityComponent"));
	const uint32 ProviderGeneration =
		FRegisteredClassProviderRegistry::GetGeneration(AssetType);
	if (!bIsBuilt || BuiltProviderGeneration != ProviderGeneration)
	{
		return Build();
	}
	return true;
}

bool FCapabilityRegistry::Build()
{
	Registry.Empty();

	FString ReadinessError;
	if (!IsReady(&ReadinessError))
	{
		bIsBuilt = false;
		UE_LOG(LogProjectObjectCapabilities, Error,
			TEXT("CapabilityRegistry is not ready: %s"),
			*ReadinessError);
		return false;
	}

	FRegistryScanConfig Config;
	Config.AssetType = FPrimaryAssetType(TEXT("CapabilityComponent"));
	Config.RequiredModules =
		FRegisteredClassProviderRegistry::GetProviderModules(Config.AssetType);
	Config.RequiredBaseClass = UActorComponent::StaticClass();
	Config.DomainName = TEXT("CapabilityRegistry");

	FRegisteredClassScan::ScanByPrimaryAssetId(Config, Registry);

	bIsBuilt = true;
	BuiltProviderGeneration =
		FRegisteredClassProviderRegistry::GetGeneration(Config.AssetType);

	UE_LOG(LogProjectObjectCapabilities, Log,
		TEXT("CapabilityRegistry built: %d capabilities registered"),
		Registry.Num());
	return true;
}
