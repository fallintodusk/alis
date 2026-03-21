// Copyright ALIS. All Rights Reserved.

#include "Components/ProjectInventoryComponent.h"
#include "Helpers/InventoryGridPlacement.h"
#include "Helpers/InventoryContainerHelper.h"
#include "Helpers/InventorySaveHelper.h"
#include "Helpers/InventoryLootHelper.h"
#include "Helpers/InventoryWeightHelper.h"
#include "Helpers/InventoryStackHelper.h"
#include "Helpers/InventoryViewHelper.h"
#include "Helpers/InventoryAddHelper.h"
#include "Helpers/InventoryMoveHelper.h"
#include "Helpers/InventoryWorldContainerTransferHelper.h"
#include "ProjectInventory.h"
#include "Subsystems/ProjectContainerSessionSubsystem.h"
#include "Services/ObjectDefinitionCache.h"
#include "Services/IObjectSpawnService.h"
#include "Interfaces/IPickupSource.h"
#include "Interfaces/IWorldContainerSessionSource.h"
#include "ProjectServiceLocator.h"
#include "ProjectGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "ProjectGASLibrary.h"
#include "ProjectSaveSubsystem.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Modules/ModuleManager.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

namespace
{
UProjectContainerSessionSubsystem* ResolveLocalPlayerSessionSubsystem(const UProjectInventoryComponent* Inventory)
{
	if (!Inventory)
	{
		return nullptr;
	}

	const APawn* OwnerPawn = Cast<APawn>(Inventory->GetOwner());
	APlayerController* PlayerController = OwnerPawn ? Cast<APlayerController>(OwnerPawn->GetController()) : nullptr;
	if (!PlayerController)
	{
		PlayerController = Cast<APlayerController>(Inventory->GetOwner());
	}

	if (!PlayerController)
	{
		return nullptr;
	}

	ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();
	return LocalPlayer ? LocalPlayer->GetSubsystem<UProjectContainerSessionSubsystem>() : nullptr;
}

AActor* ResolveWorldContainerActorFromSource(UObject* WorldContainerSource)
{
	if (AActor* Actor = Cast<AActor>(WorldContainerSource))
	{
		return Actor;
	}

	if (const UActorComponent* Component = Cast<UActorComponent>(WorldContainerSource))
	{
		return Component->GetOwner();
	}

	return nullptr;
}

int32 GetTotalItemQuantity(const TArray<FInventoryEntry>& Entries, const FPrimaryAssetId& ObjectId)
{
	int32 TotalQuantity = 0;
	for (const FInventoryEntry& Entry : Entries)
	{
		if (Entry.ItemId == ObjectId)
		{
			TotalQuantity += Entry.Quantity;
		}
	}
	return TotalQuantity;
}

void BuildExpectedLootQuantities(const TArray<FLootEntry>& Items, TMap<FPrimaryAssetId, int32>& OutExpectedQuantities)
{
	OutExpectedQuantities.Reset();
	for (const FLootEntry& Item : Items)
	{
		if (!Item.IsValid())
		{
			continue;
		}

		OutExpectedQuantities.FindOrAdd(Item.ObjectId) += Item.Quantity;
	}
}
}

// -------------------------------------------------------------------------
// Constructor & Lifecycle
// -------------------------------------------------------------------------

UProjectInventoryComponent::UProjectInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	DefaultContainerId = ProjectTags::Item_Container_LeftHand;
	MaxSlots = 2;
	DefaultContainerGridWidth = 2;

	if (EquipSlotContainerGrants.Num() == 0)
	{
		// Fallback-only slot grants. Primary path is data-driven item ContainerGrants.
		// Rationale:
		// - Equipment variants can define different pocket/backpack sizes without hardcoding slots.
		// - Slot grants are only a safety net for legacy content during migration.
		// - Hands remain the only true default container for "naked" characters.
		auto AddGrant = [this](FGameplayTag SlotTag, FGameplayTag ContainerTag, FIntPoint GridSize, bool bWidthOnly = false, int32 InMaxCells = 0)
		{
			FEquipSlotContainerGrant Grant;
			Grant.EquipSlot = SlotTag;
			Grant.Container.ContainerId = ContainerTag;
			Grant.Container.GridSize = GridSize;
			Grant.Container.bWidthOnlyValidation = bWidthOnly;
			Grant.Container.MaxCells = InMaxCells;
			EquipSlotContainerGrants.Add(Grant);
		};

		// Equipped items occupying hand equipment slots collapse hand storage back to a single held item.
		AddGrant(ProjectTags::Item_EquipmentSlot_MainHand, ProjectTags::Item_Container_LeftHand, FIntPoint(2, 2), true, 1);
		AddGrant(ProjectTags::Item_EquipmentSlot_OffHand, ProjectTags::Item_Container_RightHand, FIntPoint(2, 2), true, 1);

		AddGrant(ProjectTags::Item_EquipmentSlot_Legs, ProjectTags::Item_Container_Pockets1, FIntPoint(2, 2));
		AddGrant(ProjectTags::Item_EquipmentSlot_Legs, ProjectTags::Item_Container_Pockets2, FIntPoint(2, 2));
		AddGrant(ProjectTags::Item_EquipmentSlot_Chest, ProjectTags::Item_Container_Pockets3, FIntPoint(2, 2));
		AddGrant(ProjectTags::Item_EquipmentSlot_Chest, ProjectTags::Item_Container_Pockets4, FIntPoint(2, 2));
		AddGrant(ProjectTags::Item_EquipmentSlot_Back, ProjectTags::Item_Container_Backpack, FIntPoint(6, 6));
	}
}

void UProjectInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UProjectInventoryComponent, Inventory);
}

void UProjectInventoryComponent::PostInitProperties()
{
	Super::PostInitProperties();
	Inventory.OwnerComponent = this;
}

void UProjectInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	TryLoadInventoryFromSave();

	// Server: Bind to fatigue/condition tag changes to refresh weight state when capacity multiplier changes
	if (GetOwnerRole() == ROLE_Authority)
	{
		BindCapacityTagEvents();
	}
}

void UProjectInventoryComponent::BindCapacityTagEvents()
{
	UAbilitySystemComponent* ASC = GetOwnerASC();
	if (!ASC)
	{
		return;
	}

	// Bind to parent tags (catches all child tag changes: Rested, Tired, Exhausted, Critical, etc.)
	auto BindTag = [this, ASC](const FGameplayTag& Tag)
	{
		ASC->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &UProjectInventoryComponent::OnCapacityTagChanged);
	};

	BindTag(ProjectTags::State_Fatigue);
	BindTag(ProjectTags::State_Condition);
}

void UProjectInventoryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Unbind tag events (ASC may already be gone, so check)
	if (UAbilitySystemComponent* ASC = GetOwnerASC())
	{
		ASC->RegisterGameplayTagEvent(ProjectTags::State_Fatigue, EGameplayTagEventType::NewOrRemoved)
			.RemoveAll(this);
		ASC->RegisterGameplayTagEvent(ProjectTags::State_Condition, EGameplayTagEventType::NewOrRemoved)
			.RemoveAll(this);
	}

	TrySaveInventoryToSave();

	Super::EndPlay(EndPlayReason);
}

void UProjectInventoryComponent::OnCapacityTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	// Fatigue or condition tag changed - recalculate weight state since capacity multiplier may have changed
	UpdateWeightStateTag();
}

// -------------------------------------------------------------------------
// IInventoryReadOnly - ContainsItem (zero-allocation override)
// -------------------------------------------------------------------------

bool UProjectInventoryComponent::ContainsItem(FPrimaryAssetId ItemId, int32 MinQuantity) const
{
	for (const FInventoryEntry& Entry : Inventory.Entries)
	{
		if (Entry.ItemId == ItemId && Entry.Quantity >= MinQuantity)
		{
			return true;
		}
	}
	return false;
}

// -------------------------------------------------------------------------
// IProjectActionReceiver
// -------------------------------------------------------------------------

void UProjectInventoryComponent::HandleAction(const FString& Context, const FString& Action)
{
	// "inventory.consume:<ObjectId>" - remove 1 item matching ObjectId
	if (!Action.StartsWith(TEXT("inventory.consume:")))
	{
		return;
	}

	FString ItemIdStr = Action.Mid(18);
	ItemIdStr.TrimStartAndEndInline();
	const bool bPrefixMatch = ItemIdStr.RemoveFromEnd(TEXT("*"));
	ItemIdStr.TrimStartAndEndInline();

	const FPrimaryAssetId TargetId(FPrimaryAssetType(TEXT("ObjectDefinition")), FName(*ItemIdStr));

	FInventoryEntry FoundEntry;
	if (!FindEntryByItemId(TargetId, FoundEntry))
	{
		// Explicit family fallback: inventory.consume:WaterBottle* consumes first WaterBottle*
		// variant (WaterBottleBig/WaterBottleSmall/etc.) when exact id is absent.
		if (bPrefixMatch)
		{
			const FPrimaryAssetType ObjectType(TEXT("ObjectDefinition"));
			for (const FInventoryEntry& Entry : Inventory.Entries)
			{
				if (Entry.Quantity <= 0 || Entry.ItemId.PrimaryAssetType != ObjectType)
				{
					continue;
				}

				if (Entry.ItemId.PrimaryAssetName.ToString().StartsWith(ItemIdStr, ESearchCase::IgnoreCase))
				{
					FoundEntry = Entry;
					break;
				}
			}
		}

		if (FoundEntry.Quantity <= 0)
		{
			UE_LOG(LogProjectInventory, Warning,
				TEXT("[HandleAction] inventory.consume: Item '%s' not found in inventory"),
				*ItemIdStr);
			return;
		}
	}

	UE_LOG(LogProjectInventory, Log,
		TEXT("[HandleAction] inventory.consume: Removing 1x '%s' (InstanceId=%d, Context='%s')"),
		*ItemIdStr, FoundEntry.InstanceId, *Context);

	// Route through server RPC for proper authority
	RequestRemoveItem(FoundEntry.InstanceId, 1);
}

void UProjectInventoryComponent::OnRep_Inventory()
{
	Inventory.OwnerComponent = this;
}

// -------------------------------------------------------------------------
// Public API (Routes to Server RPCs)
// -------------------------------------------------------------------------

void UProjectInventoryComponent::RequestAddItem(FPrimaryAssetId ObjectId, int32 Quantity)
{
	Server_AddItem(ObjectId, Quantity);
}

void UProjectInventoryComponent::RequestRemoveItem(int32 InstanceId, int32 Quantity)
{
	Server_RemoveItem(InstanceId, Quantity);
}

void UProjectInventoryComponent::RequestMoveItem(int32 InstanceId, FGameplayTag FromContainer, FIntPoint FromPos, FGameplayTag ToContainer, FIntPoint ToPos, int32 Quantity, bool bRotated)
{
	Server_MoveItem(InstanceId, FromContainer, FromPos, ToContainer, ToPos, Quantity, bRotated);
}

void UProjectInventoryComponent::RequestUseItem(int32 InstanceId)
{
	Server_UseItem(InstanceId);
}

void UProjectInventoryComponent::RequestEquipItem(int32 InstanceId, FGameplayTag EquipSlot)
{
	Server_EquipItem(InstanceId, EquipSlot);
}

void UProjectInventoryComponent::RequestUnequipItem(FGameplayTag EquipSlot)
{
	Server_UnequipItem(EquipSlot);
}

void UProjectInventoryComponent::RequestDropItem(int32 InstanceId, int32 Quantity)
{
	Server_DropItem(InstanceId, Quantity);
}

void UProjectInventoryComponent::RequestSwapHands()
{
	Server_SwapHands();
}

void UProjectInventoryComponent::RequestSplitStack(int32 InstanceId, int32 SplitQuantity)
{
	const FInventoryEntry* Entry = Inventory.FindEntry(InstanceId);
	if (!Entry)
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("RequestSplitStack: InstanceId %d not found"), InstanceId);
		return;
	}

	if (SplitQuantity <= 0 || SplitQuantity >= Entry->Quantity)
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("RequestSplitStack: Invalid split quantity %d (entry has %d)"), SplitQuantity, Entry->Quantity);
		return;
	}

	FInventoryContainerConfig ContainerConfig;
	if (!GetContainerConfig(Entry->ContainerId, ContainerConfig))
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("RequestSplitStack: Container %s not found"), *Entry->ContainerId.ToString());
		return;
	}

	FItemDataView ItemData;
	if (!GetItemDataView(Entry->ItemId, ItemData))
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("RequestSplitStack: ItemData not found for %s"), *Entry->ItemId.ToString());
		return;
	}

	// Use FindFreeGridPos - automatically handles slot-based, width-only, and MaxCells
	const FIntPoint ItemSize = GetItemGridSize(ItemData, Entry->bRotated);
	FIntPoint EmptyPos(-1, -1);
	bool bRotated = Entry->bRotated;

	if (!FindFreeGridPos(ContainerConfig, ItemSize, InstanceId, EmptyPos))
	{
		// Try rotated if allowed
		if (ContainerConfig.bAllowRotation)
		{
			const FIntPoint RotatedSize = GetItemGridSize(ItemData, !Entry->bRotated);
			if (RotatedSize != ItemSize && FindFreeGridPos(ContainerConfig, RotatedSize, InstanceId, EmptyPos))
			{
				bRotated = !Entry->bRotated;
			}
		}
	}

	if (EmptyPos.X < 0)
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("RequestSplitStack: No empty space found in container %s"), *Entry->ContainerId.ToString());
		BroadcastError(NSLOCTEXT("Inventory", "NoSpaceForSplit", "No space to split stack"));
		return;
	}

	RequestMoveItem(InstanceId, Entry->ContainerId, Entry->GridPos, Entry->ContainerId, EmptyPos, SplitQuantity, bRotated);
}

// SOLID: Delegated to FInventorySaveHelper
void UProjectInventoryComponent::GetSaveData(FInventorySaveData& OutData) const
{
	FInventorySaveHelper::BuildSaveData(Inventory.Entries, EquippedItems, OutData);
}

// SOLID: Uses FInventorySaveHelper with callbacks for state access
void UProjectInventoryComponent::LoadFromSaveData(const FInventorySaveData& InData, bool bApplyEquippedAbilities)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("LoadFromSaveData: Called without authority"));
		return;
	}

	FInventorySaveHelper::FLoadCallbacks Callbacks;
	Callbacks.ResetState = [this]() {
		Inventory.Entries.Reset();
		Inventory.MarkArrayDirty();
		Inventory.NextInstanceId = 1;
		EquippedItems.Reset();
	};
	Callbacks.ComputeSlotIndex = [this](FGameplayTag C, FIntPoint P) { return ComputeSlotIndex(C, P); };
	Callbacks.AddEntry = [this](const FInventoryEntrySaveData& S, int32 SlotIndex) {
		Inventory.AddEntryWithInstanceId(static_cast<uint32>(S.InstanceId), S.ItemId, S.Quantity,
			S.ContainerId, S.GridPos, S.bRotated, SlotIndex);
		if (FInventoryEntry* E = Inventory.FindEntry(S.InstanceId))
		{
			E->InstanceData = S.InstanceData;
			E->OverrideMagnitudes = S.OverrideMagnitudes;
			Inventory.MarkEntryDirty(*E);
		}
	};
	Callbacks.RestoreEquipped = [this](int32 Id, FGameplayTag Slot, bool bApply) {
		if (bApply)
		{
			Internal_EquipItem(Id, Slot);
		}
		else
		{
			FEquippedItemData Data;
			Data.InstanceId = Id;
			EquippedItems.Add(Slot, Data);
		}
	};
	Callbacks.BroadcastChange = [this, OwnerActor]() {
		InventoryViewChanged.Broadcast();
		if (OwnerActor && OwnerActor->HasAuthority())
		{
			OwnerActor->ForceNetUpdate();
		}
	};

	FInventorySaveHelper::ApplyLoadData(InData, bApplyEquippedAbilities, Callbacks);
}

