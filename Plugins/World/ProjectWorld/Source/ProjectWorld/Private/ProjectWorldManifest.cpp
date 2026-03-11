#include "ProjectWorldManifest.h"

#include "Engine/AssetManager.h"
#include "Misc/StringBuilder.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectWorld, Log, All);

namespace
{
	static const FPrimaryAssetType WorldAssetType(TEXT("ProjectWorld"));
}

UProjectWorldManifest::UProjectWorldManifest() = default;

void UProjectWorldManifest::PostLoad()
{
	Super::PostLoad();

	// VeryVerbose logging for debugging manifest loading and property access
	if (UE_LOG_ACTIVE(LogProjectWorld, VeryVerbose))
	{
		UE_LOG(LogProjectWorld, VeryVerbose, TEXT("=== WorldManifest Loaded: %s ==="), *GetName());
		UE_LOG(LogProjectWorld, VeryVerbose, TEXT("  WorldRoot: %s"), *WorldRoot.ToString());
		UE_LOG(LogProjectWorld, VeryVerbose, TEXT("  Description: %s"), *Description.ToString());
		UE_LOG(LogProjectWorld, VeryVerbose, TEXT("  Regions: %d"), Regions.Num());

		for (int32 i = 0; i < Regions.Num(); ++i)
		{
			const FProjectWorldRegionDescriptor& Region = Regions[i];
			UE_LOG(LogProjectWorld, VeryVerbose, TEXT("    [%d] RegionId='%s', LayerType=%d, AssetCount=%d"),
				i, *Region.RegionId.ToString(), static_cast<int32>(Region.LayerType), Region.SupportingAssets.Num());
		}

		UE_LOG(LogProjectWorld, VeryVerbose, TEXT("  PartitionMetadata: %s"),
			PartitionMetadata.IsNull() ? TEXT("<None>") : *PartitionMetadata.ToString());
	}
}

FPrimaryAssetId UProjectWorldManifest::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(WorldAssetType, GetFName());
}

bool UProjectWorldManifest::Validate(TArray<FText>& OutErrors) const
{
	bool bIsValid = true;

	// 1. Validate WorldRoot is not null
	if (WorldRoot.IsNull())
	{
		OutErrors.Add(FText::FromString(TEXT("WorldRoot is null - A valid world asset must be specified")));
		bIsValid = false;
	}

	// 2. Validate Description is not empty (warning, not error)
	if (Description.IsEmpty())
	{
		OutErrors.Add(FText::FromString(TEXT("Warning: Description is empty - Recommended for UI and telemetry")));
		// Don't mark as invalid - this is just a warning
	}

	// 3. Validate region IDs are unique
	TSet<FName> UniqueRegionIds;
	for (int32 i = 0; i < Regions.Num(); ++i)
	{
		const FProjectWorldRegionDescriptor& Region = Regions[i];

		if (Region.RegionId == NAME_None)
		{
			OutErrors.Add(FText::FromString(FString::Printf(
				TEXT("Region at index %d has NAME_None as RegionId - Must be a valid identifier"), i)));
			bIsValid = false;
			continue;
		}

		bool bAlreadyExists = false;
		UniqueRegionIds.Add(Region.RegionId, &bAlreadyExists);

		if (bAlreadyExists)
		{
			OutErrors.Add(FText::FromString(FString::Printf(
				TEXT("Duplicate RegionId '%s' found at index %d - Region IDs must be unique"),
				*Region.RegionId.ToString(), i)));
			bIsValid = false;
		}
	}

	return bIsValid;
}
