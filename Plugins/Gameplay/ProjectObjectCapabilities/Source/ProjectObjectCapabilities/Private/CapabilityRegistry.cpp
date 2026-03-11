// Copyright ALIS. All Rights Reserved.

#include "CapabilityRegistry.h"
#include "Components/ActorComponent.h"
#include "UObject/UObjectIterator.h"
#include "ProjectObjectCapabilitiesModule.h"

TMap<FName, UClass*> FCapabilityRegistry::Registry;
bool FCapabilityRegistry::bIsBuilt = false;

// Motion capability IDs (meshes with these need Movable mobility)
static const TArray<FName> MotionCapabilityIds = { FName(TEXT("Hinged")), FName(TEXT("Sliding")) };

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

void FCapabilityRegistry::EnsureBuilt()
{
	if (!bIsBuilt)
	{
		Build();
	}
}

void FCapabilityRegistry::Build()
{
	Registry.Empty();

	// Ensure capability modules are loaded before CDO scan
	// TObjectIterator only sees loaded classes
	FModuleManager::Get().LoadModule(TEXT("ProjectObjectCapabilities"));
	FModuleManager::Get().LoadModule(TEXT("ProjectMotionSystem"));

	const FPrimaryAssetType CapabilityType(TEXT("CapabilityComponent"));

	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;

		// Skip abstract classes and non-components
		if (Class->HasAnyClassFlags(CLASS_Abstract) || !Class->IsChildOf(UActorComponent::StaticClass()))
		{
			continue;
		}

		// Get CDO and check for CapabilityComponent asset type
		UActorComponent* CDO = Class->GetDefaultObject<UActorComponent>();
		if (!CDO)
		{
			continue;
		}

		FPrimaryAssetId AssetId = CDO->GetPrimaryAssetId();
		if (!AssetId.IsValid() || AssetId.PrimaryAssetType != CapabilityType)
		{
			continue;
		}

		// Duplicate ID detection
		if (Registry.Contains(AssetId.PrimaryAssetName))
		{
			UE_LOG(LogProjectObjectCapabilities, Error,
				TEXT("Duplicate CapabilityId '%s' - %s conflicts with %s"),
				*AssetId.PrimaryAssetName.ToString(),
				*Class->GetName(),
				*Registry[AssetId.PrimaryAssetName]->GetName());
			continue; // Keep first, skip duplicate
		}

		Registry.Add(AssetId.PrimaryAssetName, Class);

		UE_LOG(LogProjectObjectCapabilities, Log,
			TEXT("CapabilityRegistry: registered '%s' -> %s"),
			*AssetId.PrimaryAssetName.ToString(),
			*Class->GetName());
	}

	bIsBuilt = true;

	UE_LOG(LogProjectObjectCapabilities, Log,
		TEXT("CapabilityRegistry built: %d capabilities registered"),
		Registry.Num());
}
