// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

class FJsonObject;
struct FProjectWorldRuntimeProfile;
class UWorld;

namespace ProjectWorldStaticPartitionAudit
{
	int32 CountIntersectedCells(const FBox& Bounds, int32 CellSizeMeters);

	bool Capture(
		UWorld* World,
		const TArray<FProjectWorldRuntimeProfile>& Profiles,
		const FString& SelectedProfileId,
		TSharedPtr<FJsonObject>& OutReceipt,
		FString& OutError);
}
