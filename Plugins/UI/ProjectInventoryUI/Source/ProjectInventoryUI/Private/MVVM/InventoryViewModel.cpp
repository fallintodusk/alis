// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "MVVM/InventoryViewModel.h"
#include "MVVM/InventoryViewModelActionRules.h"
#include "MVVM/InventoryViewModelCellBuilder.h"
#include "MVVM/InventoryViewModelEquipSlotBuilder.h"
#include "MVVM/InventoryViewModelPlacement.h"
#include "MVVM/InventoryViewModelSurfaceDispatch.h"
#include "Interaction/ProjectUIGridDragDropController.h"
#include "Interfaces/IInventoryWorldContainerTransferBridge.h"
#include "ProjectGameplayTags.h"
#include "Components/ActorComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UObject/PrimaryAssetId.h"

DEFINE_LOG_CATEGORY_STATIC(LogInventoryVM, Log, All);

namespace
{
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

bool BuildEnabledCellsForContainer(const FInventoryContainerView& Container, TArray<bool>& OutEnabled)
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
    return CellCount > 0;
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

IInventoryWorldContainerTransferBridge* GetWorldContainerTransferBridge(UObject* SourceObject)
{
    return (SourceObject && SourceObject->GetClass()->ImplementsInterface(UInventoryWorldContainerTransferBridge::StaticClass()))
        ? Cast<IInventoryWorldContainerTransferBridge>(SourceObject)
        : nullptr;
}

UObject* ResolveWorldContainerSessionSourceFromActor(AActor* Actor)
{
    if (!Actor)
    {
        return nullptr;
    }

    if (Actor->GetClass()->ImplementsInterface(UWorldContainerSessionSource::StaticClass()))
    {
        return Actor;
    }

    TInlineComponentArray<UActorComponent*> Components;
    Actor->GetComponents(Components);
    for (UActorComponent* Component : Components)
    {
        if (Component && Component->GetClass()->ImplementsInterface(UWorldContainerSessionSource::StaticClass()))
        {
            return Component;
        }
    }

    return nullptr;
}
}

void UInventoryViewModel::Initialize(UObject* Context)
{
    Super::Initialize(Context);
    InitializationContext = Context;

    auto BindFromPlayerController = [this](APlayerController* PlayerController) -> bool
    {
        if (!PlayerController)
        {
            return false;
        }

        if (APawn* Pawn = PlayerController->GetPawn())
        {
            SetInventorySource(FindInventorySourceFromActor(Pawn));
            return true;
        }

        return false;
    };

    if (APawn* Pawn = Cast<APawn>(Context))
    {
        SetInventorySource(FindInventorySourceFromActor(Pawn));
        return;
    }

    if (APlayerController* PC = Cast<APlayerController>(Context))
    {
        if (BindFromPlayerController(PC))
        {
            return;
        }
    }

    if (ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(Context))
    {
        if (BindFromPlayerController(LocalPlayer->GetPlayerController(Context->GetWorld())))
        {
            return;
        }
    }

    if (UGameInstance* GameInstance = Cast<UGameInstance>(Context))
    {
        for (ULocalPlayer* LocalPlayer : GameInstance->GetLocalPlayers())
        {
            if (BindFromPlayerController(LocalPlayer ? LocalPlayer->GetPlayerController(GameInstance->GetWorld()) : nullptr))
            {
                return;
            }
        }
    }
}

UObject* UInventoryViewModel::ResolveInventorySourceFromContext() const
{
    UObject* Context = InitializationContext.Get();
    if (!Context)
    {
        return nullptr;
    }

    auto ResolveFromController = [this](APlayerController* PlayerController) -> UObject*
    {
        if (!PlayerController)
        {
            return nullptr;
        }

        if (APawn* Pawn = PlayerController->GetPawn())
        {
            return FindInventorySourceFromActor(Pawn);
        }

        return nullptr;
    };

    if (APawn* Pawn = Cast<APawn>(Context))
    {
        return FindInventorySourceFromActor(Pawn);
    }

    if (APlayerController* PC = Cast<APlayerController>(Context))
    {
        return ResolveFromController(PC);
    }

    if (ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(Context))
    {
        return ResolveFromController(LocalPlayer->GetPlayerController(Context->GetWorld()));
    }

    if (UGameInstance* GameInstance = Cast<UGameInstance>(Context))
    {
        for (ULocalPlayer* LocalPlayer : GameInstance->GetLocalPlayers())
        {
            if (UObject* Resolved = ResolveFromController(LocalPlayer ? LocalPlayer->GetPlayerController(GameInstance->GetWorld()) : nullptr))
            {
                return Resolved;
            }
        }
    }

    return nullptr;
}

void UInventoryViewModel::EnsureInventorySourceFromContext()
{
    UObject* CurrentSource = InventorySource.GetObject();
    UObject* ResolvedSource = ResolveInventorySourceFromContext();

    if (ResolvedSource && ResolvedSource != CurrentSource)
    {
        SetInventorySource(ResolvedSource);
    }
}

