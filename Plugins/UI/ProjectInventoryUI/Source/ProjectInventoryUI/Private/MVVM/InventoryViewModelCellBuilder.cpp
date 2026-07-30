// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "MVVM/InventoryViewModelCellBuilder.h"
#include "MVVM/InventoryViewModel.h"

DEFINE_LOG_CATEGORY_STATIC(LogInventoryViewModelCellBuilder, Log, All);

void FInventoryViewModelCellBuilder::Build(
    const TArray<FInventoryEntryView>& Entries,
    FGameplayTag ContainerId,
    int32 GridWidth,
    int32 GridHeight,
    TArray<int32>& OutCellInstanceIds,
    TArray<FInventoryCellVisualState>& OutCellVisuals)
{
    OutCellVisuals.Reset();
    OutCellInstanceIds.Reset();

    if (!ContainerId.IsValid() || GridWidth <= 0 || GridHeight <= 0)
    {
        return;
    }

    const int32 CellCount = GridWidth * GridHeight;
    OutCellVisuals.SetNum(CellCount);
    OutCellInstanceIds.SetNum(CellCount);

    for (int32 Index = 0; Index < CellCount; ++Index)
    {
        OutCellVisuals[Index] = FInventoryCellVisualState();
        OutCellInstanceIds[Index] = UInventoryViewModel::EmptyCellInstanceId;
    }

    for (const FInventoryEntryView& Entry : Entries)
    {
        if (Entry.ContainerId != ContainerId)
        {
            continue;
        }
        if (Entry.InstanceId <= 0)
        {
            continue;
        }

        const FIntPoint BaseSize = Entry.GridSize;
        FIntPoint ItemSize = Entry.bRotated ? FIntPoint(BaseSize.Y, BaseSize.X) : BaseSize;
        if (ItemSize.X <= 0 || ItemSize.Y <= 0)
        {
            ItemSize = FIntPoint(1, 1);
        }

        const int32 StartX = Entry.GridPos.X;
        const int32 StartY = Entry.GridPos.Y;
        if (StartX < 0 || StartY < 0)
        {
            continue;
        }

        // Prefer icon codepoint; fall back to text label if missing
        FString CellLabel;
        bool bUseIconFont = false;
        if (!Entry.IconCode.IsEmpty())
        {
            CellLabel = Entry.IconCode;
            bUseIconFont = true;
        }
        else
        {
            UE_LOG(LogInventoryViewModelCellBuilder, Verbose, TEXT("Item '%s' has no IconCode - using text fallback"),
                *Entry.ItemId.ToString());
            CellLabel = BuildEntryLabel(Entry.DisplayName, Entry.Quantity, Entry.ItemId);
        }

        for (int32 OffsetY = 0; OffsetY < ItemSize.Y; ++OffsetY)
        {
            for (int32 OffsetX = 0; OffsetX < ItemSize.X; ++OffsetX)
            {
                const int32 X = StartX + OffsetX;
                const int32 Y = StartY + OffsetY;
                if (X < 0 || Y < 0 || X >= GridWidth || Y >= GridHeight)
                {
                    continue;
                }
                const int32 Index = Y * GridWidth + X;
                if (!OutCellInstanceIds.IsValidIndex(Index))
                {
                    continue;
                }
                OutCellInstanceIds[Index] = Entry.InstanceId;
                OutCellVisuals[Index].InstanceId = Entry.InstanceId;
                if (OffsetX == 0 && OffsetY == 0)
                {
                    OutCellVisuals[Index].PrimaryText = FText::FromString(CellLabel);
                    OutCellVisuals[Index].QuantityText = Entry.Quantity > 1
                        ? FText::AsNumber(Entry.Quantity)
                        : FText::GetEmpty();
                    OutCellVisuals[Index].bUseIconFont = bUseIconFont;
                    OutCellVisuals[Index].bShowQuantity = Entry.Quantity > 1;
                    OutCellVisuals[Index].bIsAnchorCell = true;
                }
            }
        }
    }
}

FString FInventoryViewModelCellBuilder::BuildEntryLabel(const FText& DisplayName, int32 Quantity, const FPrimaryAssetId& ItemId)
{
    (void)Quantity;
    FString Name = DisplayName.IsEmpty() ? ItemId.ToString() : DisplayName.ToString();
    return Name;
}
