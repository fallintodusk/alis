// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InventoryCellVisualState.generated.h"

USTRUCT(BlueprintType)
struct PROJECTINVENTORYUI_API FInventoryCellVisualState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 InstanceId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FText PrimaryText;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FText QuantityText;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	bool bUseIconFont = false;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	bool bShowQuantity = false;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	bool bIsAnchorCell = false;

	bool IsEmpty() const
	{
		return InstanceId == INDEX_NONE && PrimaryText.IsEmpty() && QuantityText.IsEmpty();
	}
};
