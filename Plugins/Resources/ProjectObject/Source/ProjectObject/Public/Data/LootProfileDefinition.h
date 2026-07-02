// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UObject/PrimaryAssetId.h"
#include "Types/LootEntryTypes.h"
#include "LootProfileDefinition.generated.h"

/**
 * Shared loot-profile entry for reusable randomized world-storage content.
 * Stored in ULootProfileDefinition.Entries.
 */
USTRUCT(BlueprintType)
struct PROJECTOBJECT_API FLootProfileEntry
{
	GENERATED_BODY()

	/** Candidate object that may be picked into the container. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LootProfile")
	FPrimaryAssetId ObjectId;

	/** Quantity to add when this candidate is selected. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LootProfile", meta = (ClampMin = 1))
	int32 Quantity = 1;

	bool IsValid() const { return ObjectId.IsValid() && Quantity > 0; }

	FLootEntryView ToLootEntryView() const
	{
		FLootEntryView Entry;
		Entry.ObjectId = ObjectId;
		Entry.Quantity = FMath::Max(Quantity, 1);
		return Entry;
	}
};

/**
 * Shared loot profile definition for reusable randomized world-storage fill.
 * Objects reference this from sections.storage.lootProfileId.
 */
UCLASS(BlueprintType)
class PROJECTOBJECT_API ULootProfileDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Stable profile identifier used by GetPrimaryAssetId. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LootProfile")
	FName ProfileId;

	/** Minimum unique picks to draw from Entries. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LootProfile", meta = (ClampMin = 0))
	int32 PickCountMin = 0;

	/** Maximum unique picks to draw from Entries. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LootProfile", meta = (ClampMin = 0))
	int32 PickCountMax = 0;

	/** Candidate entries for randomized fill. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LootProfile")
	TArray<FLootProfileEntry> Entries;

	/** True if this asset was generated from JSON (not hand-created). */
	UPROPERTY(VisibleAnywhere, Category = "Generation", AssetRegistrySearchable)
	bool bGenerated = false;

	/** Generator version that created this asset. */
	UPROPERTY(VisibleAnywhere, Category = "Generation", AssetRegistrySearchable)
	int32 GeneratorVersion = 0;

	/** Source JSON file path (relative to plugin). */
	UPROPERTY(VisibleAnywhere, Category = "Generation", AssetRegistrySearchable)
	FString SourceJsonPath;

	/** Hash of source JSON for incremental regeneration. */
	UPROPERTY(VisibleAnywhere, Category = "Generation", AssetRegistrySearchable)
	FString SourceJsonHash;

	bool HasEntries() const { return Entries.Num() > 0; }

	void BuildLootEntries(TArray<FLootEntryView>& OutEntries, FRandomStream* RandomStream = nullptr) const;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
