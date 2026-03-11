// Copyright ALIS. All Rights Reserved.

#include "MVVM/InventoryViewModel.h"
#include "MVVM/InventoryViewModelCellBuilder.h"
#include "MVVM/InventoryViewModelEquipSlotBuilder.h"
#include "ProjectGameplayTags.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UObject/PrimaryAssetId.h"

DEFINE_LOG_CATEGORY_STATIC(LogInventoryVM, Log, All);

namespace
{
const FName NAME_ActionUse(TEXT("Use"));
const FName NAME_ActionEquip(TEXT("Equip"));
const FName NAME_ActionDrop(TEXT("Drop"));
const FName NAME_ActionSplit(TEXT("Split"));

bool IsHandContainerTag(const FGameplayTag& ContainerId)
{
    return ContainerId == ProjectTags::Item_Container_Hands
        || ContainerId == ProjectTags::Item_Container_LeftHand
        || ContainerId == ProjectTags::Item_Container_RightHand;
}

bool IsPocketContainerTag(const FGameplayTag& ContainerId)
{
    return ContainerId == ProjectTags::Item_Container_Pockets1
        || ContainerId == ProjectTags::Item_Container_Pockets2
        || ContainerId == ProjectTags::Item_Container_Pockets3
        || ContainerId == ProjectTags::Item_Container_Pockets4;
}

int32 GetPocketOrderKey(const FGameplayTag& ContainerId)
{
    if (ContainerId == ProjectTags::Item_Container_Pockets1) { return 0; }
    if (ContainerId == ProjectTags::Item_Container_Pockets2) { return 1; }
    if (ContainerId == ProjectTags::Item_Container_Pockets3) { return 2; }
    if (ContainerId == ProjectTags::Item_Container_Pockets4) { return 3; }
    return 100;
}

bool IsBackpackContainerTag(const FGameplayTag& ContainerId)
{
    return ContainerId == ProjectTags::Item_Container_Backpack;
}

FString MakeContainerShortName(const FGameplayTag& Tag)
{
    FString Name = Tag.IsValid() ? Tag.ToString() : TEXT("Container");
    int32 DotIndex = INDEX_NONE;
    if (Name.FindLastChar(TEXT('.'), DotIndex))
    {
        Name = Name.Mid(DotIndex + 1);
    }
    int32 ColonIndex = INDEX_NONE;
    if (Name.FindLastChar(TEXT(':'), ColonIndex))
    {
        Name = Name.Mid(ColonIndex + 1);
    }
    return Name;
}

void AddActionDescriptor(
    TArray<FProjectUIActionDescriptor>& OutActions,
    FName ActionId,
    const FText& Label,
    bool bVisible,
    bool bEnabled,
    int32 Priority)
{
    FProjectUIActionDescriptor Descriptor;
    Descriptor.ActionId = ActionId;
    Descriptor.Label = Label;
    Descriptor.bVisible = bVisible;
    Descriptor.bEnabled = bEnabled;
    Descriptor.Priority = Priority;
    OutActions.Add(MoveTemp(Descriptor));
}

struct FInventoryActionCapabilityState
{
    bool bCanUse = false;
    bool bCanEquip = false;
    bool bCanDrop = false;
    bool bCanSplit = false;
};

FInventoryActionCapabilityState BuildActionCapabilityState(const FInventoryEntryView& Entry)
{
    FInventoryActionCapabilityState CapabilityState;

    // SOT: action visibility/enabling must come from explicit producer flags only.
    // Do not reintroduce legacy inference from bIsConsumable or EquipSlotTag here.
    CapabilityState.bCanUse = Entry.bCanUse;
    CapabilityState.bCanEquip = Entry.bCanEquip;
    if (!Entry.bActionCapsPopulated)
    {
        static bool bLoggedMissingCapsMarker = false;
        if (!bLoggedMissingCapsMarker)
        {
            UE_LOG(
                LogInventoryVM,
                Warning,
                TEXT("BuildActionCapabilityState: entry producer did not set bActionCapsPopulated; capabilities default to explicit values only."));
            bLoggedMissingCapsMarker = true;
        }
    }

    CapabilityState.bCanDrop = Entry.bCanBeDropped;
    CapabilityState.bCanSplit = Entry.MaxStack > 1 && Entry.Quantity > 1;

    // Current UX contract: "Use" and "Equip" are mutually exclusive in menu/buttons.
    if (CapabilityState.bCanUse && CapabilityState.bCanEquip)
    {
        CapabilityState.bCanEquip = false;
    }

    return CapabilityState;
}
}

void UInventoryViewModel::Initialize(UObject* Context)
{
    Super::Initialize(Context);

    if (APawn* Pawn = Cast<APawn>(Context))
    {
        SetInventorySource(FindInventorySourceFromActor(Pawn));
        return;
    }

    if (APlayerController* PC = Cast<APlayerController>(Context))
    {
        if (APawn* Pawn = PC->GetPawn())
        {
            SetInventorySource(FindInventorySourceFromActor(Pawn));
        }
    }
}

