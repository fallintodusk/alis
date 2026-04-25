// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MVVM/InventoryCellVisualState.h"
#include "MVVM/ProjectViewModel.h"
#include "Interaction/IInventoryDropCommandTarget.h"
#include "Interaction/IInventorySurfacePolicyProvider.h"
#include "Interfaces/IInventoryCommands.h"
#include "Interfaces/IInventoryReadOnly.h"
#include "Interfaces/IWorldContainerSessionSource.h"
#include "Types/ContainerSessionTypes.h"
#include "Presentation/ProjectUIActionDescriptor.h"
#include "InventoryViewModel.generated.h"

class AActor;
struct FProjectUIGridDragPayload;

/**
 * ViewModel for inventory UI (read-only scaffold).
 *
 * Exposes:
 * - Capacity stats (weight, volume)
 * - Item count
 * - Simple slot grid text (slot index -> label)
 * - Command intents (use, drop, equip)
 * - Panel visibility toggle
 *
 * SOLID: Complex building logic extracted to helpers:
 * - FInventoryViewModelCellBuilder (cell visual arrays)
 * - FInventoryViewModelEquipSlotBuilder (equip slot labels)
 */
UCLASS(BlueprintType)
class PROJECTINVENTORYUI_API UInventoryViewModel : public UProjectViewModel, public IInventorySurfacePolicyProvider, public IInventoryDropCommandTarget
{
    GENERATED_BODY()

public:
    // Capacity and counts
    VIEWMODEL_PROPERTY(int32, ItemCount)
    VIEWMODEL_PROPERTY(float, CurrentWeight)
    VIEWMODEL_PROPERTY(float, MaxWeight)
    VIEWMODEL_PROPERTY(float, CurrentVolume)
    VIEWMODEL_PROPERTY(float, MaxVolume)

    /** Get weight ratio (0.0 - 1.0+). Returns 0 if MaxWeight is 0. */
    float GetWeightRatio() const
    {
        return (MaxWeight > 0.f) ? (CurrentWeight / MaxWeight) : 0.f;
    }

    /** Check if overweight (ratio > 1.0). */
    bool IsOverweight() const
    {
        return GetWeightRatio() > 1.f;
    }

    // Selected container capacity (for capacity bar display)
    VIEWMODEL_PROPERTY(float, ContainerCurrentWeight)
    VIEWMODEL_PROPERTY(float, ContainerMaxWeight)
    VIEWMODEL_PROPERTY(float, ContainerCurrentVolume)
    VIEWMODEL_PROPERTY(float, ContainerMaxVolume)
    VIEWMODEL_PROPERTY(int32, ContainerCellDepthUnits)

    /** Get selected container weight ratio (0.0 - 1.0+). */
    float GetContainerWeightRatio() const
    {
        return (ContainerMaxWeight > 0.f) ? (ContainerCurrentWeight / ContainerMaxWeight) : 0.f;
    }

    /** Get selected container volume ratio (0.0 - 1.0+). */
    float GetContainerVolumeRatio() const
    {
        return (ContainerMaxVolume > 0.f) ? (ContainerCurrentVolume / ContainerMaxVolume) : 0.f;
    }

    // Grid dimensions and labels (selected container)
    VIEWMODEL_PROPERTY(int32, GridWidth)
    VIEWMODEL_PROPERTY(int32, GridHeight)

    /** Set grid dimensions for testing/synthetic viewmodels. */
    void SetGridDimensions(int32 InWidth, int32 InHeight)
    {
        UpdateGridWidth(InWidth);
        UpdateGridHeight(InHeight);
    }

    // Container tabs
protected:
    UPROPERTY(BlueprintReadOnly, Category = "ViewModel")
    TArray<FText> ContainerLabels;

