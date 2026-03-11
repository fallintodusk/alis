// Copyright ALIS. All Rights Reserved.

#include "MVVM/InventoryViewModelEquipSlotBuilder.h"
#include "ProjectGameplayTags.h"

void FInventoryViewModelEquipSlotBuilder::GetStandardSlotTags(TArray<FGameplayTag>& OutTags)
{
    OutTags.Reset();
    OutTags.Add(ProjectTags::Item_EquipmentSlot_MainHand);
    OutTags.Add(ProjectTags::Item_EquipmentSlot_OffHand);
    OutTags.Add(ProjectTags::Item_EquipmentSlot_Head);
    OutTags.Add(ProjectTags::Item_EquipmentSlot_Chest);
    OutTags.Add(ProjectTags::Item_EquipmentSlot_Back);
    OutTags.Add(ProjectTags::Item_EquipmentSlot_Legs);
    OutTags.Add(ProjectTags::Item_EquipmentSlot_Feet);
    OutTags.Add(ProjectTags::Item_EquipmentSlot_Accessory);
}

void FInventoryViewModelEquipSlotBuilder::Build(const TArray<FInventoryEntryView>& Entries, FResult& OutResult)
{
    GetStandardSlotTags(OutResult.SlotTags);

    const int32 SlotCount = OutResult.SlotTags.Num();
    OutResult.InstanceIds.SetNum(SlotCount);
    OutResult.ItemIconCodes.SetNum(SlotCount);
    OutResult.Labels.Reserve(SlotCount);
    OutResult.ShortLabels.Reserve(SlotCount);
    OutResult.ItemLabels.Reserve(SlotCount);

    for (int32 Index = 0; Index < SlotCount; ++Index)
    {
        OutResult.InstanceIds[Index] = 0;
    }

    for (int32 Index = 0; Index < SlotCount; ++Index)
    {
        const FGameplayTag SlotTag = OutResult.SlotTags[Index];
        const FInventoryEntryView* FoundEntry = nullptr;

        for (const FInventoryEntryView& Entry : Entries)
        {
            if (Entry.EquippedSlot == SlotTag)
            {
                FoundEntry = &Entry;
                break;
            }
        }

        // Build short name from tag (e.g., "Item.EquipmentSlot.MainHand" -> "MainHand")
        FString SlotName = SlotTag.IsValid() ? SlotTag.ToString() : TEXT("Slot");
        FString SlotShortName = SlotName;
        int32 DotIndex = INDEX_NONE;
        if (SlotShortName.FindLastChar(TEXT('.'), DotIndex))
        {
            SlotShortName = SlotShortName.Mid(DotIndex + 1);
        }

        FString ItemName = TEXT("Empty");
        if (FoundEntry)
        {
            ItemName = FoundEntry->DisplayName.IsEmpty()
                ? FoundEntry->ItemId.ToString()
                : FoundEntry->DisplayName.ToString();
            OutResult.InstanceIds[Index] = FoundEntry->InstanceId;
            OutResult.ItemIconCodes[Index] = FoundEntry->IconCode;
        }

        OutResult.Labels.Add(FText::FromString(FString::Printf(TEXT("%s: %s"), *SlotName, *ItemName)));
        OutResult.ShortLabels.Add(FText::FromString(SlotShortName));
        OutResult.ItemLabels.Add(FText::FromString(ItemName));
    }
}
