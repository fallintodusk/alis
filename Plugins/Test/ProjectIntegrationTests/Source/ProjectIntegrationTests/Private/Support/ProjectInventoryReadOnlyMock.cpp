// Copyright ALIS. All Rights Reserved.

#include "Support/ProjectInventoryReadOnlyMock.h"

void UProjectInventoryReadOnlyMock::SetEntries(const TArray<FInventoryEntryView>& InEntries)
{
    Entries = InEntries;
}

void UProjectInventoryReadOnlyMock::SetContainers(const TArray<FInventoryContainerView>& InContainers)
{
    Containers = InContainers;
}

void UProjectInventoryReadOnlyMock::SetTotals(
    float InCurrentWeight,
    float InMaxWeight,
    float InCurrentVolume,
    float InMaxVolume,
    int32 InMaxSlots)
{
    CurrentWeight = InCurrentWeight;
    MaxWeight = InMaxWeight;
    CurrentVolume = InCurrentVolume;
    MaxVolume = InMaxVolume;
    MaxSlots = InMaxSlots;
}

void UProjectInventoryReadOnlyMock::BroadcastInventoryChanged()
{
    InventoryViewChangedDelegate.Broadcast();
}

void UProjectInventoryReadOnlyMock::BroadcastInventoryError(const FText& ErrorMessage)
{
    InventoryErrorDelegate.Broadcast(ErrorMessage);
}

void UProjectInventoryReadOnlyMock::GetEntriesView(TArray<FInventoryEntryView>& OutEntries) const
{
    OutEntries = Entries;
}

void UProjectInventoryReadOnlyMock::GetContainersView(TArray<FInventoryContainerView>& OutContainers) const
{
    OutContainers = Containers;
}

float UProjectInventoryReadOnlyMock::GetCurrentWeight() const
{
    return CurrentWeight;
}

float UProjectInventoryReadOnlyMock::GetMaxWeight() const
{
    return MaxWeight;
}

float UProjectInventoryReadOnlyMock::GetCurrentVolume() const
{
    return CurrentVolume;
}

float UProjectInventoryReadOnlyMock::GetMaxVolume() const
{
    return MaxVolume;
}

int32 UProjectInventoryReadOnlyMock::GetMaxSlots() const
{
    return MaxSlots;
}

FOnInventoryViewChanged& UProjectInventoryReadOnlyMock::OnInventoryViewChanged()
{
    return InventoryViewChangedDelegate;
}

FOnInventoryErrorNative& UProjectInventoryReadOnlyMock::OnInventoryErrorNative()
{
    return InventoryErrorDelegate;
}

