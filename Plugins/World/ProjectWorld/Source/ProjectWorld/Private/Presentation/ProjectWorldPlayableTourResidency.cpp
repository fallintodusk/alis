// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Presentation/ProjectWorldPlayableTourResidency.h"

#include "Dom/JsonObject.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionRuntimeCell.h"
#include "WorldPartition/WorldPartitionRuntimeHash.h"

namespace
{
	TArray<TSharedPtr<FJsonValue>> GuidValues(const TSet<FGuid>& Guids)
	{
		TArray<FString> Strings;
		Strings.Reserve(Guids.Num());
		for (const FGuid& Guid : Guids)
		{
			Strings.Add(Guid.ToString(EGuidFormats::DigitsWithHyphensLower));
		}
		Strings.Sort();

		TArray<TSharedPtr<FJsonValue>> Values;
		Values.Reserve(Strings.Num());
		for (const FString& Value : Strings)
		{
			Values.Add(MakeShared<FJsonValueString>(Value));
		}
		return Values;
	}
}

bool FProjectWorldPlayableTourResidency::FreezeCenterCells(
	UWorldPartition& Partition,
	const FVector& Center,
	FString& OutError)
{
	InitialCenterCells.Reset();
	UnloadedCenterCells.Reset();
	ReloadedCenterCells.Reset();
	if (Partition.RuntimeHash == nullptr)
	{
		OutError = TEXT("Playable-tour center-cell census requires the live runtime partition hash.");
		return false;
	}

	const uint8 LoadedState = static_cast<uint8>(EWorldPartitionRuntimeCellState::Loaded);
	Partition.RuntimeHash->ForEachStreamingCells([this, &Center, LoadedState](
		const UWorldPartitionRuntimeCell* Cell)
	{
		if (Cell == nullptr || Cell->IsAlwaysLoaded() ||
			static_cast<uint8>(Cell->GetCurrentState()) < LoadedState)
		{
			return true;
		}
		const FBox Bounds = Cell->GetStreamingBounds();
		if (Bounds.IsValid && Center.X >= Bounds.Min.X && Center.X <= Bounds.Max.X &&
			Center.Y >= Bounds.Min.Y && Center.Y <= Bounds.Max.Y)
		{
			InitialCenterCells.Add(Cell->GetGuid());
		}
		return true;
	});
	return FreezeCenterCellIds(MoveTemp(InitialCenterCells), OutError);
}

bool FProjectWorldPlayableTourResidency::FreezeCenterCellIds(
	TSet<FGuid> CellIds,
	FString& OutError)
{
	InitialCenterCells = MoveTemp(CellIds);
	UnloadedCenterCells.Reset();
	ReloadedCenterCells.Reset();
	if (InitialCenterCells.IsEmpty())
	{
		OutError = TEXT("No initially loaded spatial runtime cell contains the playable-tour center.");
		return false;
	}
	return true;
}

void FProjectWorldPlayableTourResidency::ObserveCell(
	const UWorldPartitionRuntimeCell& Cell,
	bool bReachedEdge,
	bool bReturnedToCenter)
{
	ObserveCellState(Cell.GetGuid(), Cell.GetCurrentState(), bReachedEdge, bReturnedToCenter);
}

void FProjectWorldPlayableTourResidency::ObserveCellState(
	const FGuid& Guid,
	EWorldPartitionRuntimeCellState StateValue,
	bool bReachedEdge,
	bool bReturnedToCenter)
{
	if (!InitialCenterCells.Contains(Guid))
	{
		return;
	}
	const uint8 State = static_cast<uint8>(StateValue);
	const uint8 LoadedState = static_cast<uint8>(EWorldPartitionRuntimeCellState::Loaded);
	if (bReachedEdge && !bReturnedToCenter && State < LoadedState)
	{
		UnloadedCenterCells.Add(Guid);
	}
	if (bReturnedToCenter && UnloadedCenterCells.Contains(Guid) && State >= LoadedState)
	{
		ReloadedCenterCells.Add(Guid);
	}
}

bool FProjectWorldPlayableTourResidency::HasCompleteCycle() const
{
	return !InitialCenterCells.IsEmpty() && !UnloadedCenterCells.IsEmpty() &&
		!ReloadedCenterCells.IsEmpty();
}

void FProjectWorldPlayableTourResidency::AppendReceiptFields(FJsonObject& Receipt) const
{
	Receipt.SetArrayField(TEXT("initial_center_cell_ids"), GuidValues(InitialCenterCells));
	Receipt.SetArrayField(TEXT("unloaded_center_cell_ids"), GuidValues(UnloadedCenterCells));
	Receipt.SetArrayField(TEXT("reloaded_center_cell_ids"), GuidValues(ReloadedCenterCells));
	Receipt.SetNumberField(TEXT("initial_center_cell_count"), InitialCenterCells.Num());
	Receipt.SetNumberField(TEXT("unloaded_center_cell_count"), UnloadedCenterCells.Num());
	Receipt.SetNumberField(TEXT("reloaded_center_cell_count"), ReloadedCenterCells.Num());
	Receipt.SetBoolField(TEXT("center_cell_streaming_cycle"), HasCompleteCycle());
}
