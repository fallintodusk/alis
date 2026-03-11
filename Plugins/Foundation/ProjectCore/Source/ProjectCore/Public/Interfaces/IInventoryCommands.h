// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "UObject/PrimaryAssetId.h"
#include "IInventoryCommands.generated.h"

/**
 * Write-side inventory interface.
 * Implemented by inventory component; routes to server RPCs.
 */
UINTERFACE(MinimalAPI)
class UInventoryCommands : public UInterface
{
	GENERATED_BODY()
};

class PROJECTCORE_API IInventoryCommands
{
	GENERATED_BODY()

public:
	virtual void RequestAddItem(FPrimaryAssetId ObjectId, int32 Quantity) = 0;
	virtual void RequestRemoveItem(int32 InstanceId, int32 Quantity) = 0;
	virtual void RequestMoveItem(int32 InstanceId, FGameplayTag FromContainer, FIntPoint FromPos, FGameplayTag ToContainer, FIntPoint ToPos, int32 Quantity, bool bRotated) = 0;
	virtual void RequestUseItem(int32 InstanceId) = 0;
	virtual void RequestEquipItem(int32 InstanceId, FGameplayTag EquipSlot) = 0;
	virtual void RequestUnequipItem(FGameplayTag EquipSlot) = 0;
	virtual void RequestDropItem(int32 InstanceId, int32 Quantity) = 0;
	virtual void RequestSwapHands() = 0;
	virtual void RequestSplitStack(int32 InstanceId, int32 SplitQuantity) = 0;
};