// SOLID: Delegated to FInventorySaveHelper
bool UProjectInventoryComponent::SaveToSaveSubsystem(UProjectSaveSubsystem* SaveSubsystem, FName SaveKey)
{
	FInventorySaveData SaveData;
	GetSaveData(SaveData);
	return FInventorySaveHelper::SaveToSubsystem(SaveSubsystem, SaveKey, SaveData);
}

// SOLID: Delegated to FInventorySaveHelper
bool UProjectInventoryComponent::LoadFromSaveSubsystem(UProjectSaveSubsystem* SaveSubsystem, FName SaveKey, bool bApplyEquippedAbilities)
{
	FInventorySaveData SaveData;
	if (!FInventorySaveHelper::LoadFromSubsystem(SaveSubsystem, SaveKey, SaveData))
	{
		return false;
	}
	LoadFromSaveData(SaveData, bApplyEquippedAbilities);
	return true;
}

// SOLID: Delegated to FInventorySaveHelper
bool UProjectInventoryComponent::ShouldUseLocalSave() const
{
	return FInventorySaveHelper::ShouldUseLocalSave(GetOwner(), GetWorld());
}

void UProjectInventoryComponent::TryLoadInventoryFromSave()
{
	if (!ShouldUseLocalSave())
	{
		return;
	}

	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (!GameInstance)
	{
		return;
	}

	UProjectSaveSubsystem* SaveSubsystem = GameInstance->GetSubsystem<UProjectSaveSubsystem>();
	if (!SaveSubsystem)
	{
		return;
	}

	const FString AutoSlot = UProjectSaveSubsystem::GetAutoSaveSlotName();
	if (SaveSubsystem->DoesSaveExist(AutoSlot))
	{
		if (!SaveSubsystem->LoadGame(AutoSlot))
		{
			UE_LOG(LogProjectInventory, Warning, TEXT("TryLoadInventoryFromSave: AutoSave load failed (%s)"), *AutoSlot);
			return;
		}
	}
	else
	{
		SaveSubsystem->CreateNewSave(TEXT("Player"));
		SaveToSaveSubsystem(SaveSubsystem, NAME_None);
		if (!SaveSubsystem->SaveGame(AutoSlot))
		{
			UE_LOG(LogProjectInventory, Warning, TEXT("TryLoadInventoryFromSave: AutoSave create failed (%s)"), *AutoSlot);
			return;
		}
	}

	LoadFromSaveSubsystem(SaveSubsystem, NAME_None, true);
}

void UProjectInventoryComponent::TrySaveInventoryToSave()
{
	if (!ShouldUseLocalSave())
	{
		return;
	}

	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (!GameInstance)
	{
		return;
	}

	UProjectSaveSubsystem* SaveSubsystem = GameInstance->GetSubsystem<UProjectSaveSubsystem>();
	if (!SaveSubsystem)
	{
		return;
	}

	SaveToSaveSubsystem(SaveSubsystem, NAME_None);
	SaveSubsystem->SaveGame(UProjectSaveSubsystem::GetAutoSaveSlotName());
}

void UProjectInventoryComponent::BroadcastErrorLocal(const FText& ErrorMessage)
{
	OnInventoryError.Broadcast(ErrorMessage);
	InventoryErrorNative.Broadcast(ErrorMessage);
}

void UProjectInventoryComponent::Client_InventoryError_Implementation(const FText& ErrorMessage)
{
	BroadcastErrorLocal(ErrorMessage);
}

void UProjectInventoryComponent::Client_WorldContainerSessionOpened_Implementation(
	AActor* TargetActor,
	FContainerSessionHandle SessionHandle)
{
	HandleWorldContainerSessionOpenedLocal(TargetActor, SessionHandle);
}

void UProjectInventoryComponent::Client_WorldContainerSessionClosed_Implementation(
	FContainerSessionHandle SessionHandle)
{
	HandleWorldContainerSessionClosedLocal(SessionHandle);
}

void UProjectInventoryComponent::BroadcastError(const FText& ErrorMessage)
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		return;
	}

	// If locally controlled, broadcast directly
	if (OwnerPawn->IsLocallyControlled())
	{
		BroadcastErrorLocal(ErrorMessage);
		return;
	}

	// On server with remote client: send via Client RPC
	if (OwnerPawn->HasAuthority())
	{
		Client_InventoryError(ErrorMessage);
	}
}

void UProjectInventoryComponent::UpdateWeightStateTag()
{
	UAbilitySystemComponent* ASC = GetOwnerASC();
	// Server-only: clients should not modify ASC tags directly
	if (!ASC || !ASC->IsOwnerActorAuthoritative())
	{
		return;
	}

	const float MaxWeightTotal = GetMaxWeight();
	if (MaxWeightTotal <= 0.f)
	{
		return;
	}

	const float WeightPercent = (GetCurrentWeight() / MaxWeightTotal) * 100.f;
	const EWeightState NewState = ComputeWeightState(WeightPercent);

	// Set tag on first run or when state changes
	if (!bWeightTagInitialized || NewState != PrevWeightState)
	{
		FGameplayTag NewTag;
		switch (NewState)
		{
		case EWeightState::Light:      NewTag = ProjectTags::State_Weight_Light; break;
		case EWeightState::Medium:     NewTag = ProjectTags::State_Weight_Medium; break;
		case EWeightState::Heavy:      NewTag = ProjectTags::State_Weight_Heavy; break;
		case EWeightState::Overweight: NewTag = ProjectTags::State_Weight_Overweight; break;
		}
		SetStateTag(ASC, NewTag, ProjectTags::State_Weight);
		PrevWeightState = NewState;
		bWeightTagInitialized = true;

		UE_LOG(LogProjectInventory, Verbose, TEXT("Weight state changed: %.1f%% -> %s"),
			WeightPercent, *NewTag.ToString());
	}
}

// SOLID: Delegated to FInventoryWeightHelper
EWeightState UProjectInventoryComponent::ComputeWeightState(float WeightPercent) const
{
	return FInventoryWeightHelper::ComputeWeightState(WeightPercent, PrevWeightState);
}

void UProjectInventoryComponent::SetStateTag(UAbilitySystemComponent* ASC, const FGameplayTag& NewTag, const FGameplayTag& ParentTag)
{
	if (!ASC || !NewTag.IsValid() || !ParentTag.IsValid())
	{
		return;
	}

	// Remove any existing child tags of the parent
	FGameplayTagContainer CurrentTags;
	ASC->GetOwnedGameplayTags(CurrentTags);
	for (const FGameplayTag& Tag : CurrentTags)
	{
		if (Tag.MatchesTag(ParentTag) && Tag != ParentTag && Tag != NewTag)
		{
			ASC->RemoveLooseGameplayTag(Tag);
		}
	}

	// Add the new tag if not already present
	if (!ASC->HasMatchingGameplayTag(NewTag))
	{
		ASC->AddLooseGameplayTag(NewTag);
	}
}

// -------------------------------------------------------------------------
// Query API
// -------------------------------------------------------------------------

bool UProjectInventoryComponent::FindEntry(int32 InstanceId, FInventoryEntry& OutEntry) const
{
	const FInventoryEntry* Entry = Inventory.FindEntry(InstanceId);
	if (Entry)
	{
		OutEntry = *Entry;
		return true;
	}
	return false;
}

bool UProjectInventoryComponent::FindEntryByItemId(FPrimaryAssetId ObjectId, FInventoryEntry& OutEntry) const
{
	for (const FInventoryEntry& Entry : Inventory.Entries)
	{
		if (Entry.ItemId == ObjectId)
		{
			OutEntry = Entry;
			return true;
		}
	}
	return false;
}

// SOLID: Delegated to FInventoryViewHelper
void UProjectInventoryComponent::GetEntriesView(TArray<FInventoryEntryView>& OutEntries) const
{
	FInventoryViewHelper::FViewCallbacks Callbacks;
	Callbacks.GetItemDataView = [this](FPrimaryAssetId Id, FItemDataView& Out) { return GetItemDataView(Id, Out); };
	Callbacks.GetEffectivePlacement = [this](const FInventoryEntry& E, FGameplayTag& C, FIntPoint& P, bool& R) {
		return GetEffectiveEntryPlacement(E, C, P, R);
	};
	Callbacks.ComputeSlotIndex = [this](FGameplayTag C, FIntPoint P) { return ComputeSlotIndex(C, P); };
	Callbacks.GetEquipSlotGrants = [this](FGameplayTag S, TArray<FInventoryContainerConfig>& G) {
		return GetEquipSlotContainerGrants(S, G);
	};

	FInventoryViewHelper::BuildEntriesView(Inventory.Entries, EquippedItems, Callbacks, OutEntries);
}

// SOLID: Delegated to FInventoryViewHelper
void UProjectInventoryComponent::GetContainersView(TArray<FInventoryContainerView>& OutContainers) const
{
	TArray<FInventoryContainerConfig> EffectiveContainers;
	GetEffectiveContainers(EffectiveContainers);

	FInventoryViewHelper::FViewCallbacks Callbacks;
	Callbacks.GetContainerWeight = [this](FGameplayTag C, TMap<FPrimaryAssetId, FItemDataView>& Cache) {
		return GetContainerCurrentWeight(C, Cache);
	};
	Callbacks.GetContainerVolume = [this](FGameplayTag C, TMap<FPrimaryAssetId, FItemDataView>& Cache) {
		return GetContainerCurrentVolume(C, Cache);
	};

	FInventoryViewHelper::BuildContainersView(EffectiveContainers, Callbacks, OutContainers);
}

bool UProjectInventoryComponent::GetItemDataView(FPrimaryAssetId ObjectId, FItemDataView& OutData) const
{
	OutData = FItemDataView();

	if (!ObjectId.IsValid())
	{
		return false;
	}

	auto LogMissingItemDataOnce = [](const FPrimaryAssetId& MissingId, const TCHAR* Reason)
	{
		static TSet<FPrimaryAssetId> LoggedMissingItemData;
		if (MissingId.IsValid() && !LoggedMissingItemData.Contains(MissingId))
		{
			UE_LOG(LogProjectInventory, Warning, TEXT("GetItemDataView: Missing item data for %s (%s)"),
				*MissingId.ToString(), Reason);
			LoggedMissingItemData.Add(MissingId);
		}
	};

	UObject* LoadedObject = nullptr;

	if (ObjectDefinitionCache)
	{
		LoadedObject = ObjectDefinitionCache->GetLoaded(ObjectId);
	}

	if (!LoadedObject)
	{
		// Fallback to direct AssetManager access (may return nullptr if not loaded)
		UAssetManager& AM = UAssetManager::Get();
		LoadedObject = AM.GetPrimaryAssetObject<UObject>(ObjectId);
	}

	if (!LoadedObject)
	{
		LogMissingItemDataOnce(ObjectId, TEXT("not loaded"));
		return false;
	}

	if (!LoadedObject->Implements<UItemDataProvider>())
	{
		LogMissingItemDataOnce(ObjectId, TEXT("no IItemDataProvider"));
		return false;
	}

	OutData = IItemDataProvider::Execute_GetItemDataView(LoadedObject);
	if (!OutData.IsValid())
	{
		LogMissingItemDataOnce(ObjectId, TEXT("invalid data"));
		return false;
	}

	return true;
}

bool UProjectInventoryComponent::IsItemEquipped(int32 InstanceId) const
{
	for (const auto& Pair : EquippedItems)
	{
		if (Pair.Value.InstanceId == InstanceId)
		{
			return true;
		}
	}
	return false;
}

float UProjectInventoryComponent::GetCurrentWeight() const
{
	float TotalWeight = 0.f;
	for (const FInventoryEntry& Entry : Inventory.Entries)
	{
		FItemDataView ItemData;
		if (GetItemDataView(Entry.ItemId, ItemData))
		{
			TotalWeight += ItemData.Weight * Entry.Quantity;
		}
	}
	return TotalWeight;
}

float UProjectInventoryComponent::GetCurrentVolume() const
{
	float TotalVolume = 0.f;
	for (const FInventoryEntry& Entry : Inventory.Entries)
	{
		FItemDataView ItemData;
		if (GetItemDataView(Entry.ItemId, ItemData))
		{
			TotalVolume += ItemData.Volume * Entry.Quantity;
		}
	}
	return TotalVolume;
}

float UProjectInventoryComponent::GetMaxWeight() const
{
	float BaseMax = 0.f;
	TArray<FInventoryContainerConfig> EffectiveContainers;
	GetEffectiveContainers(EffectiveContainers);

	for (const FInventoryContainerConfig& Container : EffectiveContainers)
	{
		if (Container.MaxWeight <= 0.f)
		{
			return 0.f;
		}
		BaseMax += Container.MaxWeight;
	}

	if (BaseMax <= 0.f)
	{
		BaseMax = MaxWeight;
	}

	// Apply GAS state multiplier (fatigue/injury reduces capacity)
	return BaseMax * GetCapacityMultiplier();
}

float UProjectInventoryComponent::GetCapacityMultiplier() const
{
	UAbilitySystemComponent* ASC = GetOwnerASC();
	if (!ASC)
	{
		return 1.0f;
	}

	// Priority order: most severe condition takes precedence
	// Critical fatigue or empty condition: 40% capacity
	if (ASC->HasMatchingGameplayTag(ProjectTags::State_Fatigue_Critical) ||
		ASC->HasMatchingGameplayTag(ProjectTags::State_Condition_Empty))
	{
		return 0.4f;
	}

	// Exhausted or critical condition: 60% capacity
	if (ASC->HasMatchingGameplayTag(ProjectTags::State_Fatigue_Exhausted) ||
		ASC->HasMatchingGameplayTag(ProjectTags::State_Condition_Critical))
	{
		return 0.6f;
	}

	// Tired or low condition: 80% capacity
	if (ASC->HasMatchingGameplayTag(ProjectTags::State_Fatigue_Tired) ||
		ASC->HasMatchingGameplayTag(ProjectTags::State_Condition_Low))
	{
		return 0.8f;
	}

	// Healthy/Rested: full capacity
	return 1.0f;
}

float UProjectInventoryComponent::GetMaxVolume() const
{
	float Total = 0.f;
	TArray<FInventoryContainerConfig> EffectiveContainers;
	GetEffectiveContainers(EffectiveContainers);

	for (const FInventoryContainerConfig& Container : EffectiveContainers)
	{
		if (Container.MaxVolume <= 0.f)
		{
			return 0.f;
		}
		Total += Container.MaxVolume;
	}

	return (Total > 0.f) ? Total : MaxVolume;
}

bool UProjectInventoryComponent::HasSpace() const
{
	TArray<FInventoryContainerConfig> EffectiveContainers;
	GetEffectiveContainers(EffectiveContainers);

	for (const FInventoryContainerConfig& Container : EffectiveContainers)
	{
		if (Container.GridSize.X <= 0 || Container.GridSize.Y <= 0)
		{
			continue;
		}

		FIntPoint FoundPos;
		if (FindFreeGridPos(Container, FIntPoint(1, 1), INDEX_NONE, FoundPos))
		{
			return true;
		}
	}

	return false;
}

