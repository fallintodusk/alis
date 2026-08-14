// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

class UWorld;

namespace ProjectWorldPartitionPolicy
{
	bool DisableHLOD(UWorld* World, FString& OutError);
	int32 DisableGeneratedActorHLOD(UWorld* World);
	int32 CountHLODLayerReferences(UWorld* World);
}
