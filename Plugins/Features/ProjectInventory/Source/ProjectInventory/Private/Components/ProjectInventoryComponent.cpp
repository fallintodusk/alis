// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Components/ProjectInventoryComponent.h"
#include "Components/ProjectInventoryComponentInternals.h"
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
#include "Types/InventoryStackRules.h"
#include "ProjectInventory.h"
#include "Subsystems/ProjectContainerSessionSubsystem.h"
#include "Subsystems/ProjectObjectDefinitionCacheSubsystem.h"
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
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Modules/ModuleManager.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

using namespace ProjectInventoryInternal;

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
		auto AddGrant = [this](
			FGameplayTag SlotTag,
			FGameplayTag ContainerTag,
			FIntPoint GridSize,
			bool bWidthOnly = false,
			int32 InMaxCells = 0,
			int32 InCellDepthUnits = 1)
		{
			FEquipSlotContainerGrant Grant;
			Grant.EquipSlot = SlotTag;
			Grant.Container.ContainerId = ContainerTag;
			Grant.Container.GridSize = GridSize;
			Grant.Container.bWidthOnlyValidation = bWidthOnly;
			Grant.Container.MaxCells = InMaxCells;
			Grant.Container.CellDepthUnits = FMath::Max(1, InCellDepthUnits);
			EquipSlotContainerGrants.Add(Grant);
		};

		// Equipped items occupying hand equipment slots collapse hand storage back to a single held item.
		AddGrant(ProjectTags::Item_EquipmentSlot_MainHand, ProjectTags::Item_Container_LeftHand, FIntPoint(2, 2), true, 1, 4);
		AddGrant(ProjectTags::Item_EquipmentSlot_OffHand, ProjectTags::Item_Container_RightHand, FIntPoint(2, 2), true, 1, 4);

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

void UProjectInventoryComponent::OnRegister()
{
	Super::OnRegister();
	BindObjectDefinitionCache();
}

