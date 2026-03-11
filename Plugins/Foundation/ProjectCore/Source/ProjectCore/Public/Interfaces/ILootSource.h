// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "UObject/PrimaryAssetId.h"
#include "ILootSource.generated.h"

/**
 * Lightweight loot entry view for inventory transfer.
 */
USTRUCT(BlueprintType)
struct PROJECTCORE_API FLootEntryView
{
	GENERATED_BODY()

	/**
	 * Object to give when looted. Must have Pickup capability.
	 * Filtered via FLootEntryViewCustomization in ProjectPlacementEditor module.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	FPrimaryAssetId ObjectId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = 1))
	int32 Quantity = 1;

	bool IsValid() const { return ObjectId.IsValid() && Quantity > 0; }
};

/**
 * ILootSource
 *
 * Interface for loot sources that expose item entries for inventory transfer.
 * Implemented by ObjectDefinition loot components (LootContainerCapability).
 */
UINTERFACE(MinimalAPI, BlueprintType)
class ULootSource : public UInterface
{
	GENERATED_BODY()
};

class PROJECTCORE_API ILootSource
{
	GENERATED_BODY()

public:
	/** Return loot entries for this source. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Loot")
	TArray<FLootEntryView> GetLootEntries() const;

	/** True if the source has no loot. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Loot")
	bool IsLootEmpty() const;

	/** Consume this loot source after successful transfer. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Loot")
	void ConsumeLootSource();
};
