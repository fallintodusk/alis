// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/IPickupSource.h"
#include "DeferredPickupTestDouble.generated.h"

/**
 * Minimal IPickupSource implementer used by
 * inventory_objectdef_load_race.md Slice-2 / Slice-3 tests.
 *
 * - Returns a configurable FPrimaryAssetId from GetObjectDefinitionId.
 * - Tracks Consume call count (for exactly-once assertions).
 * - Uses a plain AActor base so it can be spawned in any editor world.
 *
 * Test-only; NOT a production pickup shape. Production pickups compose a
 * UPickupCapabilityComponent on top of a UDefinitionIdProvider host.
 */
UCLASS()
class ADeferredPickupTestActor : public AActor, public IPickupSource
{
	GENERATED_BODY()

public:
	ADeferredPickupTestActor();

	void ConfigurePickup(const FPrimaryAssetId& InObjectId, int32 InQuantity)
	{
		ConfiguredObjectId = InObjectId;
		ConfiguredQuantity = InQuantity;
	}

	int32 GetConsumeCallCount() const { return ConsumeCallCount; }
	int32 GetConsumedTotal() const { return ConsumedTotal; }

	// IPickupSource
	virtual FPrimaryAssetId GetObjectDefinitionId_Implementation() const override { return ConfiguredObjectId; }
	virtual int32 GetQuantity_Implementation() const override { return ConfiguredQuantity; }
	virtual void SetQuantity_Implementation(int32 NewQuantity) override { ConfiguredQuantity = NewQuantity; }
	virtual void Consume_Implementation(int32 Quantity) override
	{
		++ConsumeCallCount;
		ConsumedTotal += Quantity;
		ConfiguredQuantity = FMath::Max(0, ConfiguredQuantity - Quantity);
	}

private:
	FPrimaryAssetId ConfiguredObjectId;
	int32 ConfiguredQuantity = 1;
	int32 ConsumeCallCount = 0;
	int32 ConsumedTotal = 0;
};
