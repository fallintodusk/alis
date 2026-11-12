// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "ProjectWorldLayerProducerRegistry.h"
namespace ProjectWorldConstructedRealization
{
	bool ApplyOutput(const FProjectWorldLayerApplyContext& Context,FProjectWorldLayerInventory& Inventory,
		FProjectWorldRealizationResult& Result,FString& Error);
	bool CaptureOutput(const FProjectWorldLayerApplyContext& Context,FProjectWorldLayerInventory& Inventory,
		FProjectWorldRealizationResult& Result,FString& Error);
}
