// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IInventoryReadOnly.h"
#include "ProjectInventoryReadOnlyMock.generated.h"

/**
 * Lightweight inventory source used by UI integration tests.
 * Keeps the real UInventoryViewModel refresh path while avoiding gameplay setup.
 */
UCLASS()
class PROJECTINTEGRATIONTESTS_API UProjectInventoryReadOnlyMock : public UObject, public IInventoryReadOnly
{
    GENERATED_BODY()

public:
    void SetEntries(const TArray<FInventoryEntryView>& InEntries);
    void SetContainers(const TArray<FInventoryContainerView>& InContainers);
    void SetTotals(float InCurrentWeight, float InMaxWeight, float InCurrentVolume, float InMaxVolume, int32 InMaxSlots = 0);

    void BroadcastInventoryChanged();
    void BroadcastInventoryError(const FText& ErrorMessage);

    // IInventoryReadOnly
    virtual void GetEntriesView(TArray<FInventoryEntryView>& OutEntries) const override;
    virtual void GetContainersView(TArray<FInventoryContainerView>& OutContainers) const override;
    virtual float GetCurrentWeight() const override;
    virtual float GetMaxWeight() const override;
    virtual float GetCurrentVolume() const override;
    virtual float GetMaxVolume() const override;
    virtual int32 GetMaxSlots() const override;
    virtual FOnInventoryViewChanged& OnInventoryViewChanged() override;
    virtual FOnInventoryErrorNative& OnInventoryErrorNative() override;

private:
    UPROPERTY()
    TArray<FInventoryEntryView> Entries;

    UPROPERTY()
    TArray<FInventoryContainerView> Containers;

    float CurrentWeight = 0.f;
    float MaxWeight = 0.f;
    float CurrentVolume = 0.f;
    float MaxVolume = 0.f;
    int32 MaxSlots = 0;

    FOnInventoryViewChanged InventoryViewChangedDelegate;
    FOnInventoryErrorNative InventoryErrorDelegate;
};