int32 UProjectInventoryComponent::GetMaxSlots() const
{
	TArray<FInventoryContainerConfig> EffectiveContainers;
	GetEffectiveContainers(EffectiveContainers);

	int32 TotalSlots = 0;
	for (const FInventoryContainerConfig& Container : EffectiveContainers)
	{
		TotalSlots += GetContainerCellCount(Container);
	}

	return (TotalSlots > 0) ? TotalSlots : MaxSlots;
}

bool UProjectInventoryComponent::HasItemWithTag(FGameplayTag Tag) const
{
	if (!Tag.IsValid())
	{
		return false;
	}

	for (const FInventoryEntry& Entry : Inventory.Entries)
	{
		FItemDataView ItemData;
		if (GetItemDataView(Entry.ItemId, ItemData))
		{
			if (ItemData.Tags.HasTag(Tag))
			{
				return true;
			}
		}
	}
	return false;
}

// -------------------------------------------------------------------------
// Loot Container Support
// -------------------------------------------------------------------------

// SOLID: Delegated to FInventoryLootHelper
bool UProjectInventoryComponent::CanFitItems(const TArray<FLootEntry>& Items) const
{
	TArray<FInventoryContainerConfig> ContainerOrder;
	if (!GetContainerOrder(ContainerOrder))
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("CanFitItems: No containers available"));
		return false;
	}

	FInventoryLootHelper::FSimulationInput Input;
	Input.ContainerOrder = ContainerOrder;
	Input.Entries = Inventory.Entries;
	Input.CurrentWeight = GetCurrentWeight();
	Input.CurrentVolume = GetCurrentVolume();
	Input.MaxWeight = GetMaxWeight();
	Input.MaxVolume = GetMaxVolume();

	FInventoryLootHelper::FSimulationCallbacks Callbacks;
	Callbacks.ResolveItemData = [this](FPrimaryAssetId Id, FItemDataView& Out) { return GetItemDataView(Id, Out); };
	Callbacks.GetEffectivePlacement = [this](const FInventoryEntry& E, FGameplayTag& C, FIntPoint& P, bool& R) {
		return GetEffectiveEntryPlacement(E, C, P, R);
	};
	Callbacks.GetItemGridSize = [this](const FItemDataView& Data, bool bRotated) { return GetItemGridSize(Data, bRotated); };
	Callbacks.ContainerAllowsItem = [this](const FInventoryContainerConfig& C, const FItemDataView& D) {
		return ContainerAllowsItem(C, D);
	};

	return FInventoryLootHelper::CanFitItems(Items, Input, Callbacks);
}

void UProjectInventoryComponent::AddItemsBatch(const TArray<FLootEntry>& Items)
{
	// Server-only
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("AddItemsBatch: Called without authority"));
		return;
	}

	for (const FLootEntry& LootEntry : Items)
	{
		if (!LootEntry.IsValid())
		{
			continue;
		}

		const int32 Added = Internal_AddItem(LootEntry.ObjectId, LootEntry.Quantity);
		if (Added < LootEntry.Quantity)
		{
			UE_LOG(LogProjectInventory, Warning, TEXT("AddItemsBatch: Only added %d/%d of %s"),
				Added, LootEntry.Quantity, *LootEntry.ObjectId.ToString());
		}
	}

	UE_LOG(LogProjectInventory, Log, TEXT("AddItemsBatch: Processed %d loot entries"), Items.Num());
}

bool UProjectInventoryComponent::TryExtractContainerTransferEntry(
	int32 InstanceId,
	int32 Quantity,
	FContainerEntryTransfer& OutEntry,
	FText& OutError)
{
	OutEntry = FContainerEntryTransfer();
	OutError = FText::GetEmpty();

	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		OutError = NSLOCTEXT("ProjectInventory", "ExtractContainerEntryAuthority", "Inventory extraction requires authority.");
		return false;
	}

	FInventoryEntry Entry;
	if (!FindEntry(InstanceId, Entry))
	{
		OutError = NSLOCTEXT("ProjectInventory", "ExtractContainerEntryMissing", "Inventory entry is no longer available.");
		return false;
	}

	const int32 ExtractQuantity = FMath::Clamp(Quantity, 1, Entry.Quantity);
	OutEntry.ObjectId = Entry.ItemId;
	OutEntry.Quantity = ExtractQuantity;

	if (!Internal_RemoveItem(InstanceId, ExtractQuantity))
	{
		OutError = NSLOCTEXT("ProjectInventory", "ExtractContainerEntryRemoveFailed", "Inventory entry could not be removed.");
		OutEntry = FContainerEntryTransfer();
		return false;
	}

	return true;
}

UObject* UProjectInventoryComponent::ResolveWorldContainerSessionSource(AActor* TargetActor) const
{
	if (!TargetActor)
	{
		return nullptr;
	}

	if (TargetActor->Implements<UWorldContainerSessionSource>())
	{
		return TargetActor;
	}

	TInlineComponentArray<UActorComponent*> Components;
	TargetActor->GetComponents(Components);

	for (UActorComponent* Component : Components)
	{
		if (Component && Component->Implements<UWorldContainerSessionSource>())
		{
			return Component;
		}
	}

	return nullptr;
}

AActor* UProjectInventoryComponent::ResolveWorldContainerActor(UObject* WorldContainerSource) const
{
	return ResolveWorldContainerActorFromSource(WorldContainerSource);
}

bool UProjectInventoryComponent::OpenWorldContainerSessionAuthority(
	AActor* TargetActor,
	EContainerSessionMode Mode,
	FContainerSessionHandle& OutHandle,
	FText& OutError)
{
	OutHandle.Reset();
	OutError = FText::GetEmpty();

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		OutError = NSLOCTEXT("ProjectInventory", "OpenWorldSessionAuthorityRequired", "World-container session open requires authority.");
		return false;
	}

	if (!TargetActor)
	{
		OutError = NSLOCTEXT("ProjectInventory", "OpenWorldSessionMissingTarget", "World-container target is required.");
		return false;
	}

	if (ActiveWorldContainerSessionHandle.IsValid())
	{
		OutError = NSLOCTEXT("ProjectInventory", "OpenWorldSessionAlreadyActive", "A world-container session is already active.");
		return false;
	}

	UObject* SourceObject = ResolveWorldContainerSessionSource(TargetActor);
	if (!SourceObject)
	{
		OutError = NSLOCTEXT("ProjectInventory", "OpenWorldSessionMissingSource", "Target does not expose a world-container session source.");
		return false;
	}

	if (!IWorldContainerSessionSource::Execute_SupportsContainerSession(SourceObject, Mode))
	{
		OutError = NSLOCTEXT("ProjectInventory", "OpenWorldSessionUnsupportedMode", "Target does not support the requested world-container mode.");
		return false;
	}

	FContainerSessionHandle Handle;
	Handle.SessionId = FGuid::NewGuid();
	Handle.ContainerKey = IWorldContainerSessionSource::Execute_GetWorldContainerKey(SourceObject);
	Handle.Mode = Mode;
	if (!Handle.IsValid())
	{
		OutError = NSLOCTEXT("ProjectInventory", "OpenWorldSessionInvalidKey", "Target world-container key is invalid.");
		return false;
	}

	if (!IWorldContainerSessionSource::Execute_TryBeginContainerSession(
		SourceObject,
		OwnerActor,
		Handle.SessionId,
		Mode,
		OutError))
	{
		if (OutError.IsEmpty())
		{
			OutError = NSLOCTEXT("ProjectInventory", "OpenWorldSessionRejected", "World-container session open was rejected.");
		}
		return false;
	}

	ActiveWorldContainerSessionHandle = Handle;
	ActiveWorldContainerTargetActor = TargetActor;
	OutHandle = Handle;
	return true;
}

bool UProjectInventoryComponent::CloseWorldContainerSessionAuthority(
	AActor* TargetActor,
	const FContainerSessionHandle& SessionHandle,
	FText& OutError)
{
	OutError = FText::GetEmpty();

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		OutError = NSLOCTEXT("ProjectInventory", "CloseWorldSessionAuthorityRequired", "World-container session close requires authority.");
		return false;
	}

	if (!ActiveWorldContainerSessionHandle.IsValid() || ActiveWorldContainerSessionHandle.SessionId != SessionHandle.SessionId)
	{
		OutError = NSLOCTEXT("ProjectInventory", "CloseWorldSessionUnknown", "World-container session handle is not active.");
		return false;
	}

	if (!TargetActor)
	{
		TargetActor = ActiveWorldContainerTargetActor.Get();
	}

	UObject* SourceObject = ResolveWorldContainerSessionSource(TargetActor);
	if (SourceObject && ActiveWorldContainerSessionHandle.Mode == EContainerSessionMode::FullOpen)
	{
		if (!IWorldContainerSessionSource::Execute_EndContainerSession(SourceObject, SessionHandle.SessionId))
		{
			OutError = NSLOCTEXT("ProjectInventory", "CloseWorldSessionRejected", "World-container session close was rejected.");
			return false;
		}
	}

	ActiveWorldContainerSessionHandle.Reset();
	ActiveWorldContainerTargetActor.Reset();
	return true;
}

bool UProjectInventoryComponent::TakeEntryFromWorldContainerAuthority(
	AActor* TargetActor,
	const FContainerSessionHandle& SessionHandle,
	int32 EntryInstanceId,
	int32 Quantity,
	FGameplayTag TargetContainerId,
	FIntPoint TargetGridPos,
	bool bTargetRotated,
	FText& OutError)
{
	OutError = FText::GetEmpty();

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		OutError = NSLOCTEXT("ProjectInventory", "TakeWorldEntryAuthorityRequired", "World-container take requires authority.");
		return false;
	}

	if (!ActiveWorldContainerSessionHandle.IsValid() || ActiveWorldContainerSessionHandle.SessionId != SessionHandle.SessionId)
	{
		OutError = NSLOCTEXT("ProjectInventory", "TakeWorldEntryUnknownSession", "World-container session handle is not active.");
		return false;
	}

	if (!TargetActor)
	{
		TargetActor = ActiveWorldContainerTargetActor.Get();
	}

	UObject* SourceObject = ResolveWorldContainerSessionSource(TargetActor);
	if (!SourceObject)
	{
		OutError = NSLOCTEXT("ProjectInventory", "TakeWorldEntryMissingSource", "World-container source is no longer valid.");
		return false;
	}

	return TakeEntryFromWorldContainerResolved(
		SourceObject,
		SessionHandle,
		EntryInstanceId,
		Quantity,
		TargetContainerId,
		TargetGridPos,
		bTargetRotated,
		OutError);
}

bool UProjectInventoryComponent::StoreInventoryEntryInWorldContainerAuthority(
	AActor* TargetActor,
	const FContainerSessionHandle& SessionHandle,
	int32 InventoryInstanceId,
	int32 Quantity,
	FIntPoint TargetGridPos,
	bool bTargetRotated,
	FText& OutError)
{
	OutError = FText::GetEmpty();

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		OutError = NSLOCTEXT("ProjectInventory", "StoreWorldEntryAuthorityRequired", "World-container store requires authority.");
		return false;
	}

	if (!ActiveWorldContainerSessionHandle.IsValid() || ActiveWorldContainerSessionHandle.SessionId != SessionHandle.SessionId)
	{
		OutError = NSLOCTEXT("ProjectInventory", "StoreWorldEntryUnknownSession", "World-container session handle is not active.");
		return false;
	}

	if (!TargetActor)
	{
		TargetActor = ActiveWorldContainerTargetActor.Get();
	}

	UObject* SourceObject = ResolveWorldContainerSessionSource(TargetActor);
	if (!SourceObject)
	{
		OutError = NSLOCTEXT("ProjectInventory", "StoreWorldEntryMissingSource", "World-container source is no longer valid.");
		return false;
	}

	return StoreInventoryEntryInWorldContainerResolved(
		SourceObject,
		SessionHandle,
		InventoryInstanceId,
		Quantity,
		TargetGridPos,
		bTargetRotated,
		OutError);
}

bool UProjectInventoryComponent::TakeAllFromWorldContainerAuthority(
	AActor* TargetActor,
	const FContainerSessionHandle& SessionHandle,
	FText& OutError)
{
	OutError = FText::GetEmpty();

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		OutError = NSLOCTEXT("ProjectInventory", "TakeAllWorldAuthorityRequired", "World-container take-all requires authority.");
		return false;
	}

	if (!ActiveWorldContainerSessionHandle.IsValid() || ActiveWorldContainerSessionHandle.SessionId != SessionHandle.SessionId)
	{
		OutError = NSLOCTEXT("ProjectInventory", "TakeAllWorldUnknownSession", "World-container session handle is not active.");
		return false;
	}

	if (!TargetActor)
	{
		TargetActor = ActiveWorldContainerTargetActor.Get();
	}

	UObject* SourceObject = ResolveWorldContainerSessionSource(TargetActor);
	if (!SourceObject)
	{
		OutError = NSLOCTEXT("ProjectInventory", "TakeAllWorldMissingSource", "World-container source is no longer valid.");
		return false;
	}

	return TakeAllFromWorldContainerResolved(SourceObject, SessionHandle, OutError);
}

void UProjectInventoryComponent::CaptureInventoryStateSnapshot(FInventoryStateSnapshot& OutSnapshot) const
{
	OutSnapshot.Entries = Inventory.Entries;
	OutSnapshot.NextInstanceId = Inventory.NextInstanceId;
}

void UProjectInventoryComponent::RestoreInventoryStateSnapshot(const FInventoryStateSnapshot& Snapshot)
{
	Inventory.Entries = Snapshot.Entries;
	Inventory.NextInstanceId = Snapshot.NextInstanceId;
	Inventory.MarkArrayDirty();
	for (FInventoryEntry& Entry : Inventory.Entries)
	{
		Inventory.MarkEntryDirty(Entry);
	}
}