    void UpdateContainerLabels(const TArray<FText>& InValue)
    {
        ContainerLabels = InValue;
        NotifyPropertyChanged(FName(TEXT("ContainerLabels")));
    }

public:
    FORCEINLINE const TArray<FText>& GetContainerLabels() const { return ContainerLabels; }
    void SetContainerLabels(const TArray<FText>& InValue)
    {
        UpdateContainerLabels(InValue);
    }
    void ClearContainerLabels()
    {
        ContainerLabels.Empty();
        NotifyPropertyChanged(FName(TEXT("ContainerLabels")));
    }

    VIEWMODEL_PROPERTY(int32, SelectedContainerIndex)
    VIEWMODEL_PROPERTY(int32, SecondaryContainerIndex)

    // Secondary container grid
    VIEWMODEL_PROPERTY(int32, SecondaryGridWidth)
    VIEWMODEL_PROPERTY(int32, SecondaryGridHeight)

    VIEWMODEL_PROPERTY(bool, bHasNearbyContainer)

    VIEWMODEL_PROPERTY(float, NearbyContainerCurrentWeight)
    VIEWMODEL_PROPERTY(float, NearbyContainerMaxWeight)
    VIEWMODEL_PROPERTY(float, NearbyContainerCurrentVolume)
    VIEWMODEL_PROPERTY(float, NearbyContainerMaxVolume)
    VIEWMODEL_PROPERTY(int32, NearbyContainerCellDepthUnits)

    float GetNearbyContainerWeightRatio() const
    {
        return (NearbyContainerMaxWeight > 0.f) ? (NearbyContainerCurrentWeight / NearbyContainerMaxWeight) : 0.f;
    }

    float GetNearbyContainerVolumeRatio() const
    {
        return (NearbyContainerMaxVolume > 0.f) ? (NearbyContainerCurrentVolume / NearbyContainerMaxVolume) : 0.f;
    }

protected:
    UPROPERTY(BlueprintReadOnly, Category = "ViewModel")
    FText NearbyContainerLabel;

    void UpdateNearbyContainerLabel(const FText& InValue)
    {
        NearbyContainerLabel = InValue;
        NotifyPropertyChanged(FName(TEXT("NearbyContainerLabel")));
    }

protected:
    UPROPERTY(BlueprintReadOnly, Category = "ViewModel")
    TArray<FText> PocketContainerLabels;

    void UpdatePocketContainerLabels(const TArray<FText>& InValue)
    {
        PocketContainerLabels = InValue;
        NotifyPropertyChanged(FName(TEXT("PocketContainerLabels")));
    }

public:
    FORCEINLINE const TArray<FText>& GetPocketContainerLabels() const { return PocketContainerLabels; }
    FORCEINLINE const FText& GetNearbyContainerLabel() const { return NearbyContainerLabel; }
    int32 GetPocketContainerCount() const;
    FText GetPocketContainerLabel(int32 PocketIndex) const;
    FGameplayTag GetPocketContainerId(int32 PocketIndex) const;
    int32 GetPocketGridWidth(int32 PocketIndex) const;
    int32 GetPocketGridHeight(int32 PocketIndex) const;
    const TArray<FInventoryCellVisualState>& GetPocketCellVisuals(int32 PocketIndex) const;
    int32 GetPocketCellInstanceId(int32 PocketIndex, int32 CellIndex) const;
    bool IsPocketCellEnabled(int32 PocketIndex, int32 CellIndex) const;

    /**
     * Reverse-lookup: find the pocket index whose container tag equals
     * ContainerTag, or INDEX_NONE. Used by the subsystem-scoped policy
     * fan-out (see IsCellEnabledForSurface / GetCellOccupant) so widgets
     * no longer need to capture PocketIndex in surface-registration
     * closures.
     */
    int32 FindPocketIndexByContainerTag(FGameplayTag ContainerTag) const;

    /** Set secondary grid dimensions for testing/synthetic viewmodels. */
    void SetSecondaryGridDimensions(int32 InWidth, int32 InHeight)
    {
        UpdateSecondaryGridWidth(InWidth);
        UpdateSecondaryGridHeight(InHeight);
    }