void UInventoryViewModel::Shutdown()
{
    if (InventorySource)
    {
        InventorySource->OnInventoryViewChanged().RemoveAll(this);
        InventorySource->OnInventoryErrorNative().RemoveAll(this);
    }

    InventorySource.SetObject(nullptr);
    InventorySource.SetInterface(nullptr);
    InventoryCommands.SetObject(nullptr);
    InventoryCommands.SetInterface(nullptr);
    CachedEntries.Reset();
    Super::Shutdown();
}

void UInventoryViewModel::SetInventorySource(UObject* InObject)
{
    if (InventorySource.GetObject() == InObject)
    {
        return;
    }

    if (!InObject || !InObject->GetClass()->ImplementsInterface(UInventoryReadOnly::StaticClass()))
    {
        if (InventorySource)
        {
            InventorySource->OnInventoryViewChanged().RemoveAll(this);
            InventorySource->OnInventoryErrorNative().RemoveAll(this);
        }

        InventorySource.SetObject(nullptr);
        InventorySource.SetInterface(nullptr);
        InventoryCommands.SetObject(nullptr);
        InventoryCommands.SetInterface(nullptr);
        RefreshFromInventory();
        return;
    }

    if (InventorySource)
    {
        InventorySource->OnInventoryViewChanged().RemoveAll(this);
        InventorySource->OnInventoryErrorNative().RemoveAll(this);
    }

    InventorySource.SetObject(InObject);
    InventorySource.SetInterface(InObject ? Cast<IInventoryReadOnly>(InObject) : nullptr);
    InventoryCommands.SetObject(InObject);
    InventoryCommands.SetInterface(InObject->GetClass()->ImplementsInterface(UInventoryCommands::StaticClass())
        ? Cast<IInventoryCommands>(InObject)
        : nullptr);

    if (InventorySource)
    {
        InventorySource->OnInventoryViewChanged().AddUObject(this, &UInventoryViewModel::HandleInventoryViewChanged);
        InventorySource->OnInventoryErrorNative().AddUObject(this, &UInventoryViewModel::HandleInventoryErrorFromSource);
    }

    RefreshFromInventory();
}

void UInventoryViewModel::RequestAddItem(FPrimaryAssetId ObjectId, int32 Quantity)
{
    if (InventoryCommands)
    {
        InventoryCommands->RequestAddItem(ObjectId, Quantity);
    }
}

void UInventoryViewModel::RequestRemoveItem(int32 InstanceId, int32 Quantity)
{
    if (InventoryCommands)
    {
        InventoryCommands->RequestRemoveItem(InstanceId, Quantity);
    }
}

void UInventoryViewModel::RequestMoveItem(int32 InstanceId, FGameplayTag FromContainer, FIntPoint FromPos, FGameplayTag ToContainer, FIntPoint ToPos, int32 Quantity, bool bRotated)
{
    if (InventoryCommands)
    {
        InventoryCommands->RequestMoveItem(InstanceId, FromContainer, FromPos, ToContainer, ToPos, Quantity, bRotated);
    }
}

void UInventoryViewModel::RequestUseItem(int32 InstanceId)
{
    if (InventoryCommands)
    {
        InventoryCommands->RequestUseItem(InstanceId);
    }
}

void UInventoryViewModel::RequestEquipItem(int32 InstanceId, FGameplayTag EquipSlot)
{
    if (InventoryCommands)
    {
        InventoryCommands->RequestEquipItem(InstanceId, EquipSlot);
    }
}

void UInventoryViewModel::RequestEquipItemAuto(int32 InstanceId)
{
    FInventoryEntryView Entry;
    if (TryGetEntryByInstanceId(InstanceId, Entry) && Entry.EquipSlotTag.IsValid())
    {
        RequestEquipItem(InstanceId, Entry.EquipSlotTag);
    }
}

void UInventoryViewModel::RequestUnequipItem(FGameplayTag EquipSlot)
{
    if (InventoryCommands)
    {
        InventoryCommands->RequestUnequipItem(EquipSlot);
    }
}

void UInventoryViewModel::RequestDropItem(int32 InstanceId, int32 Quantity)
{
    if (InventoryCommands)
    {
        InventoryCommands->RequestDropItem(InstanceId, Quantity);
    }
}

void UInventoryViewModel::RequestSplitStack(int32 InstanceId, int32 SplitQuantity)
{
    if (InventoryCommands)
    {
        InventoryCommands->RequestSplitStack(InstanceId, SplitQuantity);
    }
}

void UInventoryViewModel::RequestSplitStackHalf(int32 InstanceId)
{
    FInventoryEntryView Entry;
    if (TryGetEntryByInstanceId(InstanceId, Entry) && Entry.Quantity > 1)
    {
        RequestSplitStack(InstanceId, Entry.Quantity / 2);
    }
}

bool UInventoryViewModel::HasCommands() const
{
    return InventoryCommands != nullptr;
}

