// Copyright ALIS. All Rights Reserved.

#include "Support/DeferredPickupTestDouble.h"

ADeferredPickupTestActor::ADeferredPickupTestActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SetReplicates(false);
}