    // Hand grids (always visible 2x2, width-only validation)
    static constexpr int32 HandGridSize = 2;
    static constexpr int32 HandCellCount = HandGridSize * HandGridSize;

protected:
    UPROPERTY(BlueprintReadOnly, Category = "ViewModel")
    TArray<FInventoryCellVisualState> LeftHandCellVisuals;

    UPROPERTY(BlueprintReadOnly, Category = "ViewModel")
    TArray<FInventoryCellVisualState> RightHandCellVisuals;

    UPROPERTY(BlueprintReadOnly, Category = "ViewModel")
    TArray<int32> LeftHandCellInstanceIds;

    UPROPERTY(BlueprintReadOnly, Category = "ViewModel")
    TArray<int32> RightHandCellInstanceIds;

public:
    /** Shared sentinel for empty grid cells in inventory instance-id arrays. */
    static constexpr int32 EmptyCellInstanceId = INDEX_NONE;

    FORCEINLINE const TArray<FInventoryCellVisualState>& GetLeftHandCellVisuals() const { return LeftHandCellVisuals; }
    FORCEINLINE const TArray<FInventoryCellVisualState>& GetRightHandCellVisuals() const { return RightHandCellVisuals; }
    FORCEINLINE int32 GetLeftHandInstanceId(int32 CellIndex) const { return LeftHandCellInstanceIds.IsValidIndex(CellIndex) ? LeftHandCellInstanceIds[CellIndex] : EmptyCellInstanceId; }
    FORCEINLINE int32 GetRightHandInstanceId(int32 CellIndex) const { return RightHandCellInstanceIds.IsValidIndex(CellIndex) ? RightHandCellInstanceIds[CellIndex] : EmptyCellInstanceId; }

    void SetLeftHandCellVisuals(const TArray<FInventoryCellVisualState>& InValue);
    void SetRightHandCellVisuals(const TArray<FInventoryCellVisualState>& InValue);
    void SetLeftHandCellInstanceIds(const TArray<int32>& InValue);
    void SetRightHandCellInstanceIds(const TArray<int32>& InValue);

    // Equip slots (display only)
protected:
    UPROPERTY(BlueprintReadOnly, Category = "ViewModel")
    TArray<FText> EquipSlotLabels;

    void UpdateEquipSlotLabels(const TArray<FText>& InValue)
    {
        EquipSlotLabels = InValue;
        NotifyPropertyChanged(FName(TEXT("EquipSlotLabels")));
    }

public:
    FORCEINLINE const TArray<FText>& GetEquipSlotLabels() const { return EquipSlotLabels; }
    void SetEquipSlotLabels(const TArray<FText>& InValue)
    {
        UpdateEquipSlotLabels(InValue);
    }
    void ClearEquipSlotLabels()
    {
        EquipSlotLabels.Empty();
        NotifyPropertyChanged(FName(TEXT("EquipSlotLabels")));
    }

protected:
    UPROPERTY(BlueprintReadOnly, Category = "ViewModel")
    TArray<FInventoryCellVisualState> CellVisuals;

    UPROPERTY(BlueprintReadOnly, Category = "ViewModel")
    TArray<FInventoryCellVisualState> SecondaryCellVisuals;

