// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LootTypes.generated.h"

/**
 * Single loot entry for designer-placed loot containers.
 * References ObjectDefinition via FPrimaryAssetId for lightweight serialization.
 *
 * Pattern: Same as FInventoryEntry - uses FPrimaryAssetId to avoid
 * hard asset references and enable async loading.
 */
USTRUCT(BlueprintType)
struct PROJECTINVENTORY_API FLootEntry
{
	GENERATED_BODY()

	FLootEntry() = default;
	FLootEntry(FPrimaryAssetId InObjectId, int32 InQuantity)
		: ObjectId(InObjectId), Quantity(InQuantity) {}

	/** Object definition to spawn. Uses FPrimaryAssetId for lightweight replication. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	FPrimaryAssetId ObjectId;

	/** Number of items to give. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot", meta = (ClampMin = 1))
	int32 Quantity = 1;

	/** Check if this entry is valid (has object ID set). */
	bool IsValid() const { return ObjectId.IsValid(); }
};