void UProjectInventoryComponent::BeginPlay()
{
	Super::BeginPlay();
	BindObjectDefinitionCache();

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

namespace
{
// Parse "ItemId:N" into TargetId and Quantity. Colon-separated, quantity defaults to 1.
void ParseActionArgs(const FString& Raw, FString& OutTargetId, int32& OutQuantity)
{
	OutQuantity = 1;
	OutTargetId = Raw;
	OutTargetId.TrimStartAndEndInline();

	int32 ColonIdx = INDEX_NONE;
	if (OutTargetId.FindLastChar(TEXT(':'), ColonIdx) && ColonIdx > 0)
	{
		const int32 Parsed = FCString::Atoi(*OutTargetId.Mid(ColonIdx + 1));
		if (Parsed > 0)
		{
			OutQuantity = Parsed;
			OutTargetId.LeftInline(ColonIdx);
			OutTargetId.TrimStartAndEndInline();
		}
	}
}
}

void UProjectInventoryComponent::HandleAction(const FString& Context, const FString& Action)
{
	// -------------------------------------------------------------------------
	// inventory.give:<ObjectId>[:Quantity] — add item(s) to inventory.
	// Examples: "inventory.give:KeyPlayerApartment", "inventory.give:Bandage:3"
	// -------------------------------------------------------------------------
	if (Action.StartsWith(TEXT("inventory.give:")))
	{
		FString TargetId;
		int32 Quantity = 1;
		ParseActionArgs(Action.Mid(15), TargetId, Quantity);

		const FPrimaryAssetId ItemId(FPrimaryAssetType(TEXT("ObjectDefinition")), FName(*TargetId));

		UE_LOG(LogProjectInventory, Log,
			TEXT("[HandleAction] inventory.give: Adding %dx '%s' (Context='%s')"),
			Quantity, *TargetId, *Context);

		RequestAddItem(ItemId, Quantity);
		return;
	}

	// -------------------------------------------------------------------------
	// inventory.consume:<ObjectId>[:Quantity] — remove item(s) from inventory.
	// Supports wildcard prefix and optional quantity.
	// Examples: "inventory.consume:WaterBottle*", "inventory.consume:Cigarette*:3"
	// -------------------------------------------------------------------------
	if (!Action.StartsWith(TEXT("inventory.consume:")))
	{
		return;
	}

	FString RawArgs = Action.Mid(18);
	FString ItemIdStr;
	int32 Quantity = 1;
	ParseActionArgs(RawArgs, ItemIdStr, Quantity);

	const bool bPrefixMatch = ItemIdStr.RemoveFromEnd(TEXT("*"));
	ItemIdStr.TrimStartAndEndInline();

	const FPrimaryAssetId TargetId(FPrimaryAssetType(TEXT("ObjectDefinition")), FName(*ItemIdStr));

	FInventoryEntry FoundEntry;
	if (!FindEntryByItemId(TargetId, FoundEntry))
	{
		// Prefix match fallback for item families (wildcard *).
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

	if (FoundEntry.Quantity < Quantity)
	{
		UE_LOG(LogProjectInventory, Warning,
			TEXT("[HandleAction] inventory.consume: Not enough '%s' (have %d, need %d)"),
			*ItemIdStr, FoundEntry.Quantity, Quantity);
		return;
	}

	UE_LOG(LogProjectInventory, Log,
		TEXT("[HandleAction] inventory.consume: Removing %dx '%s' (InstanceId=%d, Context='%s')"),
		Quantity, *ItemIdStr, FoundEntry.InstanceId, *Context);

	RequestRemoveItem(FoundEntry.InstanceId, Quantity);
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
		BroadcastError(NSLOCTEXT("Inventory", "SplitItemMissing", "Cannot split item"));
		return;
	}

	if (SplitQuantity <= 0)
	{
		UE_LOG(LogProjectInventory, Verbose, TEXT("RequestSplitStack: Invalid split quantity %d (entry has %d)"), SplitQuantity, Entry->Quantity);
		BroadcastError(NSLOCTEXT("Inventory", "SplitQuantityInvalid", "Choose a split quantity"));
		return;
	}

	if (SplitQuantity >= Entry->Quantity)
	{
		UE_LOG(LogProjectInventory, Verbose, TEXT("RequestSplitStack: Split quantity %d consumes full stack of %d"), SplitQuantity, Entry->Quantity);
		BroadcastError(NSLOCTEXT("Inventory", "SplitQuantityConsumesFullStack", "Split amount must be less than stack size"));
		return;
	}

	FInventoryContainerConfig ContainerConfig;
	if (!GetContainerConfig(Entry->ContainerId, ContainerConfig))
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("RequestSplitStack: Container %s not found"), *Entry->ContainerId.ToString());
		BroadcastError(NSLOCTEXT("Inventory", "SplitContainerMissing", "Container is not available"));
		return;
	}

	FItemDataView ItemData;
	const EInventoryItemDataResolveState ResolveState = ResolveItemDataView(Entry->ItemId, ItemData);
	if (ResolveState != EInventoryItemDataResolveState::Loaded)
	{
		UE_LOG(
			LogProjectInventory,
			Warning,
			TEXT("RequestSplitStack: ItemData unavailable for %s (%s)"),
			*Entry->ItemId.ToString(),
			LexToString(ResolveState));
		BroadcastError(NSLOCTEXT("Inventory", "SplitItemDataUnavailable", "Item data is not ready"));
		return;
	}

	// Use FindFreeGridPos - automatically handles slot-based, width-only, and MaxCells.
	// Must NOT ignore the source entry: splits keep the source stack in place and
	// spawn a new entry in a separate cell. Ignoring the source would let the
	// search treat the source cell as free, return it as the split target, and
	// the subsequent self-overlap check in Internal_MoveItem would reject with
	// SplitSourceOverlap ("Split needs a free cell").
	const FIntPoint ItemSize = GetItemGridSize(ItemData, Entry->bRotated);
	FIntPoint EmptyPos(-1, -1);
	bool bRotated = Entry->bRotated;

	if (!FindFreeGridPos(ContainerConfig, ItemSize, INDEX_NONE, EmptyPos))
	{
		// Try rotated if allowed
		if (ContainerConfig.bAllowRotation)
		{
			const FIntPoint RotatedSize = GetItemGridSize(ItemData, !Entry->bRotated);
			if (RotatedSize != ItemSize && FindFreeGridPos(ContainerConfig, RotatedSize, INDEX_NONE, EmptyPos))
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

// Client_WorldContainerSessionOpened/Closed_Implementation defined in
// ProjectInventoryComponent_WorldContainer.cpp.

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
	Callbacks.GetContainerConfig = [this](FGameplayTag C, FInventoryContainerConfig& OutConfig) {
		return GetContainerConfig(C, OutConfig);
	};
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
	return ResolveItemDataView(ObjectId, OutData) == EInventoryItemDataResolveState::Loaded;
}

EInventoryItemDataResolveState UProjectInventoryComponent::ResolveItemDataView(FPrimaryAssetId ObjectId, FItemDataView& OutData) const
{
	OutData = FItemDataView();

	if (!ObjectId.IsValid())
	{
		return EInventoryItemDataResolveState::Invalid;
	}

	const_cast<UProjectInventoryComponent*>(this)->BindObjectDefinitionCache();

	if (!ObjectDefinitionCache)
	{
		LogItemDataResolveState(ObjectId, EInventoryItemDataResolveState::Missing);
		return EInventoryItemDataResolveState::Missing;
	}

	if (UObject* LoadedObject = ObjectDefinitionCache->GetLoaded(ObjectId))
	{
		if (!LoadedObject->Implements<UItemDataProvider>())
		{
			LogItemDataResolveState(ObjectId, EInventoryItemDataResolveState::InvalidProvider);
			return EInventoryItemDataResolveState::InvalidProvider;
		}

		OutData = IItemDataProvider::Execute_GetItemDataView(LoadedObject);
		if (!OutData.IsValid())
		{
			LogItemDataResolveState(ObjectId, EInventoryItemDataResolveState::InvalidData);
			return EInventoryItemDataResolveState::InvalidData;
		}

		LoggedItemResolveStates.Remove(ObjectId);
		return EInventoryItemDataResolveState::Loaded;
	}

	const EObjectDefinitionLoadState LoadState = ObjectDefinitionCache->GetLoadState(ObjectId);
	if (LoadState == EObjectDefinitionLoadState::Loading)
	{
		LogItemDataResolveState(ObjectId, EInventoryItemDataResolveState::Loading);
		return EInventoryItemDataResolveState::Loading;
	}

	LogItemDataResolveState(ObjectId, EInventoryItemDataResolveState::Missing);
	return EInventoryItemDataResolveState::Missing;
}

void UProjectInventoryComponent::BindObjectDefinitionCache()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance)
	{
		return;
	}

	UProjectObjectDefinitionCacheSubsystem* CacheSubsystem =
		GameInstance->GetSubsystem<UProjectObjectDefinitionCacheSubsystem>();
	if (!CacheSubsystem)
	{
		return;
	}

	if (UObjectDefinitionCache* SharedCache = CacheSubsystem->GetCache())
	{
		ObjectDefinitionCache = SharedCache;
	}
}