    void UpdateCellVisuals(const TArray<FInventoryCellVisualState>& InValue);
    void UpdateSecondaryCellVisuals(const TArray<FInventoryCellVisualState>& InValue);

public:
    FORCEINLINE const TArray<FInventoryCellVisualState>& GetCellVisuals() const { return CellVisuals; }
    FORCEINLINE const TArray<FInventoryCellVisualState>& GetSecondaryCellVisuals() const { return SecondaryCellVisuals; }
    FORCEINLINE const TArray<FInventoryEntryView>& GetCachedEntriesForDiagnostics() const { return CachedEntries; }
    FORCEINLINE const TArray<FInventoryEntryView>& GetCachedNearbyEntriesForDiagnostics() const { return CachedNearbyEntries; }
    FORCEINLINE const TArray<FInventoryContainerView>& GetCachedContainersForDiagnostics() const { return CachedContainers; }
    FORCEINLINE const TArray<FInventoryContainerView>& GetCachedPocketContainersForDiagnostics() const { return CachedPocketContainers; }
    /**
     * Runtime accessor for the VM's cached pocket containers.  Used by
     * InventoryViewModelSurfaceDispatch::FindPocketIndexByContainerTag
     * during the drag-host surface lookup - NOT diagnostics-only,
     * despite the ForDiagnostics sibling.
     */
    FORCEINLINE const TArray<FInventoryContainerView>& GetCachedPocketContainers() const { return CachedPocketContainers; }
    FORCEINLINE const FInventoryContainerView& GetCachedNearbyContainerForDiagnostics() const { return CachedNearbyContainer; }
    FORCEINLINE UObject* GetInventorySourceObjectForDiagnostics() const { return InventorySource.GetObject(); }
    FORCEINLINE UObject* GetNearbyContainerSourceObjectForDiagnostics() const { return NearbyContainerSource.GetObject(); }
    void SetCellVisuals(const TArray<FInventoryCellVisualState>& InValue)
    {
        UpdateCellVisuals(InValue);
    }
    void SetSecondaryCellVisuals(const TArray<FInventoryCellVisualState>& InValue)
    {
        UpdateSecondaryCellVisuals(InValue);
    }
    void ClearCellVisuals()
    {
        CellVisuals.Empty();
        NotifyPropertyChanged(FName(TEXT("CellVisuals")));
    }
    void ClearSecondaryCellVisuals()
    {
        SecondaryCellVisuals.Empty();
        NotifyPropertyChanged(FName(TEXT("SecondaryCellVisuals")));
    }

    // Panel visibility
    VIEWMODEL_PROPERTY(bool, bPanelVisible)

    // Lifecycle
    virtual void Initialize(UObject* Context) override;
    virtual void Shutdown() override;

    // Inventory binding
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void SetInventorySource(UObject* InObject);

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void SetNearbyContainerSource(UObject* InObject, const FContainerSessionHandle& InSessionHandle);

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void ClearNearbyContainerSource();

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void RefreshFromInventory();

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void TogglePanel();

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void ShowPanel();

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void HidePanel();

    // Commands (write intents)
    UFUNCTION(BlueprintCallable, Category = "Inventory|Commands")
    void RequestAddItem(FPrimaryAssetId ObjectId, int32 Quantity = 1);

    UFUNCTION(BlueprintCallable, Category = "Inventory|Commands")
    void RequestRemoveItem(int32 InstanceId, int32 Quantity = 1);

    // Virtual so FInventoryDropRouter dispatch can be spy-verified in tests.
    // Production callers route through the router (or the method directly);
    // subclasses must forward to Super:: if they want real inventory writes
    // to happen.
    UFUNCTION(BlueprintCallable, Category = "Inventory|Commands")
    virtual void RequestMoveItem(int32 InstanceId, FGameplayTag FromContainer, FIntPoint FromPos, FGameplayTag ToContainer, FIntPoint ToPos, int32 Quantity, bool bRotated) override;

    UFUNCTION(BlueprintCallable, Category = "Inventory|Commands")
    void RequestUseItem(int32 InstanceId);

    UFUNCTION(BlueprintCallable, Category = "Inventory|Commands")
    void RequestTakeNearbyItem(int32 InstanceId, int32 Quantity = 1);

    // Virtual - see comment on RequestMoveItem.
    UFUNCTION(BlueprintCallable, Category = "Inventory|Commands")
    virtual void RequestTakeNearbyItemToContainer(
        int32 InstanceId,
        FGameplayTag TargetContainerId,
        FIntPoint TargetGridPos,
        bool bTargetRotated,
        int32 Quantity = 1) override;

    UFUNCTION(BlueprintCallable, Category = "Inventory|Commands")
    void RequestStoreItemInNearbyContainer(int32 InstanceId, int32 Quantity = 1);