bool UProjectInventoryComponent::TakeEntryFromWorldContainerResolved(
	UObject* SourceObject,
	const FContainerSessionHandle& SessionHandle,
	int32 EntryInstanceId,
	int32 Quantity,
	FGameplayTag TargetContainerId,
	FIntPoint TargetGridPos,
	bool bTargetRotated,
	FText& OutError)
{
	OutError = FText::GetEmpty();

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		OutError = NSLOCTEXT("ProjectInventory", "TakeWorldEntryResolvedAuthorityRequired", "World-container take requires authority.");
		return false;
	}

	if (!SourceObject || !SourceObject->Implements<UWorldContainerSessionSource>())
	{
		OutError = NSLOCTEXT("ProjectInventory", "TakeWorldEntryResolvedMissingSource", "World-container source is no longer valid.");
		return false;
	}

	const TArray<FInventoryEntryView> EntryViews = IWorldContainerSessionSource::Execute_GetContainerEntryViews(SourceObject);
	const FInventoryEntryView* EntryView = EntryViews.FindByPredicate([EntryInstanceId](const FInventoryEntryView& Entry)
	{
		return Entry.InstanceId == EntryInstanceId;
	});

	if (!EntryView)
	{
		OutError = NSLOCTEXT("ProjectInventory", "TakeWorldEntryResolvedMissingEntry", "World-container entry is no longer available.");
		return false;
	}

	const int32 TakeQuantity = FMath::Clamp(Quantity, 1, EntryView->Quantity);
	const bool bHasExplicitPlacement =
		TargetContainerId.IsValid() && TargetGridPos.X >= 0 && TargetGridPos.Y >= 0;

	TArray<FLootEntry> LootItems;
	if (!bHasExplicitPlacement)
	{
		FLootEntry LootItem;
		LootItem.ObjectId = EntryView->ItemId;
		LootItem.Quantity = TakeQuantity;
		LootItems.Add(LootItem);
		if (!CanFitItems(LootItems))
		{
			OutError = NSLOCTEXT("ProjectInventory", "TakeWorldEntryNoSpace", "Inventory does not have enough free space.");
			return false;
		}
	}

	FInventoryStateSnapshot Snapshot;
	CaptureInventoryStateSnapshot(Snapshot);
	const int32 QuantityBefore = GetTotalItemQuantity(Snapshot.Entries, EntryView->ItemId);

	if (bHasExplicitPlacement)
	{
		if (!TryAddItemAtPosition(
				EntryView->ItemId,
				TakeQuantity,
				TargetContainerId,
				TargetGridPos,
				bTargetRotated,
				OutError))
		{
			return false;
		}
	}
	else
	{
		AddItemsBatch(LootItems);
	}

	const int32 QuantityAfter = GetTotalItemQuantity(Inventory.Entries, EntryView->ItemId);
	if (QuantityAfter - QuantityBefore != TakeQuantity)
	{
		RestoreInventoryStateSnapshot(Snapshot);
		OutError = NSLOCTEXT("ProjectInventory", "TakeWorldEntryInventoryMutationMismatch", "Inventory add did not commit the expected quantity.");
		return false;
	}

	FContainerEntryTransfer ConsumeEntry;
	ConsumeEntry.EntryInstanceId = EntryInstanceId;
	ConsumeEntry.ObjectId = EntryView->ItemId;
	ConsumeEntry.Quantity = TakeQuantity;

	TArray<FContainerEntryTransfer> ConsumeEntries;
	ConsumeEntries.Add(ConsumeEntry);
	if (!IWorldContainerSessionSource::Execute_ConsumeContainerEntries(
			SourceObject,
			SessionHandle.SessionId,
			ConsumeEntries,
			OutError))
	{
		RestoreInventoryStateSnapshot(Snapshot);
		if (OutError.IsEmpty())
		{
			OutError = NSLOCTEXT("ProjectInventory", "TakeWorldEntryConsumeRejected", "World-container failed to consume the transferred entry.");
		}
		return false;
	}

	return true;
}

bool UProjectInventoryComponent::StoreInventoryEntryInWorldContainerResolved(
	UObject* SourceObject,
	const FContainerSessionHandle& SessionHandle,
	int32 InventoryInstanceId,
	int32 Quantity,
	FIntPoint TargetGridPos,
	bool bTargetRotated,
	FText& OutError)
{
	OutError = FText::GetEmpty();

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		OutError = NSLOCTEXT("ProjectInventory", "StoreWorldEntryResolvedAuthorityRequired", "World-container store requires authority.");
		return false;
	}

	if (!SourceObject || !SourceObject->Implements<UWorldContainerSessionSource>())
	{
		OutError = NSLOCTEXT("ProjectInventory", "StoreWorldEntryResolvedMissingSource", "World-container source is no longer valid.");
		return false;
	}

	FInventoryEntry InventoryEntry;
	if (!FindEntry(InventoryInstanceId, InventoryEntry))
	{
		OutError = NSLOCTEXT("ProjectInventory", "StoreWorldEntryMissingInventoryEntry", "Inventory entry is no longer available.");
		return false;
	}

	FContainerEntryTransfer CandidateEntry;
	CandidateEntry.ObjectId = InventoryEntry.ItemId;
	CandidateEntry.Quantity = FMath::Clamp(Quantity, 1, InventoryEntry.Quantity);
	if (TargetGridPos.X >= 0 && TargetGridPos.Y >= 0)
	{
		CandidateEntry.GridPos = TargetGridPos;
		CandidateEntry.bRotated = bTargetRotated;
	}

	TArray<FContainerEntryTransfer> CandidateEntries;
	CandidateEntries.Add(CandidateEntry);
	if (!IWorldContainerSessionSource::Execute_CanStoreContainerEntries(
		SourceObject,
		SessionHandle.SessionId,
		CandidateEntries,
		OutError))
	{
		return false;
	}

	FInventoryStateSnapshot Snapshot;
	CaptureInventoryStateSnapshot(Snapshot);

	FContainerEntryTransfer ExtractedEntry;
	if (!TryExtractContainerTransferEntry(InventoryInstanceId, CandidateEntry.Quantity, ExtractedEntry, OutError))
	{
		return false;
	}
	ExtractedEntry.GridPos = CandidateEntry.GridPos;
	ExtractedEntry.bRotated = CandidateEntry.bRotated;

	TArray<FContainerEntryTransfer> StoreEntries;
	StoreEntries.Add(ExtractedEntry);
	if (!IWorldContainerSessionSource::Execute_StoreContainerEntries(
			SourceObject,
			SessionHandle.SessionId,
			StoreEntries,
			OutError))
	{
		RestoreInventoryStateSnapshot(Snapshot);
		if (OutError.IsEmpty())
		{
			OutError = NSLOCTEXT("ProjectInventory", "StoreWorldEntryRejected", "World-container failed to store the extracted inventory entry.");
		}
		return false;
	}

	return true;
}

bool UProjectInventoryComponent::TakeAllFromWorldContainerResolved(
	UObject* SourceObject,
	const FContainerSessionHandle& SessionHandle,
	FText& OutError)
{
	OutError = FText::GetEmpty();

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		OutError = NSLOCTEXT("ProjectInventory", "TakeAllWorldResolvedAuthorityRequired", "World-container take-all requires authority.");
		return false;
	}

	TArray<FContainerEntryTransfer> ConsumeEntries;
	TArray<FLootEntry> LootItems;
	if (!FInventoryWorldContainerTransferHelper::BuildLootEntries(SourceObject, ConsumeEntries, LootItems, OutError))
	{
		return false;
	}

	if (!CanFitItems(LootItems))
	{
		OutError = NSLOCTEXT("ProjectInventory", "TakeAllWorldNoSpace", "Inventory does not have enough free space.");
		return false;
	}

	FInventoryStateSnapshot Snapshot;
	CaptureInventoryStateSnapshot(Snapshot);

	TMap<FPrimaryAssetId, int32> ExpectedQuantities;
	BuildExpectedLootQuantities(LootItems, ExpectedQuantities);

	AddItemsBatch(LootItems);

	for (const TPair<FPrimaryAssetId, int32>& Pair : ExpectedQuantities)
	{
		const int32 QuantityBefore = GetTotalItemQuantity(Snapshot.Entries, Pair.Key);
		const int32 QuantityAfter = GetTotalItemQuantity(Inventory.Entries, Pair.Key);
		if (QuantityAfter - QuantityBefore != Pair.Value)
		{
			RestoreInventoryStateSnapshot(Snapshot);
			OutError = NSLOCTEXT("ProjectInventory", "TakeAllInventoryMutationMismatch", "Inventory add did not commit the expected take-all quantity.");
			return false;
		}
	}

	if (!IWorldContainerSessionSource::Execute_ConsumeContainerEntries(
			SourceObject,
			SessionHandle.SessionId,
			ConsumeEntries,
			OutError))
	{
		RestoreInventoryStateSnapshot(Snapshot);
		if (OutError.IsEmpty())
		{
			OutError = NSLOCTEXT("ProjectInventory", "TakeAllConsumeRejected", "World-container failed to consume transferred entries.");
		}
		return false;
	}

	return true;
}

bool UProjectInventoryComponent::TryAddItemAtPosition(
	FPrimaryAssetId ObjectId,
	int32 Quantity,
	FGameplayTag TargetContainerId,
	FIntPoint TargetGridPos,
	bool bTargetRotated,
	FText& OutError)
{
	OutError = FText::GetEmpty();

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		OutError = NSLOCTEXT("ProjectInventory", "TryAddAtPositionAuthorityRequired", "Exact inventory placement requires authority.");
		return false;
	}

	if (!ObjectId.IsValid() || Quantity <= 0)
	{
		OutError = NSLOCTEXT("ProjectInventory", "TryAddAtPositionInvalidPayload", "Target item payload is invalid.");
		return false;
	}

	if (!TargetContainerId.IsValid() || TargetGridPos.X < 0 || TargetGridPos.Y < 0)
	{
		OutError = NSLOCTEXT("ProjectInventory", "TryAddAtPositionInvalidTarget", "Target inventory placement is invalid.");
		return false;
	}

	FItemDataView ItemData;
	if (!GetItemDataView(ObjectId, ItemData))
	{
		OutError = NSLOCTEXT("ProjectInventory", "TryAddAtPositionMissingItemData", "Target item data is missing.");
		return false;
	}

	FInventoryContainerConfig TargetContainer;
	if (!GetContainerConfig(TargetContainerId, TargetContainer))
	{
		OutError = NSLOCTEXT("ProjectInventory", "TryAddAtPositionMissingContainer", "Target inventory container is unavailable.");
		return false;
	}

	if (!ContainerAllowsItem(TargetContainer, ItemData))
	{
		OutError = NSLOCTEXT("ProjectInventory", "TryAddAtPositionRejectedByContainer", "Target inventory container does not accept this item.");
		return false;
	}

	const int32 AllowedQuantity = FInventoryStackHelper::CalculateAllowedQuantity(
		ItemData,
		GetMaxWeight(),
		GetMaxVolume(),
		GetCurrentWeight(),
		GetCurrentVolume(),
		Quantity);
	if (AllowedQuantity < Quantity)
	{
		OutError = NSLOCTEXT("ProjectInventory", "TryAddAtPositionCapacityExceeded", "Inventory does not have enough carrying capacity.");
		return false;
	}

	TMap<FPrimaryAssetId, FItemDataView> ItemDataCache;
	ItemDataCache.Add(ObjectId, ItemData);
	if (TargetContainer.MaxWeight > 0.f)
	{
		const float TargetWeight = GetContainerCurrentWeight(TargetContainerId, ItemDataCache);
		if (TargetWeight + (ItemData.Weight * Quantity) > TargetContainer.MaxWeight)
		{
			OutError = NSLOCTEXT("ProjectInventory", "TryAddAtPositionWeightExceeded", "Target inventory container would exceed its weight limit.");
			return false;
		}
	}

	if (TargetContainer.MaxVolume > 0.f)
	{
		const float TargetVolume = GetContainerCurrentVolume(TargetContainerId, ItemDataCache);
		if (TargetVolume + (ItemData.Volume * Quantity) > TargetContainer.MaxVolume)
		{
			OutError = NSLOCTEXT("ProjectInventory", "TryAddAtPositionVolumeExceeded", "Target inventory container would exceed its volume limit.");
			return false;
		}
	}

	const bool bEffectiveRotated = bTargetRotated && TargetContainer.bAllowRotation;
	const FIntPoint ItemSize = GetItemGridSize(ItemData, bEffectiveRotated);
	if (!IsRectWithinContainer(TargetContainer, TargetGridPos, ItemSize))
	{
		OutError = NSLOCTEXT("ProjectInventory", "TryAddAtPositionOutOfBounds", "Target inventory placement is outside the container grid.");
		return false;
	}

	FInventoryMoveHelper::FMoveCallbacks MoveCallbacks;
	MoveCallbacks.GetEffectivePlacement = [this](const FInventoryEntry& E, FGameplayTag& C, FIntPoint& P, bool& R) {
		return GetEffectiveEntryPlacement(E, C, P, R);
	};
	MoveCallbacks.GetItemDataView = [this](FPrimaryAssetId Id, FItemDataView& D) {
		return GetItemDataView(Id, D);
	};
	MoveCallbacks.GetItemGridSize = [this](const FItemDataView& D, bool R) {
		return GetItemGridSize(D, R);
	};

	const FInventoryMoveHelper::FOverlapResult OverlapResult = FInventoryMoveHelper::FindOverlapAtTarget(
		Inventory.Entries,
		TargetContainerId,
		TargetGridPos,
		ItemSize,
		INDEX_NONE,
		MoveCallbacks);
	if (OverlapResult.bMultipleOverlaps)
	{
		OutError = NSLOCTEXT("ProjectInventory", "TryAddAtPositionMultipleOverlaps", "Target inventory placement overlaps multiple entries.");
		return false;
	}

	if (OverlapResult.bHasOverlap)
	{
		FInventoryEntry* OverlapEntry = Inventory.FindEntry(OverlapResult.OverlapInstanceId);
		const int32 MaxStack = FMath::Max(1, ItemData.MaxStack);
		if (!OverlapEntry
			|| OverlapEntry->ItemId != ObjectId
			|| OverlapEntry->OverrideMagnitudes.Num() > 0
			|| MaxStack <= 1
			|| OverlapEntry->Quantity + Quantity > MaxStack)
		{
			OutError = NSLOCTEXT("ProjectInventory", "TryAddAtPositionOverlapRejected", "Target inventory entry cannot accept this stack.");
			return false;
		}

		OverlapEntry->Quantity += Quantity;
		Inventory.MarkEntryDirty(*OverlapEntry);
		return true;
	}

	const int32 SlotIndex = ComputeSlotIndex(TargetContainerId, TargetGridPos);
	Inventory.AddEntry(ObjectId, Quantity, TargetContainerId, TargetGridPos, bEffectiveRotated, SlotIndex);
	return true;
}

void UProjectInventoryComponent::HandleWorldContainerSessionOpenedLocal(
	AActor* TargetActor,
	const FContainerSessionHandle& SessionHandle)
{
	if (!TargetActor || !SessionHandle.IsValid())
	{
		return;
	}

	UObject* SourceObject = ResolveWorldContainerSessionSource(TargetActor);
	if (!SourceObject)
	{
		return;
	}

	if (UProjectContainerSessionSubsystem* SessionSubsystem = ResolveLocalPlayerSessionSubsystem(this))
	{
		FText RegisterError;
		if (!SessionSubsystem->RegisterOpenedSession(TargetActor, SourceObject, GetOwner(), SessionHandle, RegisterError))
		{
			if (!RegisterError.IsEmpty())
			{
				BroadcastErrorLocal(RegisterError);
			}
			return;
		}
	}

	WorldContainerSessionOpenedNative.Broadcast(SourceObject, SessionHandle);
}

void UProjectInventoryComponent::HandleWorldContainerSessionClosedLocal(const FContainerSessionHandle& SessionHandle)
{
	if (!SessionHandle.IsValid())
	{
		return;
	}

	if (UProjectContainerSessionSubsystem* SessionSubsystem = ResolveLocalPlayerSessionSubsystem(this))
	{
		SessionSubsystem->CloseSessionLocal(SessionHandle);
	}

	WorldContainerSessionClosedNative.Broadcast(SessionHandle);
}

bool UProjectInventoryComponent::RequestOpenWorldContainerSession_Implementation(
	AActor* TargetActor,
	EContainerSessionMode Mode,
	FText& OutError)
{
	OutError = FText::GetEmpty();

	if (!TargetActor)
	{
		OutError = NSLOCTEXT("ProjectInventory", "RequestOpenWorldSessionMissingTarget", "World-container target is required.");
		return false;
	}

	AActor* OwnerActor = GetOwner();
	if (OwnerActor && OwnerActor->HasAuthority())
	{
		FContainerSessionHandle Handle;
		if (!OpenWorldContainerSessionAuthority(TargetActor, Mode, Handle, OutError))
		{
			return false;
		}

		HandleWorldContainerSessionOpenedLocal(TargetActor, Handle);
		return true;
	}

	Server_RequestOpenWorldContainerSession(TargetActor, Mode);
	return true;
}

