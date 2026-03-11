// Copyright ALIS. All Rights Reserved.

#include "Helpers/InventoryGridPlacement.h"
#include "Types/InventoryContainerConfig.h"

FIntPoint FInventoryGridPlacement::SanitizeGridSize(FIntPoint InSize)
{
    return FIntPoint(FMath::Max(1, InSize.X), FMath::Max(1, InSize.Y));
}

FIntPoint FInventoryGridPlacement::GetItemGridSize(FIntPoint BaseSize, bool bRotated)
{
    FIntPoint Size = SanitizeGridSize(BaseSize);
    if (bRotated)
    {
        Swap(Size.X, Size.Y);
    }
    return Size;
}

bool FInventoryGridPlacement::IsRectWithinContainer(
    const FInventoryContainerConfig& Container,
    FIntPoint GridPos,
    FIntPoint ItemSize)
{
    if (GridPos.X < 0 || GridPos.Y < 0)
    {
        return false;
    }

    // Slot-based containers: GridPos.X = slot index, GridPos.Y must be 0
    if (Container.bSlotBased)
    {
        if (GridPos.Y != 0)
        {
            return false;
        }
        if (Container.MaxCells > 0 && GridPos.X >= Container.MaxCells)
        {
            return false;
        }
        return true;
    }

    // Grid-based containers
    if (Container.GridSize.X <= 0 || Container.GridSize.Y <= 0)
    {
        return false;
    }

    // Width-only validation (for hands): tall items can overflow visually
    if (Container.bWidthOnlyValidation)
    {
        if (GridPos.X + ItemSize.X > Container.GridSize.X)
        {
            return false;
        }
        // Height overflow allowed
    }
    else
    {
        // Standard full bounds check
        if (GridPos.X + ItemSize.X > Container.GridSize.X ||
            GridPos.Y + ItemSize.Y > Container.GridSize.Y)
        {
            return false;
        }
    }

    // MaxCells constraint - use effective occupancy size for width-only containers
    if (Container.MaxCells > 0)
    {
        const FIntPoint EffectiveSize = ClampSizeForContainer(Container, ItemSize);
        const int32 MaxIndex = (GridPos.Y + EffectiveSize.Y - 1) * Container.GridSize.X + (GridPos.X + EffectiveSize.X - 1);
        if (MaxIndex >= Container.MaxCells)
        {
            return false;
        }
    }

    return true;
}

bool FInventoryGridPlacement::DoRectsOverlap(
    FIntPoint Pos1, FIntPoint Size1,
    FIntPoint Pos2, FIntPoint Size2)
{
    return Pos1.X < (Pos2.X + Size2.X) &&
           (Pos1.X + Size1.X) > Pos2.X &&
           Pos1.Y < (Pos2.Y + Size2.Y) &&
           (Pos1.Y + Size1.Y) > Pos2.Y;
}

FIntPoint FInventoryGridPlacement::ClampSizeForContainer(
    const FInventoryContainerConfig& Container,
    FIntPoint ItemSize)
{
    if (Container.bWidthOnlyValidation && Container.GridSize.Y > 0)
    {
        return FIntPoint(ItemSize.X, FMath::Min(ItemSize.Y, Container.GridSize.Y));
    }
    return ItemSize;
}

bool FInventoryGridPlacement::FindFreeGridPos(
    const FInventoryContainerConfig& Container,
    FIntPoint ItemSize,
    const TFunction<bool(FIntPoint TestPos, FIntPoint TestSize)>& IsOccupied,
    FIntPoint& OutPos)
{
    // Slot-based: use FindFreeSlot instead
    if (Container.bSlotBased)
    {
        int32 SlotCount = Container.MaxCells > 0 ? Container.MaxCells : Container.GridSize.X;
        int32 SlotIndex = 0;
        if (FindFreeSlot(SlotCount, [&](int32 Idx) { return IsOccupied(FIntPoint(Idx, 0), ItemSize); }, SlotIndex))
        {
            OutPos = FIntPoint(SlotIndex, 0);
            return true;
        }
        return false;
    }

    // Use effective occupancy size for width-only containers
    const FIntPoint EffectiveSize = ClampSizeForContainer(Container, ItemSize);

    // Check if item fits at all
    if (!IsRectWithinContainer(Container, FIntPoint(0, 0), ItemSize))
    {
        return false;
    }

    // Scan bounds use effective size (clamped for width-only containers)
    const int32 MaxX = Container.GridSize.X - EffectiveSize.X;
    const int32 MaxY = Container.GridSize.Y - EffectiveSize.Y;

    for (int32 Y = 0; Y <= MaxY; ++Y)
    {
        for (int32 X = 0; X <= MaxX; ++X)
        {
            const FIntPoint TestPos(X, Y);
            // Pass original ItemSize for visual/logical purposes, but occupancy uses EffectiveSize
            if (!IsOccupied(TestPos, EffectiveSize))
            {
                OutPos = TestPos;
                return true;
            }
        }
    }

    return false;
}

bool FInventoryGridPlacement::FindFreeSlot(
    int32 SlotCount,
    const TFunction<bool(int32 SlotIndex)>& IsSlotOccupied,
    int32& OutSlotIndex)
{
    for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
    {
        if (!IsSlotOccupied(SlotIndex))
        {
            OutSlotIndex = SlotIndex;
            return true;
        }
    }
    return false;
}