void UInventoryViewModel::BuildActionDescriptors(const FInventoryEntryView& Entry, TArray<FProjectUIActionDescriptor>& OutActions) const
{
    // SOT guardrail:
    // Action visibility and enabling rules must be authored only in this function.
    // Widgets consume descriptors and must not re-implement business rules.
    const bool bCanSendCommands = HasCommands();
    const FInventoryActionCapabilityState CapabilityState = BuildActionCapabilityState(Entry);

    OutActions.Reset();
    OutActions.Reserve(4);

    AddActionDescriptor(
        OutActions,
        NAME_ActionUse,
        NSLOCTEXT("Inventory", "ActionUse", "Use"),
        CapabilityState.bCanUse,
        CapabilityState.bCanUse && bCanSendCommands,
        100);

    // Show "Unequip" when item is currently equipped, "Equip" otherwise
    const bool bIsEquipped = Entry.EquippedSlot.IsValid();
    const bool bShowEquipAction = bIsEquipped || CapabilityState.bCanEquip;
    AddActionDescriptor(
        OutActions,
        NAME_ActionEquip,
        bIsEquipped ? NSLOCTEXT("Inventory", "ActionUnequip", "Unequip")
                    : NSLOCTEXT("Inventory", "ActionEquip", "Equip"),
        bShowEquipAction,
        bShowEquipAction && bCanSendCommands,
        90);

    AddActionDescriptor(
        OutActions,
        NAME_ActionDrop,
        NSLOCTEXT("Inventory", "ActionDrop", "Drop"),
        CapabilityState.bCanDrop,
        CapabilityState.bCanDrop && bCanSendCommands,
        80);

    AddActionDescriptor(
        OutActions,
        NAME_ActionSplit,
        NSLOCTEXT("Inventory", "ActionSplit", "Split"),
        CapabilityState.bCanSplit,
        CapabilityState.bCanSplit && bCanSendCommands,
        70);
}

bool UInventoryViewModel::TryGetActionDescriptorsByInstanceId(int32 InstanceId, TArray<FProjectUIActionDescriptor>& OutActions) const
{
    FInventoryEntryView Entry;
    if (!TryGetEntryByInstanceId(InstanceId, Entry))
    {
        OutActions.Reset();
        return false;
    }

    BuildActionDescriptors(Entry, OutActions);
    return true;
}

const FName& UInventoryViewModel::GetActionIdUse()
{
    return NAME_ActionUse;
}

const FName& UInventoryViewModel::GetActionIdEquip()
{
    return NAME_ActionEquip;
}

const FName& UInventoryViewModel::GetActionIdDrop()
{
    return NAME_ActionDrop;
}

const FName& UInventoryViewModel::GetActionIdSplit()
{
    return NAME_ActionSplit;
}

const FProjectUIActionDescriptor* UInventoryViewModel::FindActionDescriptor(const TArray<FProjectUIActionDescriptor>& Actions, FName ActionId)
{
    for (const FProjectUIActionDescriptor& Descriptor : Actions)
    {
        if (Descriptor.ActionId == ActionId)
        {
            return &Descriptor;
        }
    }
    return nullptr;
}

bool UInventoryViewModel::IsActionEnabled(const TArray<FProjectUIActionDescriptor>& Actions, FName ActionId)
{
    const FProjectUIActionDescriptor* Descriptor = FindActionDescriptor(Actions, ActionId);
    return Descriptor && Descriptor->bVisible && Descriptor->bEnabled;
}

bool UInventoryViewModel::HasEnabledActions(const TArray<FProjectUIActionDescriptor>& Actions)
{
    for (const FProjectUIActionDescriptor& Descriptor : Actions)
    {
        if (Descriptor.bVisible && Descriptor.bEnabled)
        {
            return true;
        }
    }
    return false;
}

void UInventoryViewModel::HandleInventoryViewChanged()
{
    RefreshFromInventory();
}

void UInventoryViewModel::HandleInventoryErrorFromSource(const FText& ErrorMessage)
{
    // Forward to listeners (e.g., W_InventoryPanel for toast display)
    OnInventoryError.Broadcast(ErrorMessage);
}