bool UProjectInventoryComponent::RequestCloseWorldContainerSession_Implementation(
	const FContainerSessionHandle& SessionHandle,
	FText& OutError)
{
	OutError = FText::GetEmpty();

	if (!SessionHandle.IsValid())
	{
		OutError = NSLOCTEXT("ProjectInventory", "RequestCloseWorldSessionInvalidHandle", "World-container session handle is invalid.");
		return false;
	}

	AActor* TargetActor = ActiveWorldContainerTargetActor.Get();
	AActor* OwnerActor = GetOwner();
	if (OwnerActor && OwnerActor->HasAuthority())
	{
		if (!CloseWorldContainerSessionAuthority(TargetActor, SessionHandle, OutError))
		{
			return false;
		}

		HandleWorldContainerSessionClosedLocal(SessionHandle);
		return true;
	}

	Server_RequestCloseWorldContainerSession(TargetActor, SessionHandle);
	return true;
}

bool UProjectInventoryComponent::GetActiveWorldContainerSession_Implementation(
	AActor*& OutTargetActor,
	FContainerSessionHandle& OutSessionHandle) const
{
	OutTargetActor = ActiveWorldContainerTargetActor.Get();
	OutSessionHandle = ActiveWorldContainerSessionHandle;
	return OutTargetActor != nullptr && OutSessionHandle.IsValid();
}

bool UProjectInventoryComponent::TransferWorldContainerEntryToInventory_Implementation(
	UObject* WorldContainerSource,
	const FContainerSessionHandle& SessionHandle,
	int32 EntryInstanceId,
	int32 Quantity,
	FGameplayTag TargetContainerId,
	FIntPoint TargetGridPos,
	bool bTargetRotated,
	FText& OutError)
{
	AActor* TargetActor = ResolveWorldContainerActor(WorldContainerSource);
	if (!TargetActor)
	{
		OutError = NSLOCTEXT("ProjectInventory", "MissingWorldContainerActorForTake", "World-container actor is unavailable.");
		return false;
	}

	AActor* OwnerActor = GetOwner();
	if (OwnerActor && OwnerActor->HasAuthority())
	{
		if (ActiveWorldContainerSessionHandle.IsValid()
			&& ActiveWorldContainerSessionHandle.SessionId == SessionHandle.SessionId)
		{
			return TakeEntryFromWorldContainerAuthority(
				TargetActor,
				SessionHandle,
				EntryInstanceId,
				Quantity,
				TargetContainerId,
				TargetGridPos,
				bTargetRotated,
				OutError);
		}

		if (UProjectContainerSessionSubsystem* SessionSubsystem = ResolveLocalPlayerSessionSubsystem(this))
		{
			if (SessionSubsystem->IsSessionActive(SessionHandle))
			{
				return SessionSubsystem->TakeEntryFromWorldContainerSession(
					SessionHandle,
					this,
					EntryInstanceId,
					Quantity,
					TargetContainerId,
					TargetGridPos,
					bTargetRotated,
					OutError);
			}
		}

		return TakeEntryFromWorldContainerAuthority(
			TargetActor,
			SessionHandle,
			EntryInstanceId,
			Quantity,
			TargetContainerId,
			TargetGridPos,
			bTargetRotated,
			OutError);
	}

	Server_RequestTakeEntryFromWorldContainer(
		TargetActor,
		SessionHandle,
		EntryInstanceId,
		Quantity,
		TargetContainerId,
		TargetGridPos,
		bTargetRotated);
	OutError = FText::GetEmpty();
	return true;
}

bool UProjectInventoryComponent::StoreInventoryEntryInWorldContainer_Implementation(
	UObject* WorldContainerSource,
	const FContainerSessionHandle& SessionHandle,
	int32 InventoryInstanceId,
	int32 Quantity,
	FIntPoint TargetGridPos,
	bool bTargetRotated,
	FText& OutError)
{
	AActor* TargetActor = ResolveWorldContainerActor(WorldContainerSource);
	if (!TargetActor)
	{
		OutError = NSLOCTEXT("ProjectInventory", "MissingWorldContainerActorForStore", "World-container actor is unavailable.");
		return false;
	}

	AActor* OwnerActor = GetOwner();
	if (OwnerActor && OwnerActor->HasAuthority())
	{
		if (ActiveWorldContainerSessionHandle.IsValid()
			&& ActiveWorldContainerSessionHandle.SessionId == SessionHandle.SessionId)
		{
			return StoreInventoryEntryInWorldContainerAuthority(
				TargetActor,
				SessionHandle,
				InventoryInstanceId,
				Quantity,
				TargetGridPos,
				bTargetRotated,
				OutError);
		}

		if (UProjectContainerSessionSubsystem* SessionSubsystem = ResolveLocalPlayerSessionSubsystem(this))
		{
			if (SessionSubsystem->IsSessionActive(SessionHandle))
			{
				return SessionSubsystem->StoreInventoryEntryInWorldContainerSession(
					SessionHandle,
					this,
					InventoryInstanceId,
					Quantity,
					TargetGridPos,
					bTargetRotated,
					OutError);
			}
		}

		return StoreInventoryEntryInWorldContainerAuthority(
			TargetActor,
			SessionHandle,
			InventoryInstanceId,
			Quantity,
			TargetGridPos,
			bTargetRotated,
			OutError);
	}

	Server_RequestStoreInventoryEntryInWorldContainer(
		TargetActor,
		SessionHandle,
		InventoryInstanceId,
		Quantity,
		TargetGridPos,
		bTargetRotated);
	OutError = FText::GetEmpty();
	return true;
}

bool UProjectInventoryComponent::TakeAllFromWorldContainer_Implementation(
	UObject* WorldContainerSource,
	const FContainerSessionHandle& SessionHandle,
	FText& OutError)
{
	AActor* TargetActor = ResolveWorldContainerActor(WorldContainerSource);
	if (!TargetActor)
	{
		OutError = NSLOCTEXT("ProjectInventory", "MissingWorldContainerActorForTakeAll", "World-container actor is unavailable.");
		return false;
	}

	AActor* OwnerActor = GetOwner();
	if (OwnerActor && OwnerActor->HasAuthority())
	{
		if (UProjectContainerSessionSubsystem* SessionSubsystem = ResolveLocalPlayerSessionSubsystem(this))
		{
			if (SessionSubsystem->IsSessionActive(SessionHandle))
			{
				return SessionSubsystem->TakeAllFromWorldContainerSession(SessionHandle, this, OutError);
			}
		}

		return TakeAllFromWorldContainerAuthority(TargetActor, SessionHandle, OutError);
	}

	Server_RequestTakeAllFromWorldContainer(TargetActor, SessionHandle);
	OutError = FText::GetEmpty();
	return true;
}

// -------------------------------------------------------------------------
// FFastArraySerializer Callbacks
// -------------------------------------------------------------------------

void UProjectInventoryComponent::OnEntryAdded(const FInventoryEntry& Entry)
{
	UE_LOG(LogProjectInventory, Log, TEXT("Entry added: InstanceId=%d, ObjectId=%s, Qty=%d"),
		Entry.InstanceId, *Entry.ItemId.ToString(), Entry.Quantity);
	OnItemAdded.Broadcast(Entry.ItemId, Entry.Quantity);
	OnInventoryChanged.Broadcast(this);
	InventoryViewChanged.Broadcast();
	UpdateWeightStateTag();
}

void UProjectInventoryComponent::OnEntryRemoved(const FInventoryEntry& Entry)
{
	UE_LOG(LogProjectInventory, Log, TEXT("Entry removed: InstanceId=%d, ObjectId=%s"),
		Entry.InstanceId, *Entry.ItemId.ToString());
	OnItemRemoved.Broadcast(Entry.ItemId, Entry.Quantity);
	OnInventoryChanged.Broadcast(this);
	InventoryViewChanged.Broadcast();
	UpdateWeightStateTag();
}

void UProjectInventoryComponent::OnEntryChanged(const FInventoryEntry& Entry)
{
	UE_LOG(LogProjectInventory, Verbose, TEXT("Entry changed: InstanceId=%d, ObjectId=%s, Qty=%d"),
		Entry.InstanceId, *Entry.ItemId.ToString(), Entry.Quantity);
	OnInventoryChanged.Broadcast(this);
	InventoryViewChanged.Broadcast();
	UpdateWeightStateTag();
}

// -------------------------------------------------------------------------
// Server RPCs
// -------------------------------------------------------------------------

void UProjectInventoryComponent::Server_RequestOpenWorldContainerSession_Implementation(
	AActor* TargetActor,
	EContainerSessionMode Mode)
{
	FText OpenError;
	FContainerSessionHandle Handle;
	if (!OpenWorldContainerSessionAuthority(TargetActor, Mode, Handle, OpenError))
	{
		if (!OpenError.IsEmpty())
		{
			BroadcastError(OpenError);
		}
		return;
	}

	Client_WorldContainerSessionOpened(TargetActor, Handle);
}

void UProjectInventoryComponent::Server_RequestCloseWorldContainerSession_Implementation(
	AActor* TargetActor,
	FContainerSessionHandle SessionHandle)
{
	FText CloseError;
	if (!CloseWorldContainerSessionAuthority(TargetActor, SessionHandle, CloseError))
	{
		if (!CloseError.IsEmpty())
		{
			BroadcastError(CloseError);
		}
		return;
	}

	Client_WorldContainerSessionClosed(SessionHandle);
}

void UProjectInventoryComponent::Server_RequestTakeEntryFromWorldContainer_Implementation(
	AActor* TargetActor,
	FContainerSessionHandle SessionHandle,
	int32 EntryInstanceId,
	int32 Quantity,
	FGameplayTag TargetContainerId,
	FIntPoint TargetGridPos,
	bool bTargetRotated)
{
	FText TakeError;
	if (!TakeEntryFromWorldContainerAuthority(
			TargetActor,
			SessionHandle,
			EntryInstanceId,
			Quantity,
			TargetContainerId,
			TargetGridPos,
			bTargetRotated,
			TakeError))
	{
		if (!TakeError.IsEmpty())
		{
			BroadcastError(TakeError);
		}
		return;
	}

	InventoryViewChanged.Broadcast();
}

void UProjectInventoryComponent::Server_RequestStoreInventoryEntryInWorldContainer_Implementation(
	AActor* TargetActor,
	FContainerSessionHandle SessionHandle,
	int32 InventoryInstanceId,
	int32 Quantity,
	FIntPoint TargetGridPos,
	bool bTargetRotated)
{
	FText StoreError;
	if (!StoreInventoryEntryInWorldContainerAuthority(
			TargetActor,
			SessionHandle,
			InventoryInstanceId,
			Quantity,
			TargetGridPos,
			bTargetRotated,
			StoreError))
	{
		if (!StoreError.IsEmpty())
		{
			BroadcastError(StoreError);
		}
		return;
	}

	InventoryViewChanged.Broadcast();
}

void UProjectInventoryComponent::Server_RequestTakeAllFromWorldContainer_Implementation(
	AActor* TargetActor,
	FContainerSessionHandle SessionHandle)
{
	FText TakeAllError;
	if (!TakeAllFromWorldContainerAuthority(TargetActor, SessionHandle, TakeAllError))
	{
		if (!TakeAllError.IsEmpty())
		{
			BroadcastError(TakeAllError);
		}
		return;
	}

	InventoryViewChanged.Broadcast();
}

void UProjectInventoryComponent::Server_AddItem_Implementation(FPrimaryAssetId ObjectId, int32 Quantity)
{
	Internal_AddItem(ObjectId, Quantity);
	// FFastArraySerializer callbacks only fire on receiving clients, not on
	// the authority (listen server / standalone). Broadcast explicitly so
	// local ViewModel listeners refresh immediately.
	UE_LOG(LogProjectInventory, Verbose, TEXT("Server_AddItem: authority broadcast"));
	InventoryViewChanged.Broadcast();
}

void UProjectInventoryComponent::Server_RemoveItem_Implementation(int32 InstanceId, int32 Quantity)
{
	Internal_RemoveItem(InstanceId, Quantity);
	UE_LOG(LogProjectInventory, Verbose, TEXT("Server_RemoveItem: authority broadcast"));
	InventoryViewChanged.Broadcast();
}

void UProjectInventoryComponent::Server_MoveItem_Implementation(int32 InstanceId, FGameplayTag FromContainer, FIntPoint FromPos, FGameplayTag ToContainer, FIntPoint ToPos, int32 Quantity, bool bRotated)
{
	Internal_MoveItem(InstanceId, FromContainer, FromPos, ToContainer, ToPos, Quantity, bRotated);
	UE_LOG(LogProjectInventory, Verbose, TEXT("Server_MoveItem: authority broadcast"));
	InventoryViewChanged.Broadcast();
}

void UProjectInventoryComponent::Server_UseItem_Implementation(int32 InstanceId)
{
	Internal_UseItem(InstanceId);
	UE_LOG(LogProjectInventory, Verbose, TEXT("Server_UseItem: authority broadcast"));
	InventoryViewChanged.Broadcast();
}

void UProjectInventoryComponent::Server_EquipItem_Implementation(int32 InstanceId, FGameplayTag EquipSlot)
{
	Internal_EquipItem(InstanceId, EquipSlot);
	UE_LOG(LogProjectInventory, Verbose, TEXT("Server_EquipItem: authority broadcast"));
	InventoryViewChanged.Broadcast();
}

void UProjectInventoryComponent::Server_UnequipItem_Implementation(FGameplayTag EquipSlot)
{
	Internal_UnequipItem(EquipSlot);
	UE_LOG(LogProjectInventory, Verbose, TEXT("Server_UnequipItem: authority broadcast"));
	InventoryViewChanged.Broadcast();
}

void UProjectInventoryComponent::Server_DropItem_Implementation(int32 InstanceId, int32 Quantity)
{
	FInventoryEntry* Entry = Inventory.FindEntry(InstanceId);
	if (!Entry)
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("Server_DropItem: InstanceId %d not found"), InstanceId);
		return;
	}

	FItemDataView ItemData;
	if (!GetItemDataView(Entry->ItemId, ItemData) || !ItemData.bCanBeDropped)
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("Server_DropItem: Item cannot be dropped"));
		return;
	}

	// Clamp quantity to available
	const int32 DropQuantity = FMath::Min(Quantity, Entry->Quantity);
	if (DropQuantity <= 0)
	{
		return;
	}

	// Copy ItemId before any modification (Entry* may become invalid)
	const FPrimaryAssetId DroppedItemId = Entry->ItemId;

	// Calculate drop transform (in front of owner)
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	FVector DropLocation = Owner->GetActorLocation();
	FRotator DropRotation = Owner->GetActorRotation();

	// Offset forward and down
	const FVector ForwardOffset = Owner->GetActorForwardVector() * 100.f;
	const FVector DownOffset = FVector(0.f, 0.f, -50.f);
	DropLocation += ForwardOffset + DownOffset;

	const FTransform DropTransform(DropRotation, DropLocation);

	// Resolve spawn service (lazy-load module if needed)
	TSharedPtr<IObjectSpawnService> SpawnService = FProjectServiceLocator::Resolve<IObjectSpawnService>();
	if (!SpawnService)
	{
		// Try loading module (string-only, no compile-time dependency)
		FModuleManager::Get().LoadModule(TEXT("ProjectObject"));
		SpawnService = FProjectServiceLocator::Resolve<IObjectSpawnService>();
	}
	if (!SpawnService)
	{
		UE_LOG(LogProjectInventory, Error, TEXT("Server_DropItem: IObjectSpawnService not available after module load"));
		return;
	}

	// Spawn BEFORE removing (transactional - don't lose item on spawn failure)
	FText SpawnError;
	AActor* DroppedActor = SpawnService->SpawnFromDefinition(
		GetWorld(),
		DroppedItemId,
		DropTransform,
		&SpawnError
	);

	if (!DroppedActor)
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("Server_DropItem: Spawn failed - %s (item NOT removed)"), *SpawnError.ToString());
		return;
	}

	auto FindPickupSourceOnActor = [](AActor* Actor) -> UObject*
	{
		if (!Actor)
		{
			return nullptr;
		}

		if (Actor->Implements<UPickupSource>())
		{
			return Actor;
		}

		TInlineComponentArray<UActorComponent*> Components;
		Actor->GetComponents(Components);
		for (UActorComponent* Component : Components)
		{
			if (Component && Component->Implements<UPickupSource>())
			{
				return Component;
			}
		}

		return nullptr;
	};

	if (UObject* PickupSource = FindPickupSourceOnActor(DroppedActor))
	{
		IPickupSource::Execute_SetQuantity(PickupSource, DropQuantity);
	}

	// Spawn succeeded - now remove from inventory
	Internal_RemoveItem(InstanceId, DropQuantity);

	UE_LOG(LogProjectInventory, Log, TEXT("Dropped %d x %s at %s"),
		DropQuantity, *DroppedItemId.ToString(), *DropLocation.ToString());
	UE_LOG(LogProjectInventory, Verbose, TEXT("Server_DropItem: authority broadcast"));
	InventoryViewChanged.Broadcast();
}

