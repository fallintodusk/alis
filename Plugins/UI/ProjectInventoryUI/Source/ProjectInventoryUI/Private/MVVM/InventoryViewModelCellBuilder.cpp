// Copyright ALIS. All Rights Reserved.

#include "MVVM/InventoryViewModelCellBuilder.h"
#include "MVVM/InventoryViewModel.h"

void FInventoryViewModelCellBuilder::Build(
    const TArray<FInventoryEntryView>& Entries,
    FGameplayTag ContainerId,
    int32 GridWidth,
    int32 GridHeight,
    TArray<int32>& OutCellInstanceIds,
    TArray<FText>& OutCellTexts)
{
    OutCellTexts.Reset();
    OutCellInstanceIds.Reset();

    if (!ContainerId.IsValid() || GridWidth <= 0 || GridHeight <= 0)
    {
        return;
    }

    const int32 CellCount = GridWidth * GridHeight;
    OutCellTexts.SetNum(CellCount);
    OutCellInstanceIds.SetNum(CellCount);

    for (int32 Index = 0; Index < CellCount; ++Index)
    {
        OutCellTexts[Index] = FText::GetEmpty();
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
        if (!Entry.IconCode.IsEmpty())
        {
            CellLabel = Entry.IconCode;
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[CellBuilder] Item '%s' has no IconCode - using text fallback"),
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
                if (OffsetX == 0 && OffsetY == 0)
                {
                    OutCellTexts[Index] = FText::FromString(CellLabel);
                }
            }
        }
    }
}

FString FInventoryViewModelCellBuilder::BuildEntryLabel(const FText& DisplayName, int32 Quantity, const FPrimaryAssetId& ItemId)
{
    FString Name = DisplayName.IsEmpty() ? ItemId.ToString() : DisplayName.ToString();
    if (Quantity > 1)
    {
        return FString::Printf(TEXT("%s x%d"), *Name, Quantity);
    }
    return Name;
}