void UInventoryViewModel::Shutdown()
{
    if (IInventoryWorldContainerTransferBridge* Bridge = GetWorldContainerTransferBridge(InventorySource.GetObject()))
    {
        Bridge->OnWorldContainerSessionOpenedNative().RemoveAll(this);
        Bridge->OnWorldContainerSessionClosedNative().RemoveAll(this);
    }

    if (InventorySource)
    {
        InventorySource->OnInventoryViewChanged().RemoveAll(this);
        InventorySource->OnInventoryErrorNative().RemoveAll(this);
    }

    InventorySource.SetObject(nullptr);
    InventorySource.SetInterface(nullptr);
    InventoryCommands.SetObject(nullptr);
    InventoryCommands.SetInterface(nullptr);
    NearbyContainerSource.SetObject(nullptr);
    NearbyContainerSource.SetInterface(nullptr);
    NearbySessionHandle.Reset();
    ClearNearbyContainerData();
    CachedEntries.Reset();
    Super::Shutdown();
}

void UInventoryViewModel::SetInventorySource(UObject* InObject)
{
    if (InventorySource.GetObject() == InObject)
    {
        return;
    }

    if (IInventoryWorldContainerTransferBridge* ExistingBridge = GetWorldContainerTransferBridge(InventorySource.GetObject()))
    {
        ExistingBridge->OnWorldContainerSessionOpenedNative().RemoveAll(this);
        ExistingBridge->OnWorldContainerSessionClosedNative().RemoveAll(this);
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

    if (IInventoryWorldContainerTransferBridge* Bridge = GetWorldContainerTransferBridge(InObject))
    {
        Bridge->OnWorldContainerSessionOpenedNative().AddUObject(this, &UInventoryViewModel::HandleWorldContainerSessionOpened);
        Bridge->OnWorldContainerSessionClosedNative().AddUObject(this, &UInventoryViewModel::HandleWorldContainerSessionClosed);
        AActor* ActiveTargetActor = nullptr;
        FContainerSessionHandle ActiveHandle;
        if (IInventoryWorldContainerTransferBridge::Execute_GetActiveWorldContainerSession(
                InObject,
                ActiveTargetActor,
                ActiveHandle))
        {
            if (UObject* ActiveSourceObject = ResolveWorldContainerSessionSourceFromActor(ActiveTargetActor))
            {
                SetNearbyContainerSource(ActiveSourceObject, ActiveHandle);
            }
            ShowPanel();
        }
    }

    RefreshFromInventory();
}

void UInventoryViewModel::SetNearbyContainerSource(UObject* InObject, const FContainerSessionHandle& InSessionHandle)
{
    if (!InObject || !InObject->GetClass()->ImplementsInterface(UWorldContainerSessionSource::StaticClass()) || !InSessionHandle.IsValid())
    {
        ClearNearbyContainerSource();
        return;
    }

    NearbyContainerSource.SetObject(InObject);
    NearbyContainerSource.SetInterface(Cast<IWorldContainerSessionSource>(InObject));
    NearbySessionHandle = InSessionHandle;
    RefreshFromInventory();
}

void UInventoryViewModel::ClearNearbyContainerSource()
{
    NearbyContainerSource.SetObject(nullptr);
    NearbyContainerSource.SetInterface(nullptr);
    NearbySessionHandle.Reset();
    ClearNearbyContainerData();
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
    if (IsNearbyEntryInstanceId(InstanceId))
    {
        RequestTakeNearbyItem(InstanceId, 1);
        return;
    }

    if (InventoryCommands)
    {
        InventoryCommands->RequestUseItem(InstanceId);
    }
}

void UInventoryViewModel::RequestTakeNearbyItem(int32 InstanceId, int32 Quantity)
{
    RequestTakeNearbyItemToContainer(
        InstanceId,
        FGameplayTag(),
        FIntPoint(-1, -1),
        false,
        Quantity);
}

void UInventoryViewModel::RequestTakeNearbyItemToContainer(
    int32 InstanceId,
    FGameplayTag TargetContainerId,
    FIntPoint TargetGridPos,
    bool bTargetRotated,
    int32 Quantity)
{
    if (!NearbyContainerSource || !NearbySessionHandle.IsValid())
    {
        return;
    }

    IInventoryWorldContainerTransferBridge* TransferBridge = GetWorldContainerTransferBridge(InventorySource.GetObject());
    if (!TransferBridge)
    {
        return;
    }

    FText ErrorMessage;
    const bool bTransferSucceeded = IInventoryWorldContainerTransferBridge::Execute_TransferWorldContainerEntryToInventory(
            InventorySource.GetObject(),
            NearbyContainerSource.GetObject(),
            NearbySessionHandle,
            InstanceId,
            Quantity,
            TargetContainerId,
            TargetGridPos,
            bTargetRotated,
            ErrorMessage);

    if (!bTransferSucceeded
        && TargetContainerId.IsValid()
        && TargetGridPos.X >= 0
        && TargetGridPos.Y >= 0)
    {
        FInventoryEntryView DraggedEntry;
        FGameplayTag AlternateContainerId;
        FIntPoint AlternateGridPos = FIntPoint(-1, -1);
        if (TryGetEntryByInstanceId(InstanceId, DraggedEntry))
        {
            const FIntPoint ItemSize = bTargetRotated
                ? FIntPoint(FMath::Max(1, DraggedEntry.GridSize.Y), FMath::Max(1, DraggedEntry.GridSize.X))
                : FIntPoint(FMath::Max(1, DraggedEntry.GridSize.X), FMath::Max(1, DraggedEntry.GridSize.Y));

            if (DoesCachedInventoryPlacementOverlap(TargetContainerId, TargetGridPos, ItemSize, InstanceId)
                && TryResolveAlternateHandDropTarget(
                    TargetContainerId,
                    TargetGridPos,
                    ItemSize,
                    AlternateContainerId,
                    AlternateGridPos)
                && !DoesCachedInventoryPlacementOverlap(AlternateContainerId, AlternateGridPos, ItemSize, InstanceId))
            {
                FText RetryError;
                if (IInventoryWorldContainerTransferBridge::Execute_TransferWorldContainerEntryToInventory(
                        InventorySource.GetObject(),
                        NearbyContainerSource.GetObject(),
                        NearbySessionHandle,
                        InstanceId,
                        Quantity,
                        AlternateContainerId,
                        AlternateGridPos,
                        bTargetRotated,
                        RetryError))
                {
                    UE_LOG(LogInventoryVM, Verbose,
                        TEXT("RequestTakeNearbyItemToContainer: redirected occupied hand target %s (%d,%d) -> %s (%d,%d)"),
                        *TargetContainerId.ToString(),
                        TargetGridPos.X,
                        TargetGridPos.Y,
                        *AlternateContainerId.ToString(),
                        AlternateGridPos.X,
                        AlternateGridPos.Y);
                    RefreshFromInventory();
                    return;
                }

                if (!RetryError.IsEmpty())
                {
                    ErrorMessage = RetryError;
                }
            }
        }
    }

    if (!bTransferSucceeded)
    {
        if (!ErrorMessage.IsEmpty())
        {
            OnInventoryError.Broadcast(ErrorMessage);
        }
        return;
    }

    RefreshFromInventory();
}

void UInventoryViewModel::RequestStoreItemInNearbyContainer(int32 InstanceId, int32 Quantity)
{
    RequestStoreItemInNearbyContainerAt(
        InstanceId,
        FIntPoint(-1, -1),
        false,
        Quantity);
}

void UInventoryViewModel::RequestStoreItemInNearbyContainerAt(
    int32 InstanceId,
    FIntPoint TargetGridPos,
    bool bTargetRotated,
    int32 Quantity)
{
    if (!NearbyContainerSource || !NearbySessionHandle.IsValid())
    {
        UE_LOG(LogInventoryVM, Warning,
            TEXT("RequestStoreItemInNearbyContainerAt ignored: Nearby source/session missing. HasSource=%d HasSession=%d"),
            NearbyContainerSource ? 1 : 0,
            NearbySessionHandle.IsValid() ? 1 : 0);
        return;
    }

    IInventoryWorldContainerTransferBridge* TransferBridge = GetWorldContainerTransferBridge(InventorySource.GetObject());
    if (!TransferBridge)
    {
        UE_LOG(LogInventoryVM, Warning,
            TEXT("RequestStoreItemInNearbyContainerAt ignored: Inventory source has no world-container transfer bridge"));
        return;
    }

    UE_LOG(LogInventoryVM, Log,
        TEXT("RequestStoreItemInNearbyContainerAt: InstanceId=%d Qty=%d Pos=(%d,%d) Rot=%d"),
        InstanceId,
        Quantity,
        TargetGridPos.X,
        TargetGridPos.Y,
        bTargetRotated ? 1 : 0);

    FText ErrorMessage;
    if (!IInventoryWorldContainerTransferBridge::Execute_StoreInventoryEntryInWorldContainer(
            InventorySource.GetObject(),
            NearbyContainerSource.GetObject(),
            NearbySessionHandle,
            InstanceId,
            Quantity,
            TargetGridPos,
            bTargetRotated,
            ErrorMessage))
    {
        RefreshFromInventory();
        UE_LOG(LogInventoryVM, Warning,
            TEXT("RequestStoreItemInNearbyContainerAt failed: InstanceId=%d Qty=%d Pos=(%d,%d) Rot=%d Error=%s"),
            InstanceId,
            Quantity,
            TargetGridPos.X,
            TargetGridPos.Y,
            bTargetRotated ? 1 : 0,
            *ErrorMessage.ToString());
        if (!ErrorMessage.IsEmpty())
        {
            OnInventoryError.Broadcast(ErrorMessage);
        }
        return;
    }

    RefreshFromInventory();
}

void UInventoryViewModel::RequestMoveItemInNearbyContainer(
    int32 InstanceId,
    FIntPoint TargetGridPos,
    bool bTargetRotated,
    int32 Quantity)
{
    if (!NearbyContainerSource || !NearbySessionHandle.IsValid())
    {
        UE_LOG(LogInventoryVM, Warning,
            TEXT("RequestMoveItemInNearbyContainer ignored: Nearby source/session missing. HasSource=%d HasSession=%d"),
            NearbyContainerSource ? 1 : 0,
            NearbySessionHandle.IsValid() ? 1 : 0);
        return;
    }

    IInventoryWorldContainerTransferBridge* TransferBridge = GetWorldContainerTransferBridge(InventorySource.GetObject());
    if (!TransferBridge)
    {
        UE_LOG(LogInventoryVM, Warning,
            TEXT("RequestMoveItemInNearbyContainer ignored: Inventory source has no world-container transfer bridge"));
        return;
    }

    UE_LOG(LogInventoryVM, Log,
        TEXT("RequestMoveItemInNearbyContainer: InstanceId=%d Qty=%d Pos=(%d,%d) Rot=%d"),
        InstanceId,
        Quantity,
        TargetGridPos.X,
        TargetGridPos.Y,
        bTargetRotated ? 1 : 0);

    FText ErrorMessage;
    if (!IInventoryWorldContainerTransferBridge::Execute_MoveWithinWorldContainer(
            InventorySource.GetObject(),
            NearbyContainerSource.GetObject(),
            NearbySessionHandle,
            InstanceId,
            Quantity,
            TargetGridPos,
            bTargetRotated,
            ErrorMessage))
    {
        RefreshFromInventory();
        UE_LOG(LogInventoryVM, Warning,
            TEXT("RequestMoveItemInNearbyContainer failed: InstanceId=%d Qty=%d Pos=(%d,%d) Rot=%d Error=%s"),
            InstanceId,
            Quantity,
            TargetGridPos.X,
            TargetGridPos.Y,
            bTargetRotated ? 1 : 0,
            *ErrorMessage.ToString());
        if (!ErrorMessage.IsEmpty())
        {
            OnInventoryError.Broadcast(ErrorMessage);
        }
        return;
    }

    RefreshFromInventory();
}

void UInventoryViewModel::RequestTakeAllNearbyContainer()
{
    if (!NearbyContainerSource || !NearbySessionHandle.IsValid())
    {
        return;
    }

    IInventoryWorldContainerTransferBridge* TransferBridge = GetWorldContainerTransferBridge(InventorySource.GetObject());
    if (!TransferBridge)
    {
        return;
    }

    FText ErrorMessage;
    if (!IInventoryWorldContainerTransferBridge::Execute_TakeAllFromWorldContainer(
            InventorySource.GetObject(),
            NearbyContainerSource.GetObject(),
            NearbySessionHandle,
            ErrorMessage))
    {
        if (!ErrorMessage.IsEmpty())
        {
            OnInventoryError.Broadcast(ErrorMessage);
        }
        return;
    }

    RefreshFromInventory();
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
    InventoryViewModelActionRules::FActionBuildContext Context;
    Context.bHasCommands = HasCommands();
    Context.bIsNearbyEntry = IsNearbyEntryInstanceId(Entry.InstanceId);
    Context.bCanTakeNearby = InventorySource
        && NearbyContainerSource
        && NearbySessionHandle.IsValid()
        && GetWorldContainerTransferBridge(InventorySource.GetObject()) != nullptr;
    InventoryViewModelActionRules::BuildActionDescriptors(Entry, Context, OutActions);
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

const FName& UInventoryViewModel::GetActionIdUse() { return InventoryViewModelActionRules::GetActionIdUse(); }

const FName& UInventoryViewModel::GetActionIdEquip() { return InventoryViewModelActionRules::GetActionIdEquip(); }

const FName& UInventoryViewModel::GetActionIdDrop() { return InventoryViewModelActionRules::GetActionIdDrop(); }

const FName& UInventoryViewModel::GetActionIdSplit() { return InventoryViewModelActionRules::GetActionIdSplit(); }

const FProjectUIActionDescriptor* UInventoryViewModel::FindActionDescriptor(const TArray<FProjectUIActionDescriptor>& Actions, FName ActionId)
{
    return InventoryViewModelActionRules::FindActionDescriptor(Actions, ActionId);
}

bool UInventoryViewModel::IsActionEnabled(const TArray<FProjectUIActionDescriptor>& Actions, FName ActionId)
{
    return InventoryViewModelActionRules::IsActionEnabled(Actions, ActionId);
}

bool UInventoryViewModel::HasEnabledActions(const TArray<FProjectUIActionDescriptor>& Actions)
{
    return InventoryViewModelActionRules::HasEnabledActions(Actions);
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

void UInventoryViewModel::RefreshNearbyContainerData()
{
    if (!NearbyContainerSource || !NearbySessionHandle.IsValid())
    {
        UE_LOG(LogInventoryVM, Log,
            TEXT("RefreshNearbyContainerData: Cleared (Source=%d Session=%d)"),
            NearbyContainerSource ? 1 : 0,
            NearbySessionHandle.IsValid() ? 1 : 0);
        ClearNearbyContainerData();
        return;
    }

    CachedNearbyContainer = IWorldContainerSessionSource::Execute_GetContainerView(NearbyContainerSource.GetObject());
    CachedNearbyEntries = IWorldContainerSessionSource::Execute_GetContainerEntryViews(NearbyContainerSource.GetObject());
    UpdateNearbyContainerLabel(IWorldContainerSessionSource::Execute_GetContainerDisplayLabel(NearbyContainerSource.GetObject()));
    UpdatebHasNearbyContainer(CachedNearbyContainer.GridSize.X > 0 && CachedNearbyContainer.GridSize.Y > 0);
    UpdateNearbyContainerCurrentWeight(CachedNearbyContainer.CurrentWeight);
    UpdateNearbyContainerMaxWeight(CachedNearbyContainer.MaxWeight);
    UpdateNearbyContainerCurrentVolume(CachedNearbyContainer.CurrentVolume);
    UpdateNearbyContainerMaxVolume(CachedNearbyContainer.MaxVolume);
    UpdateNearbyContainerCellDepthUnits(CachedNearbyContainer.CellDepthUnits);

    UE_LOG(LogInventoryVM, Log,
        TEXT("RefreshNearbyContainerData: %d entries, Grid=%dx%d, W=%.1f/%.1f, V=%.1f/%.1f"),
        CachedNearbyEntries.Num(),
        CachedNearbyContainer.GridSize.X, CachedNearbyContainer.GridSize.Y,
        CachedNearbyContainer.CurrentWeight, CachedNearbyContainer.MaxWeight,
        CachedNearbyContainer.CurrentVolume, CachedNearbyContainer.MaxVolume);

    BuildEnabledCellsForContainer(CachedNearbyContainer, NearbyCellEnabled);
}

void UInventoryViewModel::ClearNearbyContainerData()
{
    CachedNearbyContainer = FInventoryContainerView();
    CachedNearbyEntries.Reset();
    NearbyCellInstanceIds.Reset();
    NearbyCellEnabled.Reset();
    UpdateNearbyContainerLabel(FText::GetEmpty());
    UpdatebHasNearbyContainer(false);
    UpdateNearbyContainerCurrentWeight(0.f);
    UpdateNearbyContainerMaxWeight(0.f);
    UpdateNearbyContainerCurrentVolume(0.f);
    UpdateNearbyContainerMaxVolume(0.f);
    UpdateNearbyContainerCellDepthUnits(0);
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
        NearbyCellInstanceIds.Reset();
        NearbyCellEnabled.Reset();
        PocketCellVisuals.Reset();
        PocketCellInstanceIds.Reset();
        PocketCellEnabled.Reset();
        EquipSlotTags.Reset();
        EquipSlotInstanceIds.Reset();
        ClearNearbyContainerData();
        SetLeftHandCellVisuals(TArray<FInventoryCellVisualState>());
        SetRightHandCellVisuals(TArray<FInventoryCellVisualState>());
        SetCellVisuals(TArray<FInventoryCellVisualState>());
        SetSecondaryCellVisuals(TArray<FInventoryCellVisualState>());
        NotifyPocketCellVisualsChanged();
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

    RefreshNearbyContainerData();
    BuildContainerData(Containers);

    UpdateItemCount(Entries.Num());
    UpdateCurrentWeight(InventorySource->GetCurrentWeight());
    UpdateMaxWeight(InventorySource->GetMaxWeight());
    UpdateCurrentVolume(InventorySource->GetCurrentVolume());
    UpdateMaxVolume(InventorySource->GetMaxVolume());
    BuildCellVisuals(Entries);
    BuildSecondaryCellVisuals(Entries);
    BuildHandCellVisuals(Entries);
    BuildPocketCellVisuals(Entries);
    BuildEquipSlotLabels(Entries);

    UE_LOG(LogInventoryVM, Log, TEXT("RefreshFromInventory: Grid=%dx%d, StorageContainers=%d, LHand=%d visuals, RHand=%d visuals"),
        GridWidth, GridHeight, CachedContainers.Num(), LeftHandCellVisuals.Num(), RightHandCellVisuals.Num());
}

void UInventoryViewModel::TogglePanel()
{
    if (bPanelVisible)
    {
        HidePanel();
        return;
    }

    ShowPanel();
}

void UInventoryViewModel::ShowPanel()
{
    EnsureInventorySourceFromContext();
    RefreshFromInventory();
    UpdatebPanelVisible(true);
}

void UInventoryViewModel::HidePanel()
{
    if (NearbySessionHandle.IsValid())
    {
        if (IInventoryWorldContainerTransferBridge* Bridge = GetWorldContainerTransferBridge(InventorySource.GetObject()))
        {
            FText CloseError;
            const bool bCloseRequested = IInventoryWorldContainerTransferBridge::Execute_RequestCloseWorldContainerSession(
                InventorySource.GetObject(),
                NearbySessionHandle,
                CloseError);
            if (!bCloseRequested && !CloseError.IsEmpty())
            {
                HandleInventoryErrorFromSource(CloseError);
            }
        }
    }

    UpdatebPanelVisible(false);
}

void UInventoryViewModel::HandleWorldContainerSessionOpened(
    UObject* WorldContainerSource,
    const FContainerSessionHandle& SessionHandle)
{
    EnsureInventorySourceFromContext();
    SetNearbyContainerSource(WorldContainerSource, SessionHandle);
    ShowPanel();
}

void UInventoryViewModel::HandleWorldContainerSessionClosed(const FContainerSessionHandle& SessionHandle)
{
    if (NearbySessionHandle.IsValid() && NearbySessionHandle.SessionId == SessionHandle.SessionId)
    {
        ClearNearbyContainerSource();
    }
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

    TArray<FText> PocketLabels;
    PocketLabels.Reserve(PocketContainers.Num());
    PocketCellEnabled.SetNum(PocketContainers.Num());
    for (int32 PocketIndex = 0; PocketIndex < PocketContainers.Num(); ++PocketIndex)
    {
        const FInventoryContainerView& PocketContainer = PocketContainers[PocketIndex];
        PocketLabels.Add(FText::FromString(MakeContainerShortName(PocketContainer.ContainerId)));
        BuildEnabledCellsForContainer(PocketContainer, PocketCellEnabled[PocketIndex]);
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
        UpdateContainerCellDepthUnits(0);
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
        UpdateContainerCellDepthUnits(Primary.CellDepthUnits);
        BuildEnabledCellsForContainer(Primary, CellEnabled);
    }

    if (bHasNearbyContainer)
    {
        UpdateSecondaryGridWidth(CachedNearbyContainer.GridSize.X);
        UpdateSecondaryGridHeight(CachedNearbyContainer.GridSize.Y);
        SecondaryCellEnabled = NearbyCellEnabled;
    }
    else if (!LargeContainers.IsValidIndex(SecondaryIndex))
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
        BuildEnabledCellsForContainer(Secondary, SecondaryCellEnabled);
    }
}

// Cell text building delegated to FInventoryViewModelCellBuilder (SOLID)
void UInventoryViewModel::BuildCellVisuals(const TArray<FInventoryEntryView>& Entries)
{
    TArray<FInventoryCellVisualState> NewVisuals;
    const FGameplayTag ContainerId = CachedContainers.IsValidIndex(SelectedContainerIndex)
        ? CachedContainers[SelectedContainerIndex].ContainerId : FGameplayTag();
    FInventoryViewModelCellBuilder::Build(Entries, ContainerId, GridWidth, GridHeight, CellInstanceIds, NewVisuals);
    SetCellVisuals(NewVisuals);
}

void UInventoryViewModel::BuildSecondaryCellVisuals(const TArray<FInventoryEntryView>& Entries)
{
    TArray<FInventoryCellVisualState> NewVisuals;
    if (bHasNearbyContainer)
    {
        FInventoryViewModelCellBuilder::Build(
            CachedNearbyEntries,
            CachedNearbyContainer.ContainerId,
            SecondaryGridWidth,
            SecondaryGridHeight,
            NearbyCellInstanceIds,
            NewVisuals);
        SecondaryCellInstanceIds = NearbyCellInstanceIds;
    }
    else
    {
        const FGameplayTag ContainerId = CachedContainers.IsValidIndex(SecondaryContainerIndex)
            ? CachedContainers[SecondaryContainerIndex].ContainerId : FGameplayTag();
        FInventoryViewModelCellBuilder::Build(Entries, ContainerId, SecondaryGridWidth, SecondaryGridHeight, SecondaryCellInstanceIds, NewVisuals);
    }
    SetSecondaryCellVisuals(NewVisuals);
}

void UInventoryViewModel::BuildPocketCellVisuals(const TArray<FInventoryEntryView>& Entries)
{
    PocketCellVisuals.SetNum(CachedPocketContainers.Num());
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
            PocketCellVisuals[PocketIndex]);
    }

    NotifyPocketCellVisualsChanged();
}