void UInventoryViewModel::RefreshFromInventory()
{
    if (!InventorySource)
    {
        UE_LOG(LogInventoryVM, Warning, TEXT("RefreshFromInventory: No InventorySource"));
        UpdateItemCount(0);
        UpdateCurrentWeight(0.f);
        UpdateMaxWeight(0.f);
        UpdateCurrentVolume(0.f);
        UpdateMaxVolume(0.f);
        UpdateGridWidth(0);
        UpdateGridHeight(0);
        UpdateSecondaryGridWidth(0);
        UpdateSecondaryGridHeight(0);
        UpdateContainerLabels(TArray<FText>());
        UpdatePocketContainerLabels(TArray<FText>());
        UpdateSelectedContainerIndex(INDEX_NONE);
        UpdateSecondaryContainerIndex(INDEX_NONE);
        UpdateEquipSlotLabels(TArray<FText>());
        CachedAllContainers.Reset();
        CachedEntries.Reset();
        CachedContainers.Reset();
        CachedPocketContainers.Reset();
        CellInstanceIds.Reset();
        CellEnabled.Reset();
        SecondaryCellInstanceIds.Reset();
        SecondaryCellEnabled.Reset();
        PocketCellTexts.Reset();
        PocketCellInstanceIds.Reset();
        PocketCellEnabled.Reset();
        EquipSlotTags.Reset();
        EquipSlotInstanceIds.Reset();
        SetCellTexts(TArray<FText>());
        SetSecondaryCellTexts(TArray<FText>());
        NotifyPocketCellTextsChanged();
        return;
    }

    TArray<FInventoryEntryView> Entries;
    InventorySource->GetEntriesView(Entries);
    CachedEntries = Entries;

    TArray<FInventoryContainerView> Containers;
    InventorySource->GetContainersView(Containers);
    CachedAllContainers = Containers;

    UE_LOG(LogInventoryVM, Log, TEXT("RefreshFromInventory: %d entries, %d containers"), Entries.Num(), Containers.Num());
    for (const FInventoryContainerView& C : Containers)
    {
        UE_LOG(LogInventoryVM, Log, TEXT("  Container: %s (%dx%d)"), *C.ContainerId.ToString(), C.GridSize.X, C.GridSize.Y);
    }
    for (const FInventoryEntryView& E : Entries)
    {
        UE_LOG(LogInventoryVM, Log, TEXT("  Entry: %s x%d in %s at (%d,%d)"),
            *E.ItemId.ToString(), E.Quantity, *E.ContainerId.ToString(), E.GridPos.X, E.GridPos.Y);
    }

    BuildContainerData(Containers);

    UpdateItemCount(Entries.Num());
    UpdateCurrentWeight(InventorySource->GetCurrentWeight());
    UpdateMaxWeight(InventorySource->GetMaxWeight());
    UpdateCurrentVolume(InventorySource->GetCurrentVolume());
    UpdateMaxVolume(InventorySource->GetMaxVolume());
    BuildCellTexts(Entries);
    BuildSecondaryCellTexts(Entries);
    BuildHandCellTexts(Entries);
    BuildPocketCellTexts(Entries);
    BuildEquipSlotLabels(Entries);

    UE_LOG(LogInventoryVM, Log, TEXT("RefreshFromInventory: Grid=%dx%d, StorageContainers=%d, LHand=%d texts, RHand=%d texts"),
        GridWidth, GridHeight, CachedContainers.Num(), LeftHandCellTexts.Num(), RightHandCellTexts.Num());
}

void UInventoryViewModel::TogglePanel()
{
    if (!bPanelVisible)
    {
        RefreshFromInventory();
    }
    UpdatebPanelVisible(!bPanelVisible);
}

void UInventoryViewModel::ShowPanel()
{
    RefreshFromInventory();
    UpdatebPanelVisible(true);
}

void UInventoryViewModel::HidePanel()
{
    UpdatebPanelVisible(false);
}

