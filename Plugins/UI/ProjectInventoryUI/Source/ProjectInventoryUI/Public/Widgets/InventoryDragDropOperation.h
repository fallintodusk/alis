// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "GameplayTagContainer.h"
#include "Interfaces/IInventoryReadOnly.h"
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

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	bool bFromNearbyContainer = false;

	void ApplyRotationFromEntry(const FInventoryEntryView& Entry, bool bInRotated)
	{
		bRotated = bInRotated;
		const int32 Width = FMath::Max(1, Entry.GridSize.X);
		const int32 Height = FMath::Max(1, Entry.GridSize.Y);
		ItemSize = bRotated ? FIntPoint(Height, Width) : FIntPoint(Width, Height);
	}
};
