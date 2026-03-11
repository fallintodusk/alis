// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/PrimaryAssetId.h"
#include "InventoryTypes.h"
#include "InventorySaveData.generated.h"

USTRUCT(BlueprintType)
struct PROJECTINVENTORY_API FInventoryEntrySaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	int32 InstanceId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	FPrimaryAssetId ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	int32 Quantity = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	FGameplayTag ContainerId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	FIntPoint GridPos = FIntPoint(-1, -1);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	bool bRotated = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	FInventoryInstanceData InstanceData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	TArray<FMagnitudeOverride> OverrideMagnitudes;
};

USTRUCT(BlueprintType)
struct PROJECTINVENTORY_API FInventorySaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	TArray<FInventoryEntrySaveData> Entries;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	TMap<FGameplayTag, int32> EquippedInstanceIds;
};

/** Serialize inventory save data to a binary blob. */
PROJECTINVENTORY_API bool SerializeInventorySaveData(const FInventorySaveData& InData, TArray<uint8>& OutBytes);

/** Deserialize inventory save data from a binary blob. */
PROJECTINVENTORY_API bool DeserializeInventorySaveData(const TArray<uint8>& InBytes, FInventorySaveData& OutData);