void UInventoryViewModel::BuildContainerData(const TArray<FInventoryContainerView>& Containers)
{
    // Layout SOT (Plugins/Features/ProjectInventory/docs/design_vision.md):
    // - Hands are always dedicated left/right grids.
    // - Pockets1..4 are rendered as compact top-row grids near hands.
    // - Backpack and other large containers are rendered in the lower storage row.
    TArray<FInventoryContainerView> StorageContainers;
    StorageContainers.Reserve(Containers.Num());
    for (const FInventoryContainerView& Container : Containers)
    {
        if (IsHandContainerTag(Container.ContainerId))
        {
            UE_LOG(LogInventoryVM, Log, TEXT("BuildContainerData: Skipping hand container %s (dedicated grid)"),
                *Container.ContainerId.ToString());
            continue;
        }
        StorageContainers.Add(Container);
    }

    TArray<FInventoryContainerView> PocketContainers;
    TArray<FInventoryContainerView> LargeContainers;
    PocketContainers.Reserve(StorageContainers.Num());
    LargeContainers.Reserve(StorageContainers.Num());

    for (const FInventoryContainerView& Container : StorageContainers)
    {
        if (IsPocketContainerTag(Container.ContainerId))
        {
            PocketContainers.Add(Container);
        }
        else
        {
            LargeContainers.Add(Container);
        }
    }

    PocketContainers.Sort([](const FInventoryContainerView& A, const FInventoryContainerView& B)
    {
        return GetPocketOrderKey(A.ContainerId) < GetPocketOrderKey(B.ContainerId);
    });

    LargeContainers.Sort([](const FInventoryContainerView& A, const FInventoryContainerView& B)
    {
        const bool bAIsBackpack = IsBackpackContainerTag(A.ContainerId);
        const bool bBIsBackpack = IsBackpackContainerTag(B.ContainerId);
        if (bAIsBackpack != bBIsBackpack)
        {
            return bAIsBackpack;
        }
        return A.ContainerId.ToString() < B.ContainerId.ToString();
    });

    UE_LOG(LogInventoryVM, Log, TEXT("BuildContainerData: %d total -> %d pocket, %d large storage containers"),
        Containers.Num(), PocketContainers.Num(), LargeContainers.Num());

    CachedPocketContainers = PocketContainers;
    CachedContainers = LargeContainers;

    auto BuildCellEnabled = [](const FInventoryContainerView& Container, TArray<bool>& OutEnabled)
    {
        const int32 CellCount = Container.GridSize.X * Container.GridSize.Y;
        OutEnabled.SetNum(CellCount);
        for (int32 Index = 0; Index < CellCount; ++Index)
        {
            OutEnabled[Index] = true;
        }
        if (Container.MaxCells > 0)
        {
            for (int32 Index = Container.MaxCells; Index < CellCount; ++Index)
            {
                OutEnabled[Index] = false;
            }
        }
    };

    TArray<FText> PocketLabels;
    PocketLabels.Reserve(PocketContainers.Num());
    PocketCellEnabled.SetNum(PocketContainers.Num());
    for (int32 PocketIndex = 0; PocketIndex < PocketContainers.Num(); ++PocketIndex)
    {
        const FInventoryContainerView& PocketContainer = PocketContainers[PocketIndex];
        PocketLabels.Add(FText::FromString(MakeContainerShortName(PocketContainer.ContainerId)));
        BuildCellEnabled(PocketContainer, PocketCellEnabled[PocketIndex]);
    }
    UpdatePocketContainerLabels(PocketLabels);

    TArray<FText> Labels;
    Labels.Reserve(LargeContainers.Num());
    for (const FInventoryContainerView& Container : LargeContainers)
    {
        Labels.Add(FText::FromString(MakeContainerShortName(Container.ContainerId)));
    }
    UpdateContainerLabels(Labels);

    int32 PrimaryIndex = SelectedContainerIndex;
    if (PrimaryIndex < 0 || PrimaryIndex >= LargeContainers.Num())
    {
        PrimaryIndex = (LargeContainers.Num() > 0) ? 0 : INDEX_NONE;
    }
    if (PrimaryIndex != SelectedContainerIndex)
    {
        UpdateSelectedContainerIndex(PrimaryIndex);
    }

    int32 SecondaryIndex = SecondaryContainerIndex;
    if (SecondaryIndex < 0 || SecondaryIndex >= LargeContainers.Num() || SecondaryIndex == PrimaryIndex)
    {
        if (LargeContainers.Num() > 1)
        {
            SecondaryIndex = (PrimaryIndex == 0) ? 1 : 0;
        }
        else
        {
            SecondaryIndex = INDEX_NONE;
        }
    }
    if (SecondaryIndex != SecondaryContainerIndex)
    {
        UpdateSecondaryContainerIndex(SecondaryIndex);
    }

    if (!LargeContainers.IsValidIndex(PrimaryIndex))
    {
        UpdateGridWidth(0);
        UpdateGridHeight(0);
        UpdateContainerCurrentWeight(0.f);
        UpdateContainerMaxWeight(0.f);
        UpdateContainerCurrentVolume(0.f);
        UpdateContainerMaxVolume(0.f);
        CellEnabled.Reset();
    }
    else
    {
        const FInventoryContainerView& Primary = LargeContainers[PrimaryIndex];
        UpdateGridWidth(Primary.GridSize.X);
        UpdateGridHeight(Primary.GridSize.Y);
        UpdateContainerCurrentWeight(Primary.CurrentWeight);
        UpdateContainerMaxWeight(Primary.MaxWeight);
        UpdateContainerCurrentVolume(Primary.CurrentVolume);
        UpdateContainerMaxVolume(Primary.MaxVolume);
        BuildCellEnabled(Primary, CellEnabled);
    }

    if (!LargeContainers.IsValidIndex(SecondaryIndex))
    {
        UpdateSecondaryGridWidth(0);
        UpdateSecondaryGridHeight(0);
        SecondaryCellEnabled.Reset();
    }
    else
    {
        const FInventoryContainerView& Secondary = LargeContainers[SecondaryIndex];
        UpdateSecondaryGridWidth(Secondary.GridSize.X);
        UpdateSecondaryGridHeight(Secondary.GridSize.Y);
        BuildCellEnabled(Secondary, SecondaryCellEnabled);
    }
}

// Cell text building delegated to FInventoryViewModelCellBuilder (SOLID)
void UInventoryViewModel::BuildCellTexts(const TArray<FInventoryEntryView>& Entries)
{
    TArray<FText> NewTexts;
    const FGameplayTag ContainerId = CachedContainers.IsValidIndex(SelectedContainerIndex)
        ? CachedContainers[SelectedContainerIndex].ContainerId : FGameplayTag();
    FInventoryViewModelCellBuilder::Build(Entries, ContainerId, GridWidth, GridHeight, CellInstanceIds, NewTexts);
    SetCellTexts(NewTexts);
}

void UInventoryViewModel::BuildSecondaryCellTexts(const TArray<FInventoryEntryView>& Entries)
{
    TArray<FText> NewTexts;
    const FGameplayTag ContainerId = CachedContainers.IsValidIndex(SecondaryContainerIndex)
        ? CachedContainers[SecondaryContainerIndex].ContainerId : FGameplayTag();
    FInventoryViewModelCellBuilder::Build(Entries, ContainerId, SecondaryGridWidth, SecondaryGridHeight, SecondaryCellInstanceIds, NewTexts);
    SetSecondaryCellTexts(NewTexts);
}