void UProjectInventoryComponent::Server_SwapHands_Implementation()
{
	bool bChanged = false;

	for (FInventoryEntry& Entry : Inventory.Entries)
	{
		if (Entry.ContainerId == ProjectTags::Item_Container_LeftHand)
		{
			Entry.ContainerId = ProjectTags::Item_Container_RightHand;
			Inventory.MarkEntryDirty(Entry);
			bChanged = true;
		}
		else if (Entry.ContainerId == ProjectTags::Item_Container_RightHand)
		{
			Entry.ContainerId = ProjectTags::Item_Container_LeftHand;
			Inventory.MarkEntryDirty(Entry);
			bChanged = true;
		}
		else if (Entry.ContainerId == ProjectTags::Item_Container_Hands)
		{
			Entry.GridPos = FIntPoint(Entry.GridPos.X == 0 ? 1 : 0, Entry.GridPos.Y);
			Inventory.MarkEntryDirty(Entry);
			bChanged = true;
		}
	}

	if (!bChanged)
	{
		return;
	}

	UE_LOG(LogProjectInventory, Log, TEXT("Server_SwapHands: Swapped hand items"));
	UE_LOG(LogProjectInventory, Verbose, TEXT("Server_SwapHands: authority broadcast"));
	InventoryViewChanged.Broadcast();
}

// -------------------------------------------------------------------------
// Internal Implementation
// -------------------------------------------------------------------------

int32 UProjectInventoryComponent::TryAddItem(FPrimaryAssetId ObjectId, int32 Quantity)
{
	// Server-only: public API must enforce authority
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("TryAddItem: Called without authority"));
		return 0;
	}

	return Internal_AddItem(ObjectId, Quantity);
}

// SOLID: Uses FInventoryStackHelper + FInventoryAddHelper for placement
uint32 UProjectInventoryComponent::TryAddItemWithOverrides(FPrimaryAssetId ObjectId, int32 Quantity, const TArray<FMagnitudeOverride>& Overrides)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return 0;
	}

	if (!ObjectId.IsValid() || Quantity <= 0)
	{
		return 0;
	}

	FItemDataView ItemData;
	if (!GetItemDataView(ObjectId, ItemData))
	{
		return 0;
	}

	// Calculate allowed quantity
	const int32 AllowedQuantity = FInventoryStackHelper::CalculateAllowedQuantity(
		ItemData, GetMaxWeight(), GetMaxVolume(), GetCurrentWeight(), GetCurrentVolume(), Quantity);
	if (AllowedQuantity <= 0)
	{
		return 0;
	}

	// Build container states and find placement (no stacking - items with overrides are unique)
	TArray<FInventoryContainerConfig> ContainerOrder;
	if (!GetContainerOrder(ContainerOrder))
	{
		return 0;
	}

	TMap<FPrimaryAssetId, FItemDataView> ItemDataCache;
	ItemDataCache.Add(ObjectId, ItemData);

	TArray<FInventoryAddHelper::FContainerState> ContainerStates;
	for (const FInventoryContainerConfig& Container : ContainerOrder)
	{
		FInventoryAddHelper::FContainerState State;
		State.Config = Container;
		State.CurrentWeight = GetContainerCurrentWeight(Container.ContainerId, ItemDataCache);
		State.CurrentVolume = GetContainerCurrentVolume(Container.ContainerId, ItemDataCache);
		ContainerStates.Add(State);
	}

	FInventoryAddHelper::FAddCallbacks Callbacks;
	Callbacks.FindFreeGridPos = [this](const FInventoryContainerConfig& C, FIntPoint S, int32 I, FIntPoint& P) {
		return FindFreeGridPos(C, S, I, P);
	};
	Callbacks.ContainerAllowsItem = [this](const FInventoryContainerConfig& C, const FItemDataView& D) {
		return ContainerAllowsItem(C, D);
	};

	// Add entries one at a time to avoid overlapping placements
	const int32 MaxStack = FMath::Max(1, ItemData.MaxStack);
	uint32 FirstInstanceId = 0;
	int32 RemainingQuantity = AllowedQuantity;

	while (RemainingQuantity > 0)
	{
		TArray<FInventoryAddHelper::FNewStackPlacement> Placements;
		FInventoryAddHelper::CalculateNewPlacements(ItemData, RemainingQuantity, MaxStack, ContainerStates, Callbacks, Placements);

		if (Placements.Num() == 0)
		{
			break;
		}

		const FInventoryAddHelper::FNewStackPlacement& Placement = Placements[0];
		const int32 SlotIndex = ComputeSlotIndex(Placement.ContainerId, Placement.GridPos);
		const uint32 InstanceId = Inventory.AddEntry(ObjectId, Placement.Quantity, Placement.ContainerId, Placement.GridPos, Placement.bRotated, SlotIndex);

		if (FInventoryEntry* Entry = Inventory.FindEntry(InstanceId))
		{
			Entry->OverrideMagnitudes = Overrides;
			Inventory.MarkEntryDirty(*Entry);
		}

		if (FirstInstanceId == 0)
		{
			FirstInstanceId = InstanceId;
		}

		RemainingQuantity -= Placement.Quantity;
		UE_LOG(LogProjectInventory, Log, TEXT("Added item with overrides: %d x %s (InstanceId: %d, %d overrides)"),
			Placement.Quantity, *ObjectId.ToString(), InstanceId, Overrides.Num());
	}

	return FirstInstanceId;
}

// SOLID: Uses FInventoryAddHelper for stacking/placement computation
int32 UProjectInventoryComponent::Internal_AddItem(FPrimaryAssetId ObjectId, int32 Quantity)
{
	if (!ObjectId.IsValid() || Quantity <= 0)
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("Internal_AddItem: Invalid ObjectId or Quantity"));
		return 0;
	}

	FItemDataView ItemData;
	if (!GetItemDataView(ObjectId, ItemData))
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("Internal_AddItem: Missing item data for %s"), *ObjectId.ToString());
		return 0;
	}

	// Calculate allowed quantity based on weight/volume
	const int32 AllowedQuantity = FInventoryStackHelper::CalculateAllowedQuantity(
		ItemData, GetMaxWeight(), GetMaxVolume(), GetCurrentWeight(), GetCurrentVolume(), Quantity);

	UE_LOG(LogProjectInventory, Log, TEXT("Internal_AddItem: %s x%d (size: %dx%d, weight: %.2f) -> allowed: %d"),
		*ObjectId.ToString(), Quantity, ItemData.GridSize.X, ItemData.GridSize.Y, ItemData.Weight, AllowedQuantity);

	if (AllowedQuantity <= 0)
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("Internal_AddItem: No capacity for %s (weight: %.2f/%.2f, volume: %.2f/%.2f)"),
			*ObjectId.ToString(), GetCurrentWeight(), GetMaxWeight(), GetCurrentVolume(), GetMaxVolume());
		BroadcastError(NSLOCTEXT("Inventory", "NoCapacity", "Cannot carry more"));
		return 0;
	}

	TArray<FInventoryContainerConfig> ContainerOrder;
	if (!GetContainerOrder(ContainerOrder))
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("Internal_AddItem: No containers available"));
		return 0;
	}

	// Build container states for helper
	TMap<FPrimaryAssetId, FItemDataView> ItemDataCache;
	ItemDataCache.Add(ObjectId, ItemData);

	TArray<FInventoryAddHelper::FContainerState> ContainerStates;
	ContainerStates.Reserve(ContainerOrder.Num());
	for (const FInventoryContainerConfig& Container : ContainerOrder)
	{
		FInventoryAddHelper::FContainerState State;
		State.Config = Container;
		State.CurrentWeight = GetContainerCurrentWeight(Container.ContainerId, ItemDataCache);
		State.CurrentVolume = GetContainerCurrentVolume(Container.ContainerId, ItemDataCache);
		ContainerStates.Add(State);

		UE_LOG(LogProjectInventory, Log, TEXT("  Container: %s (%dx%d, slot:%d, w:%.1f/%.1f, v:%.1f/%.1f)"),
			*Container.ContainerId.ToString(), Container.GridSize.X, Container.GridSize.Y,
			Container.bSlotBased ? 1 : 0,
			State.CurrentWeight, Container.MaxWeight, State.CurrentVolume, Container.MaxVolume);
	}

	// Setup callbacks
	FInventoryAddHelper::FAddCallbacks Callbacks;
	Callbacks.GetEffectivePlacement = [this](const FInventoryEntry& E, FGameplayTag& C, FIntPoint& P, bool& R) {
		return GetEffectiveEntryPlacement(E, C, P, R);
	};
	Callbacks.FindFreeGridPos = [this](const FInventoryContainerConfig& C, FIntPoint S, int32 I, FIntPoint& P) {
		return FindFreeGridPos(C, S, I, P);
	};
	Callbacks.ContainerAllowsItem = [this](const FInventoryContainerConfig& C, const FItemDataView& D) {
		return ContainerAllowsItem(C, D);
	};

	const int32 MaxStack = FMath::Max(1, ItemData.MaxStack);
	const bool bIsStackable = MaxStack > 1;
	int32 RemainingQuantity = AllowedQuantity;

	// Phase 1: Stack with existing entries
	if (bIsStackable)
	{
		TArray<FInventoryAddHelper::FStackTarget> StackTargets;
		RemainingQuantity = FInventoryAddHelper::CalculateStackTargets(
			ObjectId, ItemData, AllowedQuantity, MaxStack, Inventory.Entries, ContainerStates, Callbacks, StackTargets);

		// Apply stack targets to state
		for (const FInventoryAddHelper::FStackTarget& Target : StackTargets)
		{
			if (FInventoryEntry* Entry = Inventory.FindEntry(Target.InstanceId))
			{
				Entry->Quantity += Target.Quantity;
				Inventory.MarkEntryDirty(*Entry);
				UE_LOG(LogProjectInventory, Log, TEXT("Stacked %d x %s (Total: %d)"),
					Target.Quantity, *ItemData.DisplayName.ToString(), Entry->Quantity);
			}
		}
	}

	// Phase 2: Create new stacks one at a time
	// Add entries immediately so FindFreeGridPos sees them for next iteration
	while (RemainingQuantity > 0)
	{
		TArray<FInventoryAddHelper::FNewStackPlacement> Placements;
		const int32 BeforeRemaining = RemainingQuantity;
		RemainingQuantity = FInventoryAddHelper::CalculateNewPlacements(
			ItemData, RemainingQuantity, MaxStack, ContainerStates, Callbacks, Placements);

		if (Placements.Num() == 0)
		{
			break; // No more placements possible
		}

		// Add first placement immediately so next iteration sees it
		const FInventoryAddHelper::FNewStackPlacement& Placement = Placements[0];
		const int32 SlotIndex = ComputeSlotIndex(Placement.ContainerId, Placement.GridPos);
		const uint32 InstanceId = Inventory.AddEntry(
			ObjectId, Placement.Quantity, Placement.ContainerId, Placement.GridPos, Placement.bRotated, SlotIndex);
		UE_LOG(LogProjectInventory, Log, TEXT("Placed %d x %s -> %s at (%d,%d) rot:%d (InstanceId: %d)"),
			Placement.Quantity, *ItemData.DisplayName.ToString(), *Placement.ContainerId.ToString(),
			Placement.GridPos.X, Placement.GridPos.Y, Placement.bRotated ? 1 : 0, InstanceId);

		// Only process one placement per iteration to avoid overlaps
		RemainingQuantity = BeforeRemaining - Placement.Quantity;
	}

	const int32 QuantityAdded = AllowedQuantity - RemainingQuantity;
	if (RemainingQuantity > 0)
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("Internal_AddItem: Could not add %d remaining items"), RemainingQuantity);
	}
	return QuantityAdded;
}

bool UProjectInventoryComponent::Internal_RemoveItem(uint32 InstanceId, int32 Quantity)
{
	FInventoryEntry* Entry = Inventory.FindEntry(InstanceId);
	if (!Entry)
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("Internal_RemoveItem: InstanceId %d not found"), InstanceId);
		return false;
	}

	Entry->Quantity -= Quantity;
	if (Entry->Quantity <= 0)
	{
		Inventory.RemoveEntry(InstanceId);
	}
	else
	{
		Inventory.MarkEntryDirty(*Entry);
	}

	return true;
}

