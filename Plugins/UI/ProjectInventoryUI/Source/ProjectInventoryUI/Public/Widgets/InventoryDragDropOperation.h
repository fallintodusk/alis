// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "GameplayTagContainer.h"
#include "InventoryDragDropOperation.generated.h"

/**
 * Drag-drop payload for inventory moves.
 */
UCLASS()
class PROJECTINVENTORYUI_API UInventoryDragDropOperation : public UDragDropOperation
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 InstanceId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 Quantity = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FGameplayTag FromContainer;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FIntPoint FromPos = FIntPoint(-1, -1);

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FIntPoint ItemSize = FIntPoint(1, 1);

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	bool bRotated = false;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FGameplayTag EquipSlotTag;
};
