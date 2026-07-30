// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Support/DeferredPickupTestDouble.h"

ADeferredPickupTestActor::ADeferredPickupTestActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SetReplicates(false);
}