void UInventoryViewModel::BuildPocketCellTexts(const TArray<FInventoryEntryView>& Entries)
{
    PocketCellTexts.SetNum(CachedPocketContainers.Num());
    PocketCellInstanceIds.SetNum(CachedPocketContainers.Num());

    for (int32 PocketIndex = 0; PocketIndex < CachedPocketContainers.Num(); ++PocketIndex)
    {
        const FInventoryContainerView& PocketContainer = CachedPocketContainers[PocketIndex];
        FInventoryViewModelCellBuilder::Build(
            Entries,
            PocketContainer.ContainerId,
            PocketContainer.GridSize.X,
            PocketContainer.GridSize.Y,
            PocketCellInstanceIds[PocketIndex],
            PocketCellTexts[PocketIndex]);
    }

    NotifyPocketCellTextsChanged();
}

void UInventoryViewModel::NotifyPocketCellTextsChanged()
{
    NotifyPropertyChanged(FName(TEXT("PocketCellTexts")));
}

void UInventoryViewModel::BuildHandCellTexts(const TArray<FInventoryEntryView>& Entries)
{
    // Try dedicated LeftHand/RightHand containers first (from equip slot grants)
    TArray<FText> LeftTexts;
    TArray<int32> LeftIds;
    FInventoryViewModelCellBuilder::Build(Entries, ProjectTags::Item_Container_LeftHand,
        HandGridSize, HandGridSize, LeftIds, LeftTexts);

    TArray<FText> RightTexts;
    TArray<int32> RightIds;
    FInventoryViewModelCellBuilder::Build(Entries, ProjectTags::Item_Container_RightHand,
        HandGridSize, HandGridSize, RightIds, RightTexts);

    // Also handle the generic "Hands" container (slot-based 2x1: X=0 -> left, X=1 -> right)
    // This is the default container when no equipment grants LeftHand/RightHand
    for (const FInventoryEntryView& Entry : Entries)
    {
        if (Entry.ContainerId != ProjectTags::Item_Container_Hands)
        {
            continue;
        }
        if (Entry.InstanceId <= 0)
        {
            continue;
        }

        const FString Label = !Entry.IconCode.IsEmpty()
            ? Entry.IconCode
            : FInventoryViewModelCellBuilder::BuildEntryLabel(Entry.DisplayName, Entry.Quantity, Entry.ItemId);

        // Ensure arrays are sized
        if (LeftTexts.Num() < HandCellCount)
        {
            LeftTexts.SetNum(HandCellCount);
            LeftIds.Init(EmptyCellInstanceId, HandCellCount);
        }
        if (RightTexts.Num() < HandCellCount)
        {
            RightTexts.SetNum(HandCellCount);
            RightIds.Init(EmptyCellInstanceId, HandCellCount);
        }

        // Slot X=0 -> left hand, X=1 -> right hand
        TArray<FText>& TargetTexts = (Entry.GridPos.X == 0) ? LeftTexts : RightTexts;
        TArray<int32>& TargetIds = (Entry.GridPos.X == 0) ? LeftIds : RightIds;
        if (TargetTexts[0].IsEmpty())
        {
            TargetTexts[0] = FText::FromString(Label);
            TargetIds[0] = Entry.InstanceId;
        }

        // Log hex for PUA codepoints - raw PUA chars in Output Log trigger Slate glyph warnings
        const FString SafeLabel = (!Label.IsEmpty() && Label[0] >= 0xE000)
            ? FString::Printf(TEXT("U+%04X"), static_cast<uint32>(Label[0]))
            : Label;
        UE_LOG(LogInventoryVM, Log, TEXT("BuildHandCellTexts: Hands entry '%s' -> %s hand"),
            *SafeLabel, (Entry.GridPos.X == 0) ? TEXT("Left") : TEXT("Right"));
    }

    SetLeftHandCellTexts(LeftTexts);
    SetLeftHandCellInstanceIds(LeftIds);
    SetRightHandCellTexts(RightTexts);
    SetRightHandCellInstanceIds(RightIds);

    UE_LOG(LogInventoryVM, Log, TEXT("BuildHandCellTexts: L=%d texts, R=%d texts"),
        LeftTexts.Num(), RightTexts.Num());
}

UObject* UInventoryViewModel::FindInventorySourceFromActor(AActor* Actor) const
{
    if (!Actor)
    {
        return nullptr;
    }

    TInlineComponentArray<UActorComponent*> Components;
    Actor->GetComponents(Components);

    for (UActorComponent* Component : Components)
    {
        if (Component && Component->GetClass()->ImplementsInterface(UInventoryReadOnly::StaticClass()))
        {
            return Component;
        }
    }

    return nullptr;
}

void UInventoryViewModel::SetSelectedContainerIndex(int32 NewIndex)
{
    if (NewIndex == SelectedContainerIndex)
    {
        return;
    }

    UpdateSelectedContainerIndex(NewIndex);
    BuildContainerData(CachedAllContainers);
    BuildCellTexts(CachedEntries);
    BuildSecondaryCellTexts(CachedEntries);
    BuildPocketCellTexts(CachedEntries);
}

void UInventoryViewModel::SetSecondaryContainerIndex(int32 NewIndex)
{
    if (NewIndex == SecondaryContainerIndex)
    {
        return;
    }

    UpdateSecondaryContainerIndex(NewIndex);
    BuildContainerData(CachedAllContainers);
    BuildCellTexts(CachedEntries);
    BuildSecondaryCellTexts(CachedEntries);
    BuildPocketCellTexts(CachedEntries);
}