bool UProjectInventoryComponent::Internal_MoveItem(uint32 InstanceId, FGameplayTag FromContainer, FIntPoint FromPos, FGameplayTag ToContainer, FIntPoint ToPos, int32 Quantity, bool bRotated)
{
	FInventoryEntry* Entry = Inventory.FindEntry(InstanceId);
	if (!Entry)
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("Internal_MoveItem: InstanceId %d not found"), InstanceId);
		return false;
	}

	if (Quantity <= 0 || Quantity > Entry->Quantity)
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("Internal_MoveItem: Invalid quantity %d for InstanceId %d"), Quantity, InstanceId);
		return false;
	}

	FGameplayTag CurrentContainerId;
	FIntPoint CurrentPos;
	bool bCurrentRotated = false;
	if (!GetEffectiveEntryPlacement(*Entry, CurrentContainerId, CurrentPos, bCurrentRotated))
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("Internal_MoveItem: Invalid current placement for InstanceId %d"), InstanceId);
		return false;
	}

	if (FromContainer.IsValid() && FromContainer != CurrentContainerId)
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("Internal_MoveItem: FromContainer mismatch for InstanceId %d"), InstanceId);
		return false;
	}

	if (FromPos.X >= 0 && FromPos.Y >= 0 && FromPos != CurrentPos)
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("Internal_MoveItem: FromPos mismatch for InstanceId %d"), InstanceId);
		return false;
	}

	FItemDataView ItemData;
	if (!GetItemDataView(Entry->ItemId, ItemData))
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("Internal_MoveItem: Missing item data"));
		return false;
	}

	FInventoryContainerConfig TargetContainer;
	if (!GetContainerConfig(ToContainer, TargetContainer))
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("Internal_MoveItem: Invalid target container"));
		return false;
	}

	if (!ContainerAllowsItem(TargetContainer, ItemData))
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("Internal_MoveItem: Item not allowed in target container"));
		return false;
	}

	const bool bTargetRotated = bRotated && TargetContainer.bAllowRotation;
	const FIntPoint ItemSize = GetItemGridSize(ItemData, bTargetRotated);
	const FIntPoint CurrentItemSize = GetItemGridSize(ItemData, bCurrentRotated);

	if (!IsRectWithinContainer(TargetContainer, ToPos, ItemSize))
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("Internal_MoveItem: Target position out of bounds"));
		return false;
	}

	// SOLID: Delegated to FInventoryMoveHelper
	if (Quantity < Entry->Quantity && CurrentContainerId == ToContainer)
	{
		if (FInventoryMoveHelper::CheckSelfOverlap(CurrentPos, CurrentItemSize, ToPos, ItemSize))
		{
			UE_LOG(LogProjectInventory, Warning, TEXT("Internal_MoveItem: Target overlaps source when splitting"));
			return false;
		}
	}

	// No-op move
	if (CurrentContainerId == ToContainer && CurrentPos == ToPos && bCurrentRotated == bTargetRotated && Quantity == Entry->Quantity)
	{
		return true;
	}

	TMap<FPrimaryAssetId, FItemDataView> ItemDataCache;
	ItemDataCache.Add(Entry->ItemId, ItemData);

	// Capacity check when moving across containers
	if (CurrentContainerId != ToContainer)
	{
		if (TargetContainer.MaxWeight > 0.f && ItemData.Weight > 0.f)
		{
			const float TargetWeight = GetContainerCurrentWeight(ToContainer, ItemDataCache);
			if (TargetWeight + (ItemData.Weight * Quantity) > TargetContainer.MaxWeight)
			{
				UE_LOG(LogProjectInventory, Warning, TEXT("Internal_MoveItem: Target container weight exceeded"));
				return false;
			}
		}

		if (TargetContainer.MaxVolume > 0.f && ItemData.Volume > 0.f)
		{
			const float TargetVolume = GetContainerCurrentVolume(ToContainer, ItemDataCache);
			if (TargetVolume + (ItemData.Volume * Quantity) > TargetContainer.MaxVolume)
			{
				UE_LOG(LogProjectInventory, Warning, TEXT("Internal_MoveItem: Target container volume exceeded"));
				return false;
			}
		}
	}

	// SOLID: Overlap detection delegated to FInventoryMoveHelper
	FInventoryMoveHelper::FMoveCallbacks MoveCallbacks;
	MoveCallbacks.GetEffectivePlacement = [this](const FInventoryEntry& E, FGameplayTag& C, FIntPoint& P, bool& R) {
		return GetEffectiveEntryPlacement(E, C, P, R);
	};
	MoveCallbacks.GetItemDataView = [this](FPrimaryAssetId Id, FItemDataView& D) {
		return GetItemDataView(Id, D);
	};
	MoveCallbacks.GetItemGridSize = [this](const FItemDataView& D, bool R) {
		return GetItemGridSize(D, R);
	};

	const FInventoryMoveHelper::FOverlapResult OverlapResult = FInventoryMoveHelper::FindOverlapAtTarget(
		Inventory.Entries, ToContainer, ToPos, ItemSize, InstanceId, MoveCallbacks);

	if (OverlapResult.bMultipleOverlaps)
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("Internal_MoveItem: Multiple overlaps at target"));
		return false;
	}

	FInventoryEntry* OverlapEntry = OverlapResult.bHasOverlap ? Inventory.FindEntry(OverlapResult.OverlapInstanceId) : nullptr;

	// SOLID: Stack validation delegated to FInventoryMoveHelper
	if (OverlapEntry)
	{
		const int32 MaxStack = FMath::Max(1, ItemData.MaxStack);
		if (!FInventoryMoveHelper::CanStackWith(*Entry, *OverlapEntry, MaxStack, Quantity))
		{
			UE_LOG(LogProjectInventory, Warning, TEXT("Internal_MoveItem: Cannot stack at target"));
			return false;
		}

		OverlapEntry->Quantity += Quantity;
		Inventory.MarkEntryDirty(*OverlapEntry);

		if (Quantity >= Entry->Quantity)
		{
			Inventory.RemoveEntry(InstanceId);
		}
		else
		{
			Entry->Quantity -= Quantity;
			Inventory.MarkEntryDirty(*Entry);
		}

		return true;
	}

	if (Quantity < Entry->Quantity)
	{
		// Split stack into new entry
		const int32 SlotIndex = ComputeSlotIndex(ToContainer, ToPos);
		const uint32 NewInstanceId = Inventory.AddEntry(Entry->ItemId, Quantity, ToContainer, ToPos, bTargetRotated, SlotIndex);
		FInventoryEntry* NewEntry = Inventory.FindEntry(NewInstanceId);
		if (NewEntry)
		{
			NewEntry->InstanceData = Entry->InstanceData;
			NewEntry->OverrideMagnitudes = Entry->OverrideMagnitudes;
			Inventory.MarkEntryDirty(*NewEntry);
		}

		Entry->Quantity -= Quantity;
		Inventory.MarkEntryDirty(*Entry);
		return true;
	}

	// Move entire entry
	Entry->ContainerId = ToContainer;
	Entry->GridPos = ToPos;
	Entry->bRotated = bTargetRotated;
	Entry->SlotIndex = ComputeSlotIndex(ToContainer, ToPos);
	Inventory.MarkEntryDirty(*Entry);
	return true;
}

bool UProjectInventoryComponent::Internal_UseItem(uint32 InstanceId)
{
	FInventoryEntry* Entry = Inventory.FindEntry(InstanceId);
	if (!Entry)
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("Internal_UseItem: InstanceId %d not found"), InstanceId);
		return false;
	}

	FItemDataView ItemData;
	if (!GetItemDataView(Entry->ItemId, ItemData))
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("Internal_UseItem: Missing item data"));
		return false;
	}

	if (!ItemData.IsConsumable())
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("Internal_UseItem: Item is not consumable"));
		return false;
	}

	UAbilitySystemComponent* ASC = GetOwnerASC();
	if (!ASC)
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("Internal_UseItem: No ASC found"));
		return false;
	}

	// Build magnitudes: start with definition defaults, then apply instance overrides
	TMap<FGameplayTag, float> MergedMagnitudes = ItemData.Magnitudes;
	for (const FMagnitudeOverride& Override : Entry->OverrideMagnitudes)
	{
		MergedMagnitudes.Add(Override.Tag, Override.Value); // Override wins
	}

	TArray<FAttributeMagnitude> Magnitudes;
	for (const auto& Pair : MergedMagnitudes)
	{
		Magnitudes.Add(FAttributeMagnitude{ Pair.Key, Pair.Value });
	}

	if (Magnitudes.Num() > 0)
	{
		EApplyMagnitudesResult Result = UProjectGASLibrary::ApplyMagnitudes(ASC, Magnitudes);
		if (Result == EApplyMagnitudesResult::Failed)
		{
			UE_LOG(LogProjectInventory, Warning, TEXT("Internal_UseItem: Failed to apply magnitudes"));
			return false;
		}
	}

	// Copy ItemId before potentially removing (Entry* becomes invalid after remove)
	const FPrimaryAssetId UsedItemId = Entry->ItemId;

	// Consume item if configured
	if (ItemData.bConsumeOnUse)
	{
		Internal_RemoveItem(InstanceId, 1);
	}

	// Broadcast event (after GAS effects applied)
	OnItemUsed.Broadcast(UsedItemId);

	UE_LOG(LogProjectInventory, Log, TEXT("Used item: %s"), *ItemData.DisplayName.ToString());
	return true;
}

bool UProjectInventoryComponent::Internal_EquipItem(uint32 InstanceId, FGameplayTag EquipSlot)
{
	FInventoryEntry* Entry = Inventory.FindEntry(InstanceId);
	if (!Entry)
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("Internal_EquipItem: InstanceId %d not found"), InstanceId);
		return false;
	}

	FItemDataView ItemData;
	if (!GetItemDataView(Entry->ItemId, ItemData) || !ItemData.IsEquipment())
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("Internal_EquipItem: Item is not equipment"));
		return false;
	}

	// Unequip current item in slot if any
	if (EquippedItems.Contains(EquipSlot))
	{
		if (!Internal_UnequipItem(EquipSlot))
		{
			UE_LOG(LogProjectInventory, Warning, TEXT("Internal_EquipItem: Failed to unequip current item in slot %s"), *EquipSlot.ToString());
			return false;
		}
	}

	if (!EquipSlot.IsValid())
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("Internal_EquipItem: Invalid equip slot"));
		return false;
	}

	if (ItemData.EquipSlotTag.IsValid() && ItemData.EquipSlotTag != EquipSlot)
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("Internal_EquipItem: Item slot mismatch (item=%s, request=%s)"),
			*ItemData.EquipSlotTag.ToString(), *EquipSlot.ToString());
		return false;
	}

	UAbilitySystemComponent* ASC = GetOwnerASC();
	if (!ASC && !ItemData.EquipAbilitySetPath.IsNull())
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("Internal_EquipItem: No ASC found"));
		return false;
	}

	UProjectAbilitySet* AbilitySet = nullptr;
	if (!ItemData.EquipAbilitySetPath.IsNull())
	{
		AbilitySet = Cast<UProjectAbilitySet>(ItemData.EquipAbilitySetPath.TryLoad());
		if (!AbilitySet)
		{
			UE_LOG(LogProjectInventory, Warning, TEXT("Internal_EquipItem: Failed to load AbilitySet %s"),
				*ItemData.EquipAbilitySetPath.ToString());
			return false;
		}
	}

	FEquippedItemData EquipData;
	EquipData.InstanceId = InstanceId;
	if (AbilitySet && ASC)
	{
		AbilitySet->GiveToAbilitySystem(ASC, &EquipData.GrantedHandles);
	}
	EquippedItems.Add(EquipSlot, EquipData);

	// Physical move: remove item from its container (it now lives in the equipment slot)
	Entry->ContainerId = FGameplayTag();
	Entry->GridPos = FIntPoint(-1, -1);
	Inventory.MarkEntryDirty(*Entry);

	UE_LOG(LogProjectInventory, Log, TEXT("Equipped item %s to slot %s (removed from container)"), *Entry->ItemId.ToString(), *EquipSlot.ToString());
	return true;
}

bool UProjectInventoryComponent::Internal_UnequipItem(FGameplayTag EquipSlot)
{
	FEquippedItemData* EquipData = EquippedItems.Find(EquipSlot);
	if (!EquipData)
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("Internal_UnequipItem: No item in slot %s"), *EquipSlot.ToString());
		return false;
	}

	FInventoryEntry* Entry = Inventory.FindEntry(EquipData->InstanceId);
	if (!Entry)
	{
		// Entry gone - still clean up GAS grants to avoid leaked abilities
		UE_LOG(LogProjectInventory, Warning, TEXT("Internal_UnequipItem: Entry %d not found - cleaning up GAS"), EquipData->InstanceId);
		if (UAbilitySystemComponent* ASC = GetOwnerASC())
		{
			UProjectAbilitySet::TakeFromAbilitySystem(ASC, &EquipData->GrantedHandles);
		}
		EquippedItems.Remove(EquipSlot);
		return false;
	}

	// Validate granted containers are empty (check first - more informative error than "hands full")
	bool bHasGrant = false;
	TArray<FGameplayTag> GrantedContainerIds;

	FItemDataView ItemData;
	if (GetItemDataView(Entry->ItemId, ItemData) && ItemData.ContainerGrants.Num() > 0)
	{
		for (const FInventoryContainerGrantView& Grant : ItemData.ContainerGrants)
		{
			if (Grant.ContainerId.IsValid())
			{
				GrantedContainerIds.Add(Grant.ContainerId);
				bHasGrant = true;
			}
		}
	}

	if (!bHasGrant)
	{
		TArray<FInventoryContainerConfig> SlotGrants;
		if (GetEquipSlotContainerGrants(EquipSlot, SlotGrants))
		{
			for (const FInventoryContainerConfig& GrantedContainer : SlotGrants)
			{
				if (GrantedContainer.ContainerId.IsValid())
				{
					GrantedContainerIds.Add(GrantedContainer.ContainerId);
				}
			}
		}
	}

	for (const FGameplayTag& ContainerId : GrantedContainerIds)
	{
		if (!IsContainerEmpty(ContainerId))
		{
			UE_LOG(LogProjectInventory, Warning, TEXT("Internal_UnequipItem: Container not empty for slot %s"), *EquipSlot.ToString());
			BroadcastError(NSLOCTEXT("Inventory", "UnequipBlocked", "Cannot unequip - pockets not empty"));
			return false;
		}
	}

	// Pre-check: find a free hand cell to return the item to.
	FGameplayTag TargetHand;
	FIntPoint TargetHandPos = FIntPoint(-1, -1);
	const FIntPoint ItemSize = GetItemGridSize(ItemData, Entry->bRotated);

	FInventoryContainerConfig LeftHandConfig;
	if (GetContainerConfig(ProjectTags::Item_Container_LeftHand, LeftHandConfig)
		&& FindFreeGridPos(LeftHandConfig, ItemSize, Entry->InstanceId, TargetHandPos))
	{
		TargetHand = ProjectTags::Item_Container_LeftHand;
	}
	else
	{
		FInventoryContainerConfig RightHandConfig;
		if (GetContainerConfig(ProjectTags::Item_Container_RightHand, RightHandConfig)
			&& FindFreeGridPos(RightHandConfig, ItemSize, Entry->InstanceId, TargetHandPos))
		{
			TargetHand = ProjectTags::Item_Container_RightHand;
		}
	}

	if (!TargetHand.IsValid())
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("Internal_UnequipItem: No free hand space, cannot unequip slot %s"), *EquipSlot.ToString());
		BroadcastError(NSLOCTEXT("Inventory", "UnequipHandsFull", "Cannot unequip - hands full"));
		return false;
	}

	// All validations passed - commit: GAS cleanup
	UAbilitySystemComponent* ASC = GetOwnerASC();
	if (ASC)
	{
		UProjectAbilitySet::TakeFromAbilitySystem(ASC, &EquipData->GrantedHandles);
	}

	// Commit: remove from equipped map and return to the resolved hand cell.
	EquippedItems.Remove(EquipSlot);
	Entry->ContainerId = TargetHand;
	Entry->GridPos = TargetHandPos;
	Inventory.MarkEntryDirty(*Entry);

	UE_LOG(LogProjectInventory, Log, TEXT("Unequipped item from slot %s -> %s"), *EquipSlot.ToString(), *TargetHand.ToString());
	return true;
}

UAbilitySystemComponent* UProjectInventoryComponent::GetOwnerASC() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}
	return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner);
}

