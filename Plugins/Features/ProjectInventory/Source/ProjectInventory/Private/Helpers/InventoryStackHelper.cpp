// Copyright ALIS. All Rights Reserved.

#include "Helpers/InventoryStackHelper.h"
#include "Inventory/InventoryTypes.h"
#include "Types/InventoryContainerConfig.h"
#include "Interfaces/IItemDataProvider.h"

int32 FInventoryStackHelper::CalculateStackableQuantity(
    const FInventoryEntry& Entry,
    const FItemDataView& ItemData,
    int32 MaxStack,
    const FInventoryContainerConfig& Container,
    float ContainerWeight,
    float ContainerVolume,
    int32 Quantity)
{
    // Items with overrides don't stack
    if (Entry.OverrideMagnitudes.Num() > 0)
    {
        return 0;
    }

    // Already at max stack
    if (Entry.Quantity >= MaxStack)
    {
        return 0;
    }

    int32 SpaceInStack = MaxStack - Entry.Quantity;

    // Apply weight constraint
    if (Container.MaxWeight > 0.f && ItemData.Weight > 0.f)
    {
        const float WeightLeft = Container.MaxWeight - ContainerWeight;
        if (WeightLeft <= 0.f)
        {
            return 0;
        }
        SpaceInStack = FMath::Min(SpaceInStack, FMath::FloorToInt(WeightLeft / ItemData.Weight));
    }

    // Apply volume constraint
    if (Container.MaxVolume > 0.f && ItemData.Volume > 0.f)
    {
        const float VolumeLeft = Container.MaxVolume - ContainerVolume;
        if (VolumeLeft <= 0.f)
        {
            return 0;
        }
        SpaceInStack = FMath::Min(SpaceInStack, FMath::FloorToInt(VolumeLeft / ItemData.Volume));
    }

    return FMath::Min(Quantity, SpaceInStack);
}

int32 FInventoryStackHelper::CalculateNewStackQuantity(
    const FItemDataView& ItemData,
    const FInventoryContainerConfig& Container,
    float ContainerWeight,
    float ContainerVolume,
    int32 DesiredQuantity)
{
    int32 StackSize = DesiredQuantity;

    // Apply weight constraint
    if (Container.MaxWeight > 0.f && ItemData.Weight > 0.f)
    {
        const float WeightLeft = Container.MaxWeight - ContainerWeight;
        if (WeightLeft <= 0.f)
        {
            return 0;
        }
        StackSize = FMath::Min(StackSize, FMath::FloorToInt(WeightLeft / ItemData.Weight));
    }

    // Apply volume constraint
    if (Container.MaxVolume > 0.f && ItemData.Volume > 0.f)
    {
        const float VolumeLeft = Container.MaxVolume - ContainerVolume;
        if (VolumeLeft <= 0.f)
        {
            return 0;
        }
        StackSize = FMath::Min(StackSize, FMath::FloorToInt(VolumeLeft / ItemData.Volume));
    }

    return StackSize;
}

int32 FInventoryStackHelper::CalculateAllowedQuantity(
    const FItemDataView& ItemData,
    float MaxWeight,
    float MaxVolume,
    float CurrentWeight,
    float CurrentVolume,
    int32 RequestedQuantity)
{
    int32 AllowedQuantity = RequestedQuantity;

    if (ItemData.Weight > 0.f && MaxWeight > 0.f)
    {
        const float WeightLeft = MaxWeight - CurrentWeight;
        if (WeightLeft <= 0.f)
        {
            return 0;
        }
        AllowedQuantity = FMath::Min(AllowedQuantity, FMath::FloorToInt(WeightLeft / ItemData.Weight));
    }

    if (ItemData.Volume > 0.f && MaxVolume > 0.f)
    {
        const float VolumeLeft = MaxVolume - CurrentVolume;
        if (VolumeLeft <= 0.f)
        {
            return 0;
        }
        AllowedQuantity = FMath::Min(AllowedQuantity, FMath::FloorToInt(VolumeLeft / ItemData.Volume));
    }

    return AllowedQuantity;
}