    // Virtual - see comment on RequestMoveItem.
    UFUNCTION(BlueprintCallable, Category = "Inventory|Commands")
    virtual void RequestStoreItemInNearbyContainerAt(
        int32 InstanceId,
        FIntPoint TargetGridPos,
        bool bTargetRotated,
        int32 Quantity = 1) override;

    UFUNCTION(BlueprintCallable, Category = "Inventory|Commands")
    void RequestTakeAllNearbyContainer();

    // Virtual - same rationale as RequestMoveItem. Dispatches a world-to-world
    // rearrangement within the active nearby container session.
    UFUNCTION(BlueprintCallable, Category = "Inventory|Commands")
    virtual void RequestMoveItemInNearbyContainer(
        int32 InstanceId,
        FIntPoint TargetGridPos,
        bool bTargetRotated,
        int32 Quantity = 1) override;

    UFUNCTION(BlueprintCallable, Category = "Inventory|Commands")
    virtual void RequestEquipItem(int32 InstanceId, FGameplayTag EquipSlot) override;

    /** Equip item using its default equip slot (looks up slot from cached entry). */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Commands")
    void RequestEquipItemAuto(int32 InstanceId);

    UFUNCTION(BlueprintCallable, Category = "Inventory|Commands")
    void RequestUnequipItem(FGameplayTag EquipSlot);

    UFUNCTION(BlueprintCallable, Category = "Inventory|Commands")
    void RequestDropItem(int32 InstanceId, int32 Quantity = 1);

    UFUNCTION(BlueprintCallable, Category = "Inventory|Commands")
    void RequestSplitStack(int32 InstanceId, int32 SplitQuantity);

    /** Split stack in half (convenience for context menu). */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Commands")
    void RequestSplitStackHalf(int32 InstanceId);

    UFUNCTION(BlueprintPure, Category = "Inventory|Commands")
    bool HasCommands() const;

    /**
     * Build generic action descriptors for a given inventory entry.
     * SOT guardrail: keep all action visibility/enabled rules centralized here.
     * Widgets should only consume these descriptors and never duplicate rules.
     */
    void BuildActionDescriptors(const FInventoryEntryView& Entry, TArray<FProjectUIActionDescriptor>& OutActions) const;

    /** Resolve descriptors for entry by instance id. */
    bool TryGetActionDescriptorsByInstanceId(int32 InstanceId, TArray<FProjectUIActionDescriptor>& OutActions) const;

    /** Shared action ids for inventory command presentation. */
    static const FName& GetActionIdUse();
    static const FName& GetActionIdEquip();
    static const FName& GetActionIdDrop();
    static const FName& GetActionIdSplit();

    /** Descriptor utility helpers used by widget adapters. */
    static const FProjectUIActionDescriptor* FindActionDescriptor(const TArray<FProjectUIActionDescriptor>& Actions, FName ActionId);
    static bool IsActionEnabled(const TArray<FProjectUIActionDescriptor>& Actions, FName ActionId);
    static bool HasEnabledActions(const TArray<FProjectUIActionDescriptor>& Actions);

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void SetSelectedContainerIndex(int32 NewIndex);

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void SetSecondaryContainerIndex(int32 NewIndex);

    UFUNCTION(BlueprintPure, Category = "Inventory")
    FGameplayTag GetSelectedContainerId() const;

    UFUNCTION(BlueprintPure, Category = "Inventory")
    FGameplayTag GetSecondaryContainerId() const;

    UFUNCTION(BlueprintPure, Category = "Inventory")
    bool TryGetEntryByCellIndex(int32 CellIndex, FInventoryEntryView& OutEntry) const;

    UFUNCTION(BlueprintPure, Category = "Inventory")
    bool TryGetEntryByInstanceId(int32 InstanceId, FInventoryEntryView& OutEntry) const;

    UFUNCTION(BlueprintPure, Category = "Inventory")
    bool TryGetNearbyEntryByInstanceId(int32 InstanceId, FInventoryEntryView& OutEntry) const;

