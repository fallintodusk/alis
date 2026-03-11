// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MVVM/ProjectViewModel.h"
#include "Interfaces/IInventoryCommands.h"
#include "Interfaces/IInventoryReadOnly.h"
#include "Presentation/ProjectUIActionDescriptor.h"
#include "InventoryViewModel.generated.h"

class AActor;

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
 * - FInventoryViewModelCellBuilder (cell text arrays)
 * - FInventoryViewModelEquipSlotBuilder (equip slot labels)
 */
UCLASS(BlueprintType)
class PROJECTINVENTORYUI_API UInventoryViewModel : public UProjectViewModel
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
    int32 GetPocketContainerCount() const;
    FText GetPocketContainerLabel(int32 PocketIndex) const;
    FGameplayTag GetPocketContainerId(int32 PocketIndex) const;
    int32 GetPocketGridWidth(int32 PocketIndex) const;
    int32 GetPocketGridHeight(int32 PocketIndex) const;
    const TArray<FText>& GetPocketCellTexts(int32 PocketIndex) const;
    int32 GetPocketCellInstanceId(int32 PocketIndex, int32 CellIndex) const;
    bool IsPocketCellEnabled(int32 PocketIndex, int32 CellIndex) const;

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
    TArray<FText> LeftHandCellTexts;

    UPROPERTY(BlueprintReadOnly, Category = "ViewModel")
    TArray<FText> RightHandCellTexts;

    UPROPERTY(BlueprintReadOnly, Category = "ViewModel")
    TArray<int32> LeftHandCellInstanceIds;

    UPROPERTY(BlueprintReadOnly, Category = "ViewModel")
    TArray<int32> RightHandCellInstanceIds;

public:
    /** Shared sentinel for empty grid cells in inventory instance-id arrays. */
    static constexpr int32 EmptyCellInstanceId = INDEX_NONE;

    FORCEINLINE const TArray<FText>& GetLeftHandCellTexts() const { return LeftHandCellTexts; }
    FORCEINLINE const TArray<FText>& GetRightHandCellTexts() const { return RightHandCellTexts; }
    FORCEINLINE int32 GetLeftHandInstanceId(int32 CellIndex) const { return LeftHandCellInstanceIds.IsValidIndex(CellIndex) ? LeftHandCellInstanceIds[CellIndex] : EmptyCellInstanceId; }
    FORCEINLINE int32 GetRightHandInstanceId(int32 CellIndex) const { return RightHandCellInstanceIds.IsValidIndex(CellIndex) ? RightHandCellInstanceIds[CellIndex] : EmptyCellInstanceId; }

    void SetLeftHandCellTexts(const TArray<FText>& InValue);
    void SetRightHandCellTexts(const TArray<FText>& InValue);
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
    TArray<FText> CellTexts;

    UPROPERTY(BlueprintReadOnly, Category = "ViewModel")
    TArray<FText> SecondaryCellTexts;

    void UpdateCellTexts(const TArray<FText>& InValue)
    {
        CellTexts = InValue;
        NotifyPropertyChanged(FName(TEXT("CellTexts")));
    }

    void UpdateSecondaryCellTexts(const TArray<FText>& InValue)
    {
        SecondaryCellTexts = InValue;
        NotifyPropertyChanged(FName(TEXT("SecondaryCellTexts")));
    }

public:
    FORCEINLINE const TArray<FText>& GetCellTexts() const { return CellTexts; }
    FORCEINLINE const TArray<FText>& GetSecondaryCellTexts() const { return SecondaryCellTexts; }
    void SetCellTexts(const TArray<FText>& InValue)
    {
        UpdateCellTexts(InValue);
    }
    void SetSecondaryCellTexts(const TArray<FText>& InValue)
    {
        UpdateSecondaryCellTexts(InValue);
    }
    void ClearCellTexts()
    {
        CellTexts.Empty();
        NotifyPropertyChanged(FName(TEXT("CellTexts")));
    }
    void ClearSecondaryCellTexts()
    {
        SecondaryCellTexts.Empty();
        NotifyPropertyChanged(FName(TEXT("SecondaryCellTexts")));
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

    UFUNCTION(BlueprintCallable, Category = "Inventory|Commands")
    void RequestMoveItem(int32 InstanceId, FGameplayTag FromContainer, FIntPoint FromPos, FGameplayTag ToContainer, FIntPoint ToPos, int32 Quantity, bool bRotated);

    UFUNCTION(BlueprintCallable, Category = "Inventory|Commands")
    void RequestUseItem(int32 InstanceId);

    UFUNCTION(BlueprintCallable, Category = "Inventory|Commands")
    void RequestEquipItem(int32 InstanceId, FGameplayTag EquipSlot);

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
    bool TryGetSecondaryEntryByCellIndex(int32 CellIndex, FInventoryEntryView& OutEntry) const;

    UFUNCTION(BlueprintPure, Category = "Inventory")
    bool IsCellEnabled(int32 CellIndex) const;

    UFUNCTION(BlueprintPure, Category = "Inventory")
    bool IsSecondaryCellEnabled(int32 CellIndex) const;

    UFUNCTION(BlueprintPure, Category = "Inventory")
    int32 GetCellInstanceId(int32 CellIndex) const;

    UFUNCTION(BlueprintPure, Category = "Inventory")
    int32 GetSecondaryCellInstanceId(int32 CellIndex) const;

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
    UPROPERTY()
    TScriptInterface<IInventoryReadOnly> InventorySource;

    UPROPERTY()
    TScriptInterface<IInventoryCommands> InventoryCommands;

    void HandleInventoryViewChanged();
    void HandleInventoryErrorFromSource(const FText& ErrorMessage);

    void BuildContainerData(const TArray<FInventoryContainerView>& Containers);
    void BuildCellTexts(const TArray<FInventoryEntryView>& Entries);       // SOLID: uses FInventoryViewModelCellBuilder
    void BuildSecondaryCellTexts(const TArray<FInventoryEntryView>& Entries); // SOLID: uses FInventoryViewModelCellBuilder
    void BuildHandCellTexts(const TArray<FInventoryEntryView>& Entries);  // SOLID: uses FInventoryViewModelCellBuilder
    void BuildPocketCellTexts(const TArray<FInventoryEntryView>& Entries);
    void NotifyPocketCellTextsChanged();
    void BuildEquipSlotLabels(const TArray<FInventoryEntryView>& Entries); // SOLID: uses FInventoryViewModelEquipSlotBuilder

    UObject* FindInventorySourceFromActor(AActor* Actor) const;

    TArray<FInventoryContainerView> CachedAllContainers;
    TArray<FInventoryEntryView> CachedEntries;
    TArray<FInventoryContainerView> CachedContainers;
    TArray<FInventoryContainerView> CachedPocketContainers;
    TArray<int32> CellInstanceIds;
    TArray<bool> CellEnabled;
    TArray<int32> SecondaryCellInstanceIds;
    TArray<bool> SecondaryCellEnabled;
    TArray<TArray<FText>> PocketCellTexts;
    TArray<TArray<int32>> PocketCellInstanceIds;
    TArray<TArray<bool>> PocketCellEnabled;
    TArray<FGameplayTag> EquipSlotTags;
    TArray<int32> EquipSlotInstanceIds;
    TArray<FText> EquipSlotShortLabels;
    TArray<FText> EquipSlotItemLabels;
    TArray<FString> EquipSlotItemIconCodes;
};
