// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"
#include "LootEntryTypes.generated.h"

/**
 * Authored seed entry for world storage initialization.
 * Also used as a compact item-count payload for Take All style transfers.
 */
USTRUCT(BlueprintType)
struct PROJECTCORE_API FLootEntryView
{
	GENERATED_BODY()

	/**
	 * Object to place into world storage.
	 * Filtered via FLootEntryViewCustomization in ProjectPlacementEditor.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	FPrimaryAssetId ObjectId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = 1))
	int32 Quantity = 1;

	bool IsValid() const { return ObjectId.IsValid() && Quantity > 0; }
};

/**
 * Runtime transfer request between inventory and a world container session.
 *
 * For consume requests:
 * - EntryInstanceId identifies the world-container entry to remove from
 * - Quantity is the amount to consume from that entry
 *
 * For store requests:
 * - ObjectId/Quantity identify the item payload to store
 * - GridPos/bRotated describe the desired placement in the world container
 */
USTRUCT(BlueprintType)
struct PROJECTCORE_API FContainerEntryTransfer
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Container")
	int32 EntryInstanceId = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Container")
	FPrimaryAssetId ObjectId;

	UPROPERTY(BlueprintReadWrite, Category = "Container", meta = (ClampMin = 1))
	int32 Quantity = 1;

	UPROPERTY(BlueprintReadWrite, Category = "Container")
	FIntPoint GridPos = FIntPoint(-1, -1);

	UPROPERTY(BlueprintReadWrite, Category = "Container")
	bool bRotated = false;

	bool HasEntryInstance() const { return EntryInstanceId > 0; }
	bool HasPlacement() const { return GridPos.X >= 0 && GridPos.Y >= 0; }
	bool IsValidForConsume() const { return EntryInstanceId > 0 && Quantity > 0; }
	bool IsValidForStore() const { return ObjectId.IsValid() && Quantity > 0; }
};
