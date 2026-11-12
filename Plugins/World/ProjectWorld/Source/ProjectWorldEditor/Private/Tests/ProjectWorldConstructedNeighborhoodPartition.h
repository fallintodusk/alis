// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "ProjectWorldConstructedNeighborhoodInput.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldConstructedNeighborhood
{
	struct FPartitionOutput
	{
		TArray<FUnit> Units;
		FExpectedOutputs Expected;
		TSharedPtr<FJsonObject> Receipt;
		double MaximumOrdinaryNorm=0;
	};
	bool BuildSharedPartition(const FScene& Scene,FPartitionOutput& Out,FString& Error);
}
#endif