void UProjectInventoryComponent::GetEffectiveContainers(TArray<FInventoryContainerConfig>& OutContainers) const
{
	OutContainers.Reset();

	if (bIncludeConfiguredContainersAsBase && Containers.Num() > 0)
	{
		OutContainers = Containers;
	}
	else
	{
		// Vision SOT: naked baseline is hands only.
		// Represent hands as two real 2x2 containers so small items can share a hand grid.
		const FGameplayTag EffectiveDefaultContainerId = GetDefaultContainerId();
		if (EffectiveDefaultContainerId == ProjectTags::Item_Container_LeftHand
			|| EffectiveDefaultContainerId == ProjectTags::Item_Container_RightHand
			|| EffectiveDefaultContainerId == ProjectTags::Item_Container_Hands)
		{
			auto AddHandContainer = [&OutContainers](const FGameplayTag& ContainerId)
			{
				FInventoryContainerConfig HandContainer;
				HandContainer.ContainerId = ContainerId;
				HandContainer.GridSize = FIntPoint(2, 2);
				HandContainer.MaxCells = 4;
				HandContainer.bWidthOnlyValidation = true;
				OutContainers.Add(HandContainer);
			};

			AddHandContainer(ProjectTags::Item_Container_LeftHand);
			AddHandContainer(ProjectTags::Item_Container_RightHand);
		}
		else
		{
			FInventoryContainerConfig DefaultContainer;
			DefaultContainer.ContainerId = EffectiveDefaultContainerId;
			DefaultContainer.MaxWeight = MaxWeight;
			DefaultContainer.MaxVolume = MaxVolume;
			DefaultContainer.MaxCells = MaxSlots;

			int32 Width = FMath::Max(1, DefaultContainerGridWidth);
			if (MaxSlots > 0)
			{
				Width = FMath::Min(Width, MaxSlots);
			}
			const int32 Height = (Width > 0) ? FMath::DivideAndRoundUp(MaxSlots, Width) : 0;
			DefaultContainer.GridSize = FIntPoint(Width, Height);
			OutContainers.Add(DefaultContainer);
		}
	}

	for (const auto& Pair : EquippedItems)
	{
		const uint32 InstanceId = Pair.Value.InstanceId;
		if (const FInventoryEntry* Entry = Inventory.FindEntry(InstanceId))
		{
			FItemDataView ItemData;
			if (GetItemDataView(Entry->ItemId, ItemData) && ItemData.ContainerGrants.Num() > 0)
			{
				// Item-driven grants are the primary source of container expansion.
				// This keeps pocket/backpack sizes attached to the item definition.
				for (const FInventoryContainerGrantView& Grant : ItemData.ContainerGrants)
				{
					if (!Grant.ContainerId.IsValid())
					{
						continue;
					}
					UpsertContainerConfig(BuildContainerConfigFromGrant(Grant), OutContainers);
				}
				continue;
			}
		}

		TArray<FInventoryContainerConfig> SlotGrants;
		if (GetEquipSlotContainerGrants(Pair.Key, SlotGrants))
		{
			for (const FInventoryContainerConfig& GrantConfig : SlotGrants)
			{
				if (GrantConfig.ContainerId.IsValid())
				{
					UpsertContainerConfig(GrantConfig, OutContainers);
				}
			}
		}
	}
}

bool UProjectInventoryComponent::GetContainerConfig(FGameplayTag ContainerId, FInventoryContainerConfig& OutConfig) const
{
	if (!ContainerId.IsValid())
	{
		return false;
	}

	TArray<FInventoryContainerConfig> EffectiveContainers;
	GetEffectiveContainers(EffectiveContainers);

	for (const FInventoryContainerConfig& Container : EffectiveContainers)
	{
		if (Container.ContainerId == ContainerId)
		{
			OutConfig = Container;
			return true;
		}
	}

	return false;
}

FGameplayTag UProjectInventoryComponent::GetDefaultContainerId() const
{
	if (!DefaultContainerId.IsValid() || DefaultContainerId == ProjectTags::Item_Container_Hands)
	{
		return ProjectTags::Item_Container_LeftHand;
	}

	return DefaultContainerId;
}

// SOLID: Delegated to FInventoryContainerHelper
int32 UProjectInventoryComponent::GetContainerIndex(FGameplayTag ContainerId, const TArray<FInventoryContainerConfig>& ContainersList) const
{
	return FInventoryContainerHelper::GetContainerIndex(ContainerId, ContainersList);
}

int32 UProjectInventoryComponent::GetContainerSlotOffset(FGameplayTag ContainerId, const TArray<FInventoryContainerConfig>& ContainersList) const
{
	return FInventoryContainerHelper::GetContainerSlotOffset(ContainerId, ContainersList);
}

int32 UProjectInventoryComponent::GetContainerCellCount(const FInventoryContainerConfig& Container) const
{
	return FInventoryContainerHelper::GetContainerCellCount(Container);
}

int32 UProjectInventoryComponent::ComputeSlotIndex(FGameplayTag ContainerId, FIntPoint GridPos) const
{
	TArray<FInventoryContainerConfig> EffectiveContainers;
	GetEffectiveContainers(EffectiveContainers);
	return FInventoryContainerHelper::ComputeSlotIndex(ContainerId, GridPos, EffectiveContainers);
}

bool UProjectInventoryComponent::TryGetGridPosFromSlot(FGameplayTag ContainerId, int32 SlotIndex, FIntPoint& OutGridPos) const
{
	TArray<FInventoryContainerConfig> EffectiveContainers;
	GetEffectiveContainers(EffectiveContainers);
	return FInventoryContainerHelper::TryGetGridPosFromSlot(ContainerId, SlotIndex, EffectiveContainers, OutGridPos);
}

// SOLID: Delegated to FInventoryGridPlacement
FIntPoint UProjectInventoryComponent::SanitizeGridSize(FIntPoint InSize) const
{
	return FInventoryGridPlacement::SanitizeGridSize(InSize);
}

FIntPoint UProjectInventoryComponent::GetItemGridSize(const FItemDataView& ItemData, bool bRotated) const
{
	return FInventoryGridPlacement::GetItemGridSize(ItemData.GridSize, bRotated);
}

FInventoryContainerConfig UProjectInventoryComponent::BuildContainerConfigFromGrant(const FInventoryContainerGrantView& Grant) const
{
	FInventoryContainerConfig Config;
	Config.ContainerId = Grant.ContainerId;
	Config.MaxWeight = Grant.MaxWeight;
	Config.MaxVolume = Grant.MaxVolume;
	Config.MaxCells = Grant.MaxCells;
	Config.AllowedTags = Grant.AllowedTags;
	Config.bAllowRotation = Grant.bAllowRotation;

	if (Grant.GridSize.X > 0 && Grant.GridSize.Y > 0)
	{
		Config.GridSize = SanitizeGridSize(Grant.GridSize);
	}
	else if (Grant.MaxCells > 0)
	{
		int32 Width = FMath::Max(1, DefaultContainerGridWidth);
		Width = FMath::Min(Width, Grant.MaxCells);
		const int32 Height = FMath::DivideAndRoundUp(Grant.MaxCells, Width);
		Config.GridSize = FIntPoint(Width, Height);
	}
	else
	{
		Config.GridSize = FIntPoint(1, 1);
	}

	return Config;
}

// SOLID: Delegated to FInventoryContainerHelper
void UProjectInventoryComponent::UpsertContainerConfig(const FInventoryContainerConfig& GrantConfig, TArray<FInventoryContainerConfig>& OutContainers) const
{
	FInventoryContainerHelper::UpsertContainerConfig(GrantConfig, OutContainers);
}

// SOLID: Delegated to FInventoryGridPlacement
bool UProjectInventoryComponent::IsRectWithinContainer(const FInventoryContainerConfig& Container, FIntPoint GridPos, FIntPoint ItemSize) const
{
	return FInventoryGridPlacement::IsRectWithinContainer(Container, GridPos, ItemSize);
}

bool UProjectInventoryComponent::DoesRectOverlap(FGameplayTag ContainerId, FIntPoint GridPos, FIntPoint ItemSize, int32 IgnoreInstanceId) const
{
	// Check container config for special handling.
	FInventoryContainerConfig ContainerConfig;
	const bool bHasConfig = GetContainerConfig(ContainerId, ContainerConfig);
	const bool bSlotBased = bHasConfig && ContainerConfig.bSlotBased;

	// SOLID: Use helper for size clamping (width-only containers)
	const FIntPoint EffectiveItemSize = bHasConfig
		? FInventoryGridPlacement::ClampSizeForContainer(ContainerConfig, ItemSize)
		: ItemSize;

	for (const FInventoryEntry& Entry : Inventory.Entries)
	{
		if (Entry.InstanceId == IgnoreInstanceId)
		{
			continue;
		}

		FGameplayTag EntryContainerId;
		FIntPoint EntryPos;
		bool bEntryRotated = false;
		if (!GetEffectiveEntryPlacement(Entry, EntryContainerId, EntryPos, bEntryRotated))
		{
			continue;
		}

		if (EntryContainerId != ContainerId)
		{
			continue;
		}

		// Slot-based: items overlap only if same slot index (GridPos.X).
		if (bSlotBased)
		{
			if (GridPos.X == EntryPos.X)
			{
				return true;
			}
			continue;
		}

		// Grid-based: AABB intersection test.
		FItemDataView EntryData;
		if (!GetItemDataView(Entry.ItemId, EntryData))
		{
			continue;
		}

		FIntPoint EntrySize = GetItemGridSize(EntryData, bEntryRotated);
		// SOLID: Clamp entry size for width-only containers
		if (bHasConfig)
		{
			EntrySize = FInventoryGridPlacement::ClampSizeForContainer(ContainerConfig, EntrySize);
		}

		// SOLID: Use helper for AABB test
		if (FInventoryGridPlacement::DoRectsOverlap(GridPos, EffectiveItemSize, EntryPos, EntrySize))
		{
			return true;
		}
	}

	return false;
}

// SOLID: Uses FInventoryGridPlacement with occupancy callback
bool UProjectInventoryComponent::FindFreeGridPos(const FInventoryContainerConfig& Container, FIntPoint ItemSize, int32 IgnoreInstanceId, FIntPoint& OutPos) const
{
	// Slot-based: log warning if no slot count defined
	if (Container.bSlotBased && Container.MaxCells <= 0 && Container.GridSize.X <= 0)
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("FindFreeGridPos: Slot-based container '%s' has no MaxCells or GridSize.X"),
			*Container.ContainerId.ToString());
		return false;
	}

	// Use helper with occupancy callback that checks our inventory state
	return FInventoryGridPlacement::FindFreeGridPos(
		Container,
		ItemSize,
		[this, &Container, IgnoreInstanceId](FIntPoint TestPos, FIntPoint TestSize) {
			return DoesRectOverlap(Container.ContainerId, TestPos, TestSize, IgnoreInstanceId);
		},
		OutPos);
}

bool UProjectInventoryComponent::GetEffectiveEntryPlacement(const FInventoryEntry& Entry, FGameplayTag& OutContainerId, FIntPoint& OutGridPos, bool& bOutRotated) const
{
	// Equipped items have no grid placement (ContainerId cleared, GridPos = -1,-1)
	if (!Entry.ContainerId.IsValid() && Entry.GridPos == FIntPoint(-1, -1))
	{
		return false;
	}

	OutContainerId = Entry.ContainerId.IsValid() ? Entry.ContainerId : GetDefaultContainerId();
	OutGridPos = Entry.GridPos;
	bOutRotated = Entry.bRotated;

	if (OutGridPos.X >= 0 && OutGridPos.Y >= 0)
	{
		return true;
	}

	if (Entry.SlotIndex >= 0)
	{
		return TryGetGridPosFromSlot(OutContainerId, Entry.SlotIndex, OutGridPos);
	}

	return false;
}

// SOLID: Delegated to FInventoryWeightHelper
float UProjectInventoryComponent::GetContainerCurrentWeight(FGameplayTag ContainerId, TMap<FPrimaryAssetId, FItemDataView>& ItemDataCache) const
{
	FInventoryWeightHelper::FWeightCallbacks Callbacks;
	Callbacks.GetEffectivePlacement = [this](const FInventoryEntry& E, FGameplayTag& C, FIntPoint& P, bool& R) {
		return GetEffectiveEntryPlacement(E, C, P, R);
	};
	Callbacks.GetItemDataView = [this](FPrimaryAssetId Id, FItemDataView& D) {
		return GetItemDataView(Id, D);
	};
	return FInventoryWeightHelper::CalculateContainerWeight(Inventory.Entries, ContainerId, ItemDataCache, Callbacks);
}

// SOLID: Delegated to FInventoryWeightHelper
float UProjectInventoryComponent::GetContainerCurrentVolume(FGameplayTag ContainerId, TMap<FPrimaryAssetId, FItemDataView>& ItemDataCache) const
{
	FInventoryWeightHelper::FWeightCallbacks Callbacks;
	Callbacks.GetEffectivePlacement = [this](const FInventoryEntry& E, FGameplayTag& C, FIntPoint& P, bool& R) {
		return GetEffectiveEntryPlacement(E, C, P, R);
	};
	Callbacks.GetItemDataView = [this](FPrimaryAssetId Id, FItemDataView& D) {
		return GetItemDataView(Id, D);
	};
	return FInventoryWeightHelper::CalculateContainerVolume(Inventory.Entries, ContainerId, ItemDataCache, Callbacks);
}

bool UProjectInventoryComponent::ContainerAllowsItem(const FInventoryContainerConfig& Container, const FItemDataView& ItemData) const
{
	if (Container.AllowedTags.Num() == 0)
	{
		return true;
	}

	return ItemData.Tags.HasAny(Container.AllowedTags);
}

bool UProjectInventoryComponent::GetContainerOrder(TArray<FInventoryContainerConfig>& OutContainers) const
{
	GetEffectiveContainers(OutContainers);
	if (OutContainers.Num() == 0)
	{
		return false;
	}

	// Sort by placement priority: Backpack > Jacket pockets > Pants pockets > Hands
	// Lower value = higher priority (tried first for auto-placement)
	auto GetContainerPriority = [](const FGameplayTag& ContainerId) -> int32
	{
		if (ContainerId == ProjectTags::Item_Container_Backpack) return 0;
		if (ContainerId == ProjectTags::Item_Container_Pockets3) return 1;
		if (ContainerId == ProjectTags::Item_Container_Pockets4) return 2;
		if (ContainerId == ProjectTags::Item_Container_Pockets1) return 3;
		if (ContainerId == ProjectTags::Item_Container_Pockets2) return 4;
		if (ContainerId == ProjectTags::Item_Container_LeftHand) return 10;
		if (ContainerId == ProjectTags::Item_Container_RightHand) return 11;
		if (ContainerId == ProjectTags::Item_Container_Hands) return 12;
		return 5; // Unknown containers between pockets and hands
	};

	OutContainers.Sort([&GetContainerPriority](const FInventoryContainerConfig& A, const FInventoryContainerConfig& B)
	{
		return GetContainerPriority(A.ContainerId) < GetContainerPriority(B.ContainerId);
	});

	UE_LOG(LogProjectInventory, Verbose, TEXT("GetContainerOrder: %d containers sorted:"), OutContainers.Num());
	for (int32 i = 0; i < OutContainers.Num(); ++i)
	{
		UE_LOG(LogProjectInventory, Verbose, TEXT("  [%d] %s (%dx%d)"),
			i, *OutContainers[i].ContainerId.ToString(), OutContainers[i].GridSize.X, OutContainers[i].GridSize.Y);
	}

	return true;
}

bool UProjectInventoryComponent::GetEquipSlotContainerGrants(FGameplayTag EquipSlot, TArray<FInventoryContainerConfig>& OutConfigs) const
{
	OutConfigs.Reset();

	for (const FEquipSlotContainerGrant& Grant : EquipSlotContainerGrants)
	{
		if (Grant.EquipSlot == EquipSlot)
		{
			OutConfigs.Add(Grant.Container);
		}
	}

	return OutConfigs.Num() > 0;
}

bool UProjectInventoryComponent::IsContainerEmpty(FGameplayTag ContainerId) const
{
	if (!ContainerId.IsValid())
	{
		return true;
	}

	for (const FInventoryEntry& Entry : Inventory.Entries)
	{
		FGameplayTag EntryContainerId;
		FIntPoint EntryPos;
		bool bEntryRotated = false;
		if (!GetEffectiveEntryPlacement(Entry, EntryContainerId, EntryPos, bEntryRotated))
		{
			continue;
		}

		if (EntryContainerId == ContainerId)
		{
			return false;
		}
	}

	return true;
}
