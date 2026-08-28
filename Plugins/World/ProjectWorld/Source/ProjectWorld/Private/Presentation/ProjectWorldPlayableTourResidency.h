// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

class FJsonObject;
class UWorldPartition;
class UWorldPartitionRuntimeCell;
enum class EWorldPartitionRuntimeCellState : uint8;

class FProjectWorldPlayableTourResidency
{
public:
	bool FreezeCenterCells(UWorldPartition& Partition, const FVector& Center, FString& OutError);
	bool FreezeCenterCellIds(TSet<FGuid> CellIds, FString& OutError);
	void ObserveCell(
		const UWorldPartitionRuntimeCell& Cell,
		bool bReachedEdge,
		bool bReturnedToCenter);
	void ObserveCellState(
		const FGuid& Guid,
		EWorldPartitionRuntimeCellState State,
		bool bReachedEdge,
		bool bReturnedToCenter);
	bool HasCompleteCycle() const;
	void AppendReceiptFields(FJsonObject& Receipt) const;

	int32 GetUnloadedCount() const { return UnloadedCenterCells.Num(); }
	int32 GetReloadedCount() const { return ReloadedCenterCells.Num(); }

private:
	TSet<FGuid> InitialCenterCells;
	TSet<FGuid> UnloadedCenterCells;
	TSet<FGuid> ReloadedCenterCells;
};