void UProjectInventoryComponent::LogItemDataResolveState(
	FPrimaryAssetId ObjectId,
	EInventoryItemDataResolveState ResolveState) const
{
	if (!ObjectId.IsValid() || ResolveState == EInventoryItemDataResolveState::Loaded)
	{
		return;
	}

	if (const EInventoryItemDataResolveState* ExistingState = LoggedItemResolveStates.Find(ObjectId))
	{
		if (*ExistingState == ResolveState)
		{
			return;
		}
	}

	if (ResolveState == EInventoryItemDataResolveState::Loading)
	{
		UE_LOG(
			LogProjectInventory,
			Verbose,
			TEXT("GetItemDataView: %s -> %s"),
			*ObjectId.ToString(),
			LexToString(ResolveState));
	}
	else
	{
		UE_LOG(
			LogProjectInventory,
			Warning,
			TEXT("GetItemDataView: %s -> %s"),
			*ObjectId.ToString(),
			LexToString(ResolveState));
	}
	LoggedItemResolveStates.Add(ObjectId, ResolveState);
}

void UProjectInventoryComponent::LogMoveReject(
	const TCHAR* Context,
	EInventoryMoveRejectReason RejectReason,
	int32 InstanceId) const
{
	if (RejectReason == EInventoryMoveRejectReason::None)
	{
		return;
	}

	const bool bExpectedUserReject =
		RejectReason == EInventoryMoveRejectReason::ItemDataLoading
		|| RejectReason == EInventoryMoveRejectReason::ItemRejectedByContainer
		|| RejectReason == EInventoryMoveRejectReason::QuantityExceedsTargetStack
		|| RejectReason == EInventoryMoveRejectReason::OutOfBounds
		|| RejectReason == EInventoryMoveRejectReason::SplitSourceOverlap
		|| RejectReason == EInventoryMoveRejectReason::MultipleTargetOverlaps
		|| RejectReason == EInventoryMoveRejectReason::StackRejected
		|| RejectReason == EInventoryMoveRejectReason::TargetWeightExceeded
		|| RejectReason == EInventoryMoveRejectReason::TargetVolumeExceeded;

	if (bExpectedUserReject)
	{
		UE_LOG(
			LogProjectInventory,
			Verbose,
			TEXT("%s: Reject %s (InstanceId=%d)"),
			Context,
			LexToString(RejectReason),
			InstanceId);
	}
	else
	{
		UE_LOG(
			LogProjectInventory,
			Warning,
			TEXT("%s: Reject %s (InstanceId=%d)"),
			Context,
			LexToString(RejectReason),
			InstanceId);
	}
}