    UFUNCTION(BlueprintPure, Category = "Inventory")
    bool TryGetSecondaryEntryByCellIndex(int32 CellIndex, FInventoryEntryView& OutEntry) const;

    UFUNCTION(BlueprintPure, Category = "Inventory")
    bool IsCellEnabled(int32 CellIndex) const;

    UFUNCTION(BlueprintPure, Category = "Inventory")
    bool IsSecondaryCellEnabled(int32 CellIndex) const;

    UFUNCTION(BlueprintPure, Category = "Inventory")
    int32 GetCellInstanceId(int32 CellIndex) const;

    UFUNCTION(BlueprintPure, Category = "Inventory")
    int32 GetSecondaryCellInstanceId(int32 CellIndex) const;

    // -------------------------------------------------------------------
    // IInventorySurfacePolicyProvider
    // -------------------------------------------------------------------
    // Slice 18: tag-keyed validation used by UInventoryUIDragHostSubsystem
    // for drop-footprint checks. Replaces per-surface OccupantAllowedChecker
    // lambdas that previously lived in widget code. The surface tag is the
    // policy key; the VM dispatches internally to the right surface-local
    // rule (primary, secondary, pocket, hand, nearby). Self-contained: does
    // not reach global Slate state.
    virtual bool IsPayloadAllowedOnOccupant(
        FGameplayTag SurfaceTag,
        const FProjectUIGridDragPayload& Payload,
        int32 OccupantId,
        int32 CellIndex) const override;

    // Follow-up #2: tag-keyed cell state queries replace widget-owned
    // EnabledChecker/OccupantChecker closures. Surface registration becomes
    // closure-free - the subsystem installs a fan-out to these overrides
    // keyed on SurfaceTag when the checkers on FProjectUIGridSurface are
    // left unset (the Slice 18 contract).
    virtual bool IsCellEnabledForSurface(FGameplayTag SurfaceTag, int32 CellIndex) const override;
    virtual int32 GetCellOccupant(FGameplayTag SurfaceTag, int32 CellIndex) const override;

    UFUNCTION(BlueprintPure, Category = "Inventory")
    bool IsNearbyEntryInstanceId(int32 InstanceId) const;

    UFUNCTION(BlueprintPure, Category = "Inventory")
    bool HasNearbyEntries() const { return CachedNearbyEntries.Num() > 0; }

    /** Resolve the actual inventory container/slot to use when dropping onto a hand grid. */
    bool ResolveHandDropTarget(bool bLeftHand, FGameplayTag& OutContainerId, FIntPoint& OutGridPos) const;
    bool TryResolveFreePlacementInContainer(
        const FGameplayTag& ContainerId,
        FIntPoint ItemSize,
        FIntPoint& OutGridPos,
        TOptional<FIntPoint> ExcludedGridPos = TOptional<FIntPoint>()) const;
    bool TryResolveAlternateHandDropTarget(
        const FGameplayTag& CurrentContainerId,
        FIntPoint CurrentGridPos,
        FIntPoint ItemSize,
        FGameplayTag& OutContainerId,
        FIntPoint& OutGridPos) const;

    UFUNCTION(BlueprintPure, Category = "Inventory")
    bool IsContainerEmpty(FGameplayTag ContainerId, int32 IgnoreInstanceId = -1) const;

    UFUNCTION(BlueprintPure, Category = "Inventory|Equip")
    int32 GetEquipSlotCount() const;

    UFUNCTION(BlueprintPure, Category = "Inventory|Equip")
    FText GetEquipSlotLabel(int32 Index) const;

    UFUNCTION(BlueprintPure, Category = "Inventory|Equip")
    FText GetEquipSlotShortLabel(int32 Index) const;

    UFUNCTION(BlueprintPure, Category = "Inventory|Equip")
    FText GetEquipSlotItemLabel(int32 Index) const;

    UFUNCTION(BlueprintPure, Category = "Inventory|Equip")
    FGameplayTag GetEquipSlotTag(int32 Index) const;

