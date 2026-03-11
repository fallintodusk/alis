// Copyright ALIS. All Rights Reserved.

#include "Helpers/InventorySaveHelper.h"
#include "Inventory/InventoryTypes.h"
#include "Types/EquippedItemData.h"
#include "Inventory/InventorySaveData.h"
#include "ProjectSaveSubsystem.h"
#include "GameFramework/Pawn.h"

void FInventorySaveHelper::BuildSaveData(
    const TArray<FInventoryEntry>& Entries,
    const TMap<FGameplayTag, FEquippedItemData>& EquippedItems,
    FInventorySaveData& OutData)
{
    OutData.Entries.Reset();
    OutData.Entries.Reserve(Entries.Num());

    for (const FInventoryEntry& Entry : Entries)
    {
        FInventoryEntrySaveData SaveEntry;
        SaveEntry.InstanceId = Entry.InstanceId;
        SaveEntry.ItemId = Entry.ItemId;
        SaveEntry.Quantity = Entry.Quantity;
        SaveEntry.ContainerId = Entry.ContainerId;
        SaveEntry.GridPos = Entry.GridPos;
        SaveEntry.bRotated = Entry.bRotated;
        SaveEntry.InstanceData = Entry.InstanceData;
        SaveEntry.OverrideMagnitudes = Entry.OverrideMagnitudes;
        OutData.Entries.Add(SaveEntry);
    }

    OutData.EquippedInstanceIds.Reset();
    for (const auto& Pair : EquippedItems)
    {
        OutData.EquippedInstanceIds.Add(Pair.Key, static_cast<int32>(Pair.Value.InstanceId));
    }
}

void FInventorySaveHelper::ApplyLoadData(
    const FInventorySaveData& Data,
    bool bApplyEquippedAbilities,
    const FLoadCallbacks& Callbacks)
{
    Callbacks.ResetState();

    for (const FInventoryEntrySaveData& SaveEntry : Data.Entries)
    {
        const int32 SlotIndex = Callbacks.ComputeSlotIndex(SaveEntry.ContainerId, SaveEntry.GridPos);
        Callbacks.AddEntry(SaveEntry, SlotIndex);
    }

    for (const auto& Pair : Data.EquippedInstanceIds)
    {
        Callbacks.RestoreEquipped(Pair.Value, Pair.Key, bApplyEquippedAbilities);
    }

    Callbacks.BroadcastChange();
}

bool FInventorySaveHelper::SaveToSubsystem(
    UProjectSaveSubsystem* Subsystem,
    FName SaveKey,
    const FInventorySaveData& Data)
{
    if (!Subsystem)
    {
        return false;
    }

    const FName EffectiveKey = SaveKey.IsNone() ? GetDefaultSaveKey() : SaveKey;
    TArray<uint8> Bytes;
    if (!SerializeInventorySaveData(Data, Bytes))
    {
        return false;
    }

    return Subsystem->SetPluginBinaryData(EffectiveKey, Bytes);
}

bool FInventorySaveHelper::LoadFromSubsystem(
    UProjectSaveSubsystem* Subsystem,
    FName SaveKey,
    FInventorySaveData& OutData)
{
    if (!Subsystem)
    {
        return false;
    }

    const FName EffectiveKey = SaveKey.IsNone() ? GetDefaultSaveKey() : SaveKey;
    TArray<uint8> Bytes;
    if (!Subsystem->GetPluginBinaryData(EffectiveKey, Bytes))
    {
        return false;
    }

    return DeserializeInventorySaveData(Bytes, OutData);
}

bool FInventorySaveHelper::ShouldUseLocalSave(AActor* Owner, UWorld* World)
{
    if (!Owner || !Owner->HasAuthority())
    {
        return false;
    }

    if (!World)
    {
        return false;
    }

    // Only persist for standalone (single-player) sessions
    if (World->GetNetMode() != NM_Standalone)
    {
        return false;
    }

    const APawn* PawnOwner = Cast<APawn>(Owner);
    if (!PawnOwner || !PawnOwner->IsPlayerControlled())
    {
        return false;
    }

    return true;
}