void UInventoryViewModel::NotifyPocketCellVisualsChanged()
{
    NotifyPropertyChanged(FName(TEXT("PocketCellVisuals")));
}

void UInventoryViewModel::BuildHandCellVisuals(const TArray<FInventoryEntryView>& Entries)
{
    // Dedicated hand containers are real 2x2 micro-grids.
    TArray<FInventoryCellVisualState> LeftVisuals;
    TArray<int32> LeftIds;
    FInventoryViewModelCellBuilder::Build(Entries, ProjectTags::Item_Container_LeftHand,
        HandGridSize, HandGridSize, LeftIds, LeftVisuals);

    TArray<FInventoryCellVisualState> RightVisuals;
    TArray<int32> RightIds;
    FInventoryViewModelCellBuilder::Build(Entries, ProjectTags::Item_Container_RightHand,
        HandGridSize, HandGridSize, RightIds, RightVisuals);

    for (const FInventoryEntryView& Entry : Entries)
    {
        if (Entry.ContainerId != ProjectTags::Item_Container_Hands || Entry.InstanceId <= 0)
        {
            continue;
        }

        if (LeftVisuals.Num() < HandCellCount)
        {
            LeftVisuals.Init(FInventoryCellVisualState(), HandCellCount);
            LeftIds.Init(EmptyCellInstanceId, HandCellCount);
        }
        if (RightVisuals.Num() < HandCellCount)
        {
            RightVisuals.Init(FInventoryCellVisualState(), HandCellCount);
            RightIds.Init(EmptyCellInstanceId, HandCellCount);
        }

        const FString Label = !Entry.IconCode.IsEmpty()
            ? Entry.IconCode
            : FInventoryViewModelCellBuilder::BuildEntryLabel(Entry.DisplayName, Entry.Quantity, Entry.ItemId);

        TArray<FInventoryCellVisualState>& TargetVisuals = (Entry.GridPos.X == 0) ? LeftVisuals : RightVisuals;
        TArray<int32>& TargetIds = (Entry.GridPos.X == 0) ? LeftIds : RightIds;
        if (TargetVisuals[0].IsEmpty())
        {
            TargetVisuals[0].InstanceId = Entry.InstanceId;
            TargetVisuals[0].PrimaryText = FText::FromString(Label);
            TargetVisuals[0].QuantityText = Entry.Quantity > 1 ? FText::AsNumber(Entry.Quantity) : FText::GetEmpty();
            TargetVisuals[0].bUseIconFont = !Entry.IconCode.IsEmpty();
            TargetVisuals[0].bShowQuantity = Entry.Quantity > 1;
            TargetVisuals[0].bIsAnchorCell = true;
            TargetIds[0] = Entry.InstanceId;
        }
    }

    SetLeftHandCellVisuals(LeftVisuals);
    SetLeftHandCellInstanceIds(LeftIds);
    SetRightHandCellVisuals(RightVisuals);
    SetRightHandCellInstanceIds(RightIds);

    UE_LOG(LogInventoryVM, Log, TEXT("BuildHandCellVisuals: L=%d visuals, R=%d visuals"),
        LeftVisuals.Num(), RightVisuals.Num());
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
    BuildCellVisuals(CachedEntries);
    BuildSecondaryCellVisuals(CachedEntries);
    BuildPocketCellVisuals(CachedEntries);
}