    /** Set equip slot tags directly (for testing/synthetic viewmodels). */
    void SetEquipSlotTags(const TArray<FGameplayTag>& InTags);

    /** Set equip slot short labels directly (for testing/synthetic viewmodels). */
    void SetEquipSlotShortLabels(const TArray<FText>& InLabels);

    UFUNCTION(BlueprintPure, Category = "Inventory|Equip")
    int32 GetEquipSlotInstanceId(int32 Index) const;

    UFUNCTION(BlueprintPure, Category = "Inventory|Equip")
    FString GetEquipSlotItemIconCode(int32 Index) const;

    /** Delegate fired when inventory error occurs (for toast display). */
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnInventoryViewModelError, const FText&);
    FOnInventoryViewModelError OnInventoryError;

private:
    void EnsureInventorySourceFromContext();
    UObject* ResolveInventorySourceFromContext() const;

    UPROPERTY()
    TScriptInterface<IInventoryReadOnly> InventorySource;

    UPROPERTY()
    TScriptInterface<IInventoryCommands> InventoryCommands;

    UPROPERTY()
    TScriptInterface<IWorldContainerSessionSource> NearbyContainerSource;

    FContainerSessionHandle NearbySessionHandle;

    void HandleInventoryViewChanged();
    void HandleInventoryErrorFromSource(const FText& ErrorMessage);
    void HandleWorldContainerSessionOpened(UObject* WorldContainerSource, const FContainerSessionHandle& SessionHandle);
    void HandleWorldContainerSessionClosed(const FContainerSessionHandle& SessionHandle);
    void RefreshNearbyContainerData();
    void ClearNearbyContainerData();
    bool DoesCachedInventoryPlacementOverlap(
        const FGameplayTag& ContainerId,
        FIntPoint GridPos,
        FIntPoint ItemSize,
        int32 IgnoreInstanceId) const;

    void BuildContainerData(const TArray<FInventoryContainerView>& Containers);
    void BuildCellVisuals(const TArray<FInventoryEntryView>& Entries);       // SOLID: uses FInventoryViewModelCellBuilder
    void BuildSecondaryCellVisuals(const TArray<FInventoryEntryView>& Entries); // SOLID: uses FInventoryViewModelCellBuilder
    void BuildHandCellVisuals(const TArray<FInventoryEntryView>& Entries);  // SOLID: uses FInventoryViewModelCellBuilder
    void BuildPocketCellVisuals(const TArray<FInventoryEntryView>& Entries);
    void NotifyPocketCellVisualsChanged();
    void BuildEquipSlotLabels(const TArray<FInventoryEntryView>& Entries); // SOLID: uses FInventoryViewModelEquipSlotBuilder

    UObject* FindInventorySourceFromActor(AActor* Actor) const;

    TArray<FInventoryContainerView> CachedAllContainers;
    TArray<FInventoryEntryView> CachedEntries;
    FInventoryContainerView CachedNearbyContainer;
    TArray<FInventoryEntryView> CachedNearbyEntries;
    TArray<FInventoryContainerView> CachedContainers;
    TArray<FInventoryContainerView> CachedPocketContainers;
    TArray<int32> CellInstanceIds;
    TArray<bool> CellEnabled;
    TArray<int32> SecondaryCellInstanceIds;
    TArray<bool> SecondaryCellEnabled;
    TArray<int32> NearbyCellInstanceIds;
    TArray<bool> NearbyCellEnabled;
    TArray<TArray<FInventoryCellVisualState>> PocketCellVisuals;
    TArray<TArray<int32>> PocketCellInstanceIds;
    TArray<TArray<bool>> PocketCellEnabled;
    TArray<FGameplayTag> EquipSlotTags;
    TArray<int32> EquipSlotInstanceIds;
    TArray<FText> EquipSlotShortLabels;
    TArray<FText> EquipSlotItemLabels;
    TArray<FString> EquipSlotItemIconCodes;
    TWeakObjectPtr<UObject> InitializationContext;
};
