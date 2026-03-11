// Copyright ALIS. All Rights Reserved.

#include "Helpers/InventoryContainerHelper.h"
#include "Types/InventoryContainerConfig.h"

int32 FInventoryContainerHelper::GetContainerIndex(
    FGameplayTag ContainerId,
    const TArray<FInventoryContainerConfig>& Containers)
{
    for (int32 Index = 0; Index < Containers.Num(); ++Index)
    {
        if (Containers[Index].ContainerId == ContainerId)
        {
            return Index;
        }
    }
    return INDEX_NONE;
}

int32 FInventoryContainerHelper::GetContainerTotalCells(const FInventoryContainerConfig& Container)
{
    if (Container.GridSize.X <= 0 || Container.GridSize.Y <= 0)
    {
        return 0;
    }
    return Container.GridSize.X * Container.GridSize.Y;
}

int32 FInventoryContainerHelper::GetContainerCellCount(const FInventoryContainerConfig& Container)
{
    const int32 TotalCells = GetContainerTotalCells(Container);
    if (Container.MaxCells > 0 && TotalCells > 0)
    {
        return FMath::Min(Container.MaxCells, TotalCells);
    }
    return TotalCells;
}

int32 FInventoryContainerHelper::GetContainerSlotOffset(
    FGameplayTag ContainerId,
    const TArray<FInventoryContainerConfig>& Containers)
{
    const int32 ContainerIndex = GetContainerIndex(ContainerId, Containers);
    if (ContainerIndex == INDEX_NONE)
    {
        return -1;
    }

    int32 Offset = 0;
    for (int32 Index = 0; Index < ContainerIndex; ++Index)
    {
        Offset += GetContainerCellCount(Containers[Index]);
    }
    return Offset;
}

int32 FInventoryContainerHelper::ComputeSlotIndex(
    FGameplayTag ContainerId,
    FIntPoint GridPos,
    const TArray<FInventoryContainerConfig>& Containers)
{
    if (GridPos.X < 0 || GridPos.Y < 0)
    {
        return -1;
    }

    const int32 Offset = GetContainerSlotOffset(ContainerId, Containers);
    if (Offset < 0)
    {
        return -1;
    }

    const int32 ContainerIndex = GetContainerIndex(ContainerId, Containers);
    if (ContainerIndex == INDEX_NONE)
    {
        return -1;
    }

    const FIntPoint GridSize = Containers[ContainerIndex].GridSize;
    if (GridSize.X <= 0 || GridSize.Y <= 0)
    {
        return -1;
    }

    const int32 LocalIndex = GridPos.Y * GridSize.X + GridPos.X;
    const int32 MaxCells = Containers[ContainerIndex].MaxCells;
    if (MaxCells > 0 && LocalIndex >= MaxCells)
    {
        return -1;
    }
    return Offset + LocalIndex;
}

bool FInventoryContainerHelper::TryGetGridPosFromSlot(
    FGameplayTag ContainerId,
    int32 SlotIndex,
    const TArray<FInventoryContainerConfig>& Containers,
    FIntPoint& OutGridPos)
{
    if (SlotIndex < 0)
    {
        return false;
    }

    const int32 Offset = GetContainerSlotOffset(ContainerId, Containers);
    if (Offset < 0 || SlotIndex < Offset)
    {
        return false;
    }

    const int32 ContainerIndex = GetContainerIndex(ContainerId, Containers);
    if (ContainerIndex == INDEX_NONE)
    {
        return false;
    }

    const FInventoryContainerConfig& Container = Containers[ContainerIndex];
    if (Container.GridSize.X <= 0)
    {
        return false;
    }

    const int32 LocalIndex = SlotIndex - Offset;
    const int32 CellCount = GetContainerCellCount(Container);
    if (CellCount > 0 && LocalIndex >= CellCount)
    {
        return false;
    }

    OutGridPos.X = LocalIndex % Container.GridSize.X;
    OutGridPos.Y = LocalIndex / Container.GridSize.X;
    return true;
}

void FInventoryContainerHelper::UpsertContainerConfig(
    const FInventoryContainerConfig& Config,
    TArray<FInventoryContainerConfig>& OutContainers)
{
    const int32 ExistingIndex = GetContainerIndex(Config.ContainerId, OutContainers);
    if (ExistingIndex >= 0)
    {
        OutContainers[ExistingIndex] = Config;
        return;
    }
    OutContainers.Add(Config);
}
