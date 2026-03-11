// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "UObject/PrimaryAssetId.h"
#include "IPickupSource.generated.h"

/**
 * IPickupSource
 *
 * Interface for pickupable sources that provide ObjectDefinition and quantity,
 * and can be consumed after successful inventory transfer.
 *
 * Implemented by pickup components (capabilities) to keep Inventory decoupled
 * from concrete gameplay modules.
 */
UINTERFACE(MinimalAPI, BlueprintType)
class UPickupSource : public UInterface
{
	GENERATED_BODY()
};

class PROJECTCORE_API IPickupSource
{
	GENERATED_BODY()

public:
	/** ObjectDefinition asset id for this pickup source. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Pickup")
	FPrimaryAssetId GetObjectDefinitionId() const;

	/** Current quantity available to pick up. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Pickup")
	int32 GetQuantity() const;

	/** Override quantity for this source (server-only). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Pickup")
	void SetQuantity(int32 Quantity);

	/** Consume quantity from this source (server-only). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Pickup")
	void Consume(int32 Quantity);
};