void UProjectInventoryComponent::RejectMove(
	const TCHAR* Context,
	EInventoryMoveRejectReason RejectReason,
	int32 InstanceId)
{
	LogMoveReject(Context, RejectReason, InstanceId);
	const FText ErrorMessage = MakeMoveRejectText(RejectReason);
	if (!ErrorMessage.IsEmpty())
	{
		BroadcastError(ErrorMessage);
	}
}

const TCHAR* UProjectInventoryComponent::LexToString(EInventoryItemDataResolveState ResolveState)
{
	switch (ResolveState)
	{
	case EInventoryItemDataResolveState::Invalid:
		return TEXT("InvalidId");
	case EInventoryItemDataResolveState::Missing:
		return TEXT("Missing");
	case EInventoryItemDataResolveState::Loading:
		return TEXT("Loading");
	case EInventoryItemDataResolveState::Loaded:
		return TEXT("Loaded");
	case EInventoryItemDataResolveState::InvalidProvider:
		return TEXT("InvalidProvider");
	case EInventoryItemDataResolveState::InvalidData:
		return TEXT("InvalidData");
	default:
		return TEXT("Unknown");
	}
}

const TCHAR* UProjectInventoryComponent::LexToString(EInventoryMoveRejectReason RejectReason)
{
	switch (RejectReason)
	{
	case EInventoryMoveRejectReason::None:
		return TEXT("None");
	case EInventoryMoveRejectReason::InvalidRequest:
		return TEXT("InvalidRequest");
	case EInventoryMoveRejectReason::ItemDataMissing:
		return TEXT("ItemDataMissing");
	case EInventoryMoveRejectReason::ItemDataLoading:
		return TEXT("ItemDataLoading");
	case EInventoryMoveRejectReason::TargetContainerMissing:
		return TEXT("TargetContainerMissing");
	case EInventoryMoveRejectReason::ItemRejectedByContainer:
		return TEXT("ItemRejectedByContainer");
	case EInventoryMoveRejectReason::QuantityExceedsTargetStack:
		return TEXT("QuantityExceedsTargetStack");
	case EInventoryMoveRejectReason::OutOfBounds:
		return TEXT("OutOfBounds");
	case EInventoryMoveRejectReason::SplitSourceOverlap:
		return TEXT("SplitSourceOverlap");
	case EInventoryMoveRejectReason::MultipleTargetOverlaps:
		return TEXT("MultipleTargetOverlaps");
	case EInventoryMoveRejectReason::StackRejected:
		return TEXT("StackRejected");
	case EInventoryMoveRejectReason::TargetWeightExceeded:
		return TEXT("TargetWeightExceeded");
	case EInventoryMoveRejectReason::TargetVolumeExceeded:
		return TEXT("TargetVolumeExceeded");
	default:
		return TEXT("Unknown");
	}
}