FGameplayTag UInventoryViewModel::GetSelectedContainerId() const
{
    if (!CachedContainers.IsValidIndex(SelectedContainerIndex))
    {
        return FGameplayTag();
    }

    return CachedContainers[SelectedContainerIndex].ContainerId;
}

FGameplayTag UInventoryViewModel::GetSecondaryContainerId() const
{
    if (!CachedContainers.IsValidIndex(SecondaryContainerIndex))
    {
        return FGameplayTag();
    }

    return CachedContainers[SecondaryContainerIndex].ContainerId;
}

int32 UInventoryViewModel::GetPocketContainerCount() const
{
    return CachedPocketContainers.Num();
}

FText UInventoryViewModel::GetPocketContainerLabel(int32 PocketIndex) const
{
    return PocketContainerLabels.IsValidIndex(PocketIndex)
        ? PocketContainerLabels[PocketIndex]
        : FText::GetEmpty();
}

FGameplayTag UInventoryViewModel::GetPocketContainerId(int32 PocketIndex) const
{
    return CachedPocketContainers.IsValidIndex(PocketIndex)
        ? CachedPocketContainers[PocketIndex].ContainerId
        : FGameplayTag();
}

int32 UInventoryViewModel::GetPocketGridWidth(int32 PocketIndex) const
{
    return CachedPocketContainers.IsValidIndex(PocketIndex)
        ? CachedPocketContainers[PocketIndex].GridSize.X
        : 0;
}

int32 UInventoryViewModel::GetPocketGridHeight(int32 PocketIndex) const
{
    return CachedPocketContainers.IsValidIndex(PocketIndex)
        ? CachedPocketContainers[PocketIndex].GridSize.Y
        : 0;
}

const TArray<FText>& UInventoryViewModel::GetPocketCellTexts(int32 PocketIndex) const
{
    static const TArray<FText> EmptyTexts;
    return PocketCellTexts.IsValidIndex(PocketIndex) ? PocketCellTexts[PocketIndex] : EmptyTexts;
}

int32 UInventoryViewModel::GetPocketCellInstanceId(int32 PocketIndex, int32 CellIndex) const
{
    if (!PocketCellInstanceIds.IsValidIndex(PocketIndex))
    {
        return EmptyCellInstanceId;
    }
    const TArray<int32>& PocketIds = PocketCellInstanceIds[PocketIndex];
    return PocketIds.IsValidIndex(CellIndex) ? PocketIds[CellIndex] : EmptyCellInstanceId;
}

bool UInventoryViewModel::IsPocketCellEnabled(int32 PocketIndex, int32 CellIndex) const
{
    if (!PocketCellEnabled.IsValidIndex(PocketIndex))
    {
        return false;
    }
    const TArray<bool>& EnabledCells = PocketCellEnabled[PocketIndex];
    return EnabledCells.IsValidIndex(CellIndex) ? EnabledCells[CellIndex] : false;
}

bool UInventoryViewModel::TryGetEntryByCellIndex(int32 CellIndex, FInventoryEntryView& OutEntry) const
{
    if (!CellInstanceIds.IsValidIndex(CellIndex))
    {
        return false;
    }

    const int32 InstanceId = CellInstanceIds[CellIndex];
    if (InstanceId <= 0 || InstanceId == EmptyCellInstanceId)
    {
        return false;
    }

    for (const FInventoryEntryView& Entry : CachedEntries)
    {
        if (Entry.InstanceId == InstanceId)
        {
            OutEntry = Entry;
            return true;
        }
    }

    return false;
}

bool UInventoryViewModel::TryGetEntryByInstanceId(int32 InstanceId, FInventoryEntryView& OutEntry) const
{
    if (InstanceId <= 0 || InstanceId == EmptyCellInstanceId)
    {
        return false;
    }

    for (const FInventoryEntryView& Entry : CachedEntries)
    {
        if (Entry.InstanceId == InstanceId)
        {
            OutEntry = Entry;
            return true;
        }
    }

    return false;
}

bool UInventoryViewModel::TryGetSecondaryEntryByCellIndex(int32 CellIndex, FInventoryEntryView& OutEntry) const
{
    if (!SecondaryCellInstanceIds.IsValidIndex(CellIndex))
    {
        return false;
    }

    const int32 InstanceId = SecondaryCellInstanceIds[CellIndex];
    if (InstanceId <= 0 || InstanceId == EmptyCellInstanceId)
    {
        return false;
    }

    for (const FInventoryEntryView& Entry : CachedEntries)
    {
        if (Entry.InstanceId == InstanceId)
        {
            OutEntry = Entry;
            return true;
        }
    }

    return false;
}

bool UInventoryViewModel::IsCellEnabled(int32 CellIndex) const
{
    if (!CellEnabled.IsValidIndex(CellIndex))
    {
        return false;
    }

    return CellEnabled[CellIndex];
}

bool UInventoryViewModel::IsSecondaryCellEnabled(int32 CellIndex) const
{
    if (!SecondaryCellEnabled.IsValidIndex(CellIndex))
    {
        return false;
    }

    return SecondaryCellEnabled[CellIndex];
}