void UInventoryViewModel::SetSecondaryContainerIndex(int32 NewIndex)
{
    if (bHasNearbyContainer)
    {
        return;
    }

    if (NewIndex == SecondaryContainerIndex)
    {
        return;
    }

    UpdateSecondaryContainerIndex(NewIndex);
    BuildContainerData(CachedAllContainers);
    BuildCellVisuals(CachedEntries);
    BuildSecondaryCellVisuals(CachedEntries);
    BuildPocketCellVisuals(CachedEntries);
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
    if (bHasNearbyContainer)
    {
        return CachedNearbyContainer.ContainerId;
    }

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

const TArray<FInventoryCellVisualState>& UInventoryViewModel::GetPocketCellVisuals(int32 PocketIndex) const
{
    static const TArray<FInventoryCellVisualState> EmptyVisuals;
    return PocketCellVisuals.IsValidIndex(PocketIndex) ? PocketCellVisuals[PocketIndex] : EmptyVisuals;
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

int32 UInventoryViewModel::FindPocketIndexByContainerTag(FGameplayTag ContainerTag) const
{
    return InventoryViewModelSurfaceDispatch::FindPocketIndexByContainerTag(*this, ContainerTag);
}

bool UInventoryViewModel::IsCellEnabledForSurface(FGameplayTag SurfaceTag, int32 CellIndex) const
{
    return InventoryViewModelSurfaceDispatch::IsCellEnabledForSurface(*this, SurfaceTag, CellIndex);
}

int32 UInventoryViewModel::GetCellOccupant(FGameplayTag SurfaceTag, int32 CellIndex) const
{
    return InventoryViewModelSurfaceDispatch::GetCellOccupant(*this, SurfaceTag, CellIndex);
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

    return TryGetNearbyEntryByInstanceId(InstanceId, OutEntry);
}

bool UInventoryViewModel::TryGetNearbyEntryByInstanceId(int32 InstanceId, FInventoryEntryView& OutEntry) const
{
    if (InstanceId <= 0 || InstanceId == EmptyCellInstanceId)
    {
        return false;
    }

    for (const FInventoryEntryView& Entry : CachedNearbyEntries)
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

    const TArray<FInventoryEntryView>& SecondaryEntries = bHasNearbyContainer ? CachedNearbyEntries : CachedEntries;
    for (const FInventoryEntryView& Entry : SecondaryEntries)
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

bool UInventoryViewModel::IsNearbyEntryInstanceId(int32 InstanceId) const
{
    FInventoryEntryView Entry;
    return TryGetNearbyEntryByInstanceId(InstanceId, Entry);
}

bool UInventoryViewModel::ResolveHandDropTarget(bool bLeftHand, FGameplayTag& OutContainerId, FIntPoint& OutGridPos) const
{
    return InventoryViewModelPlacement::ResolveHandDropTarget(CachedAllContainers, CachedEntries, bLeftHand, OutContainerId, OutGridPos);
}

bool UInventoryViewModel::TryResolveFreePlacementInContainer(
    const FGameplayTag& ContainerId,
    FIntPoint ItemSize,
    FIntPoint& OutGridPos,
    TOptional<FIntPoint> ExcludedGridPos) const
{
    return InventoryViewModelPlacement::TryResolveFreePlacementInContainer(CachedAllContainers, CachedEntries, ContainerId, ItemSize, OutGridPos, ExcludedGridPos);
}

bool UInventoryViewModel::TryResolveAlternateHandDropTarget(
    const FGameplayTag& CurrentContainerId,
    FIntPoint CurrentGridPos,
    FIntPoint ItemSize,
    FGameplayTag& OutContainerId,
    FIntPoint& OutGridPos) const
{
    return InventoryViewModelPlacement::TryResolveAlternateHandDropTarget(CachedAllContainers, CachedEntries, CurrentContainerId, CurrentGridPos, ItemSize, OutContainerId, OutGridPos);
}

bool UInventoryViewModel::DoesCachedInventoryPlacementOverlap(
    const FGameplayTag& ContainerId,
    FIntPoint GridPos,
    FIntPoint ItemSize,
    int32 IgnoreInstanceId) const
{
    return InventoryViewModelPlacement::DoesCachedInventoryPlacementOverlap(CachedEntries, ContainerId, GridPos, ItemSize, IgnoreInstanceId);
}

bool UInventoryViewModel::IsContainerEmpty(FGameplayTag ContainerId, int32 IgnoreInstanceId) const
{
    return InventoryViewModelPlacement::IsContainerEmpty(CachedEntries, ContainerId, IgnoreInstanceId);
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

void UInventoryViewModel::SetLeftHandCellVisuals(const TArray<FInventoryCellVisualState>& InValue)
{
    LeftHandCellVisuals = InValue;
    NotifyPropertyChanged(GET_MEMBER_NAME_CHECKED(UInventoryViewModel, LeftHandCellVisuals));
}

void UInventoryViewModel::SetRightHandCellVisuals(const TArray<FInventoryCellVisualState>& InValue)
{
    RightHandCellVisuals = InValue;
    NotifyPropertyChanged(GET_MEMBER_NAME_CHECKED(UInventoryViewModel, RightHandCellVisuals));
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

void UInventoryViewModel::UpdateCellVisuals(const TArray<FInventoryCellVisualState>& InValue)
{
    CellVisuals = InValue;
    NotifyPropertyChanged(GET_MEMBER_NAME_CHECKED(UInventoryViewModel, CellVisuals));
}

void UInventoryViewModel::UpdateSecondaryCellVisuals(const TArray<FInventoryCellVisualState>& InValue)
{
    SecondaryCellVisuals = InValue;
    NotifyPropertyChanged(GET_MEMBER_NAME_CHECKED(UInventoryViewModel, SecondaryCellVisuals));
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

// ============================================================================
// IInventorySurfacePolicyProvider
// ----------------------------------------------------------------------------
// Consumed by UInventoryUIDragHostSubsystem::RegisterSurface via a
// subsystem-local closure that forwards here; widgets no longer build
// lambdas that reach into global Slate state. The SurfaceTag arg IS the
// policy key: the VM dispatches by tag to the surface-local rule, then
// falls back to the generic "empty-or-self" default for any surface whose
// tag is not a player-side tabbed grid (hands, pockets, nearby world).
// ============================================================================
bool UInventoryViewModel::IsPayloadAllowedOnOccupant(
    FGameplayTag SurfaceTag,
    const FProjectUIGridDragPayload& Payload,
    int32 OccupantId,
    int32 CellIndex) const
{
    return InventoryViewModelSurfaceDispatch::IsPayloadAllowedOnOccupant(*this, SurfaceTag, Payload, OccupantId, CellIndex);
}