FText UProjectInventoryComponent::MakeMoveRejectText(EInventoryMoveRejectReason RejectReason)
{
	switch (RejectReason)
	{
	case EInventoryMoveRejectReason::InvalidRequest:
		return NSLOCTEXT("Inventory", "MoveRejectedInvalidRequest", "Inventory action is no longer valid");
	case EInventoryMoveRejectReason::ItemDataMissing:
	case EInventoryMoveRejectReason::ItemDataLoading:
		return NSLOCTEXT("Inventory", "MoveRejectedItemDataUnavailable", "Item data is not ready");
	case EInventoryMoveRejectReason::TargetContainerMissing:
		return NSLOCTEXT("Inventory", "MoveRejectedContainerMissing", "Container is not available");
	case EInventoryMoveRejectReason::ItemRejectedByContainer:
		return NSLOCTEXT("Inventory", "MoveRejectedByContainer", "Item cannot go in that container");
	case EInventoryMoveRejectReason::QuantityExceedsTargetStack:
		return NSLOCTEXT("Inventory", "MoveRejectedStackTooLarge", "Stack is too large for that cell");
	case EInventoryMoveRejectReason::OutOfBounds:
		return NSLOCTEXT("Inventory", "MoveRejectedOutOfBounds", "Item does not fit there");
	case EInventoryMoveRejectReason::SplitSourceOverlap:
		return NSLOCTEXT("Inventory", "MoveRejectedSplitSourceOverlap", "Split needs a free cell");
	case EInventoryMoveRejectReason::MultipleTargetOverlaps:
	case EInventoryMoveRejectReason::StackRejected:
		return NSLOCTEXT("Inventory", "MoveRejectedCellOccupied", "That cell is occupied");
	case EInventoryMoveRejectReason::TargetWeightExceeded:
		return NSLOCTEXT("Inventory", "MoveRejectedTargetWeightExceeded", "Container is too heavy");
	case EInventoryMoveRejectReason::TargetVolumeExceeded:
		return NSLOCTEXT("Inventory", "MoveRejectedTargetVolumeExceeded", "Container is too full");
	case EInventoryMoveRejectReason::None:
	default:
		return FText::GetEmpty();
	}
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
	Callbacks.GetEffectiveMaxStack = [this](const FInventoryContainerConfig& C, const FItemDataView& D) {
		return GetEffectiveMaxStackForContainer(C, D);
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

		const int32 Added = Internal_AddItem(LootEntry.ObjectId, LootEntry.Quantity).AddedQuantity;
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


// SOLID: Uses FInventoryAddHelper for stacking/placement computation
//
// Returns detailed outcome (FInventoryAddOutcome) so callers can distinguish:
//   - authoritative add success (AddedQuantity > 0)
//   - deferred async load  (bDeferred, retry on callback)
//   - terminal failure     (Fail != None, show toast; do not retry)
//
// Consumed by: FInventoryInteractionHandler (via TryAddItemDetailed), which
// owns the pending-pickup map and defers Consume() until an authoritative add
// lands. Legacy int32 callers (Server_AddItem, AddItemsBatch, TryAddItem) only
// read outcome.AddedQuantity.
bool UProjectInventoryComponent::Internal_UseItem(uint32 InstanceId)
{
	FInventoryEntry* Entry = Inventory.FindEntry(InstanceId);
	if (!Entry)
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("Internal_UseItem: InstanceId %d not found"), InstanceId);
		return false;
	}

	FItemDataView ItemData;
	const EInventoryItemDataResolveState ResolveState = ResolveItemDataView(Entry->ItemId, ItemData);
	if (ResolveState != EInventoryItemDataResolveState::Loaded)
	{
		UE_LOG(LogProjectInventory, Warning, TEXT("Internal_UseItem: Item data unavailable (%s)"), LexToString(ResolveState));
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
	const EInventoryItemDataResolveState ResolveState = ResolveItemDataView(Entry->ItemId, ItemData);
	if (ResolveState != EInventoryItemDataResolveState::Loaded || !ItemData.IsEquipment())
	{
		UE_LOG(
			LogProjectInventory,
			Warning,
			TEXT("Internal_EquipItem: Item is not equipment (state=%s, ItemId=%s)"),
			LexToString(ResolveState),
			*Entry->ItemId.ToString());
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

	// Hand grants are destinations for the unequipped item, not storage
	// extensions that lose their home when the source is removed. Their
	// occupancy is handled below by the free-hand-cell search.
	for (const FGameplayTag& ContainerId : GrantedContainerIds)
	{
		if (IsHandDestinationContainer(ContainerId))
		{
			continue;
		}
		if (!IsContainerEmpty(ContainerId))
		{
			UE_LOG(LogProjectInventory, Warning,
				TEXT("Internal_UnequipItem: granted storage %s not empty, blocking unequip of slot %s"),
				*ContainerId.ToString(), *EquipSlot.ToString());
			BroadcastError(NSLOCTEXT("Inventory", "UnequipBlockedByStorage",
				"Cannot unequip - empty the equipped item's storage first"));
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
		BroadcastError(NSLOCTEXT("Inventory", "UnequipNoSpace", "Cannot unequip - no space to stow the item"));
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