int32 UInventoryViewModel::GetCellInstanceId(int32 CellIndex) const
{
    return CellInstanceIds.IsValidIndex(CellIndex) ? CellInstanceIds[CellIndex] : EmptyCellInstanceId;
}

int32 UInventoryViewModel::GetSecondaryCellInstanceId(int32 CellIndex) const
{
    return SecondaryCellInstanceIds.IsValidIndex(CellIndex) ? SecondaryCellInstanceIds[CellIndex] : EmptyCellInstanceId;
}

bool UInventoryViewModel::IsContainerEmpty(FGameplayTag ContainerId, int32 IgnoreInstanceId) const
{
    if (!ContainerId.IsValid())
    {
        return true;
    }

    for (const FInventoryEntryView& Entry : CachedEntries)
    {
        if (Entry.InstanceId == IgnoreInstanceId)
        {
            continue;
        }

        if (Entry.ContainerId == ContainerId)
        {
            return false;
        }
    }

    return true;
}

int32 UInventoryViewModel::GetEquipSlotCount() const
{
    return EquipSlotLabels.Num();
}

FText UInventoryViewModel::GetEquipSlotLabel(int32 Index) const
{
    return EquipSlotLabels.IsValidIndex(Index) ? EquipSlotLabels[Index] : FText::GetEmpty();
}

FText UInventoryViewModel::GetEquipSlotShortLabel(int32 Index) const
{
    return EquipSlotShortLabels.IsValidIndex(Index) ? EquipSlotShortLabels[Index] : FText::GetEmpty();
}

FText UInventoryViewModel::GetEquipSlotItemLabel(int32 Index) const
{
    return EquipSlotItemLabels.IsValidIndex(Index) ? EquipSlotItemLabels[Index] : FText::GetEmpty();
}

FGameplayTag UInventoryViewModel::GetEquipSlotTag(int32 Index) const
{
    return EquipSlotTags.IsValidIndex(Index) ? EquipSlotTags[Index] : FGameplayTag();
}

void UInventoryViewModel::SetEquipSlotTags(const TArray<FGameplayTag>& InTags)
{
    EquipSlotTags = InTags;
    NotifyPropertyChanged(GET_MEMBER_NAME_CHECKED(UInventoryViewModel, EquipSlotTags));
}

void UInventoryViewModel::SetEquipSlotShortLabels(const TArray<FText>& InLabels)
{
    EquipSlotShortLabels = InLabels;
    NotifyPropertyChanged(GET_MEMBER_NAME_CHECKED(UInventoryViewModel, EquipSlotShortLabels));
}

void UInventoryViewModel::SetLeftHandCellTexts(const TArray<FText>& InValue)
{
    LeftHandCellTexts = InValue;
    NotifyPropertyChanged(GET_MEMBER_NAME_CHECKED(UInventoryViewModel, LeftHandCellTexts));
}

void UInventoryViewModel::SetRightHandCellTexts(const TArray<FText>& InValue)
{
    RightHandCellTexts = InValue;
    NotifyPropertyChanged(GET_MEMBER_NAME_CHECKED(UInventoryViewModel, RightHandCellTexts));
}

void UInventoryViewModel::SetLeftHandCellInstanceIds(const TArray<int32>& InValue)
{
    LeftHandCellInstanceIds = InValue;
    NotifyPropertyChanged(GET_MEMBER_NAME_CHECKED(UInventoryViewModel, LeftHandCellInstanceIds));
}

void UInventoryViewModel::SetRightHandCellInstanceIds(const TArray<int32>& InValue)
{
    RightHandCellInstanceIds = InValue;
    NotifyPropertyChanged(GET_MEMBER_NAME_CHECKED(UInventoryViewModel, RightHandCellInstanceIds));
}

int32 UInventoryViewModel::GetEquipSlotInstanceId(int32 Index) const
{
    return EquipSlotInstanceIds.IsValidIndex(Index) ? EquipSlotInstanceIds[Index] : EmptyCellInstanceId;
}

FString UInventoryViewModel::GetEquipSlotItemIconCode(int32 Index) const
{
    return EquipSlotItemIconCodes.IsValidIndex(Index) ? EquipSlotItemIconCodes[Index] : FString();
}

// Equip slot building delegated to FInventoryViewModelEquipSlotBuilder (SOLID)
void UInventoryViewModel::BuildEquipSlotLabels(const TArray<FInventoryEntryView>& Entries)
{
    FInventoryViewModelEquipSlotBuilder::FResult Result;
    FInventoryViewModelEquipSlotBuilder::Build(Entries, Result);

    EquipSlotTags = MoveTemp(Result.SlotTags);
    EquipSlotInstanceIds = MoveTemp(Result.InstanceIds);
    EquipSlotShortLabels = MoveTemp(Result.ShortLabels);
    EquipSlotItemLabels = MoveTemp(Result.ItemLabels);
    EquipSlotItemIconCodes = MoveTemp(Result.ItemIconCodes);
    UpdateEquipSlotLabels(Result.Labels);
    NotifyPropertyChanged(FName(TEXT("EquipSlotItemIconCodes")));
}

