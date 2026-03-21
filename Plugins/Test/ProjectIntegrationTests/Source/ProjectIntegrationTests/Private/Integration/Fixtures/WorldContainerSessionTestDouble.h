// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/IWorldContainerSessionSource.h"
#include "ProjectGameplayTags.h"
#include "WorldContainerSessionTestDouble.generated.h"

UCLASS()
class UWorldContainerSessionTestDouble : public UActorComponent, public IWorldContainerSessionSource
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FText DisplayLabel;

	UPROPERTY()
	FInventoryContainerView ContainerView;

	UPROPERTY()
	TArray<FInventoryEntryView> EntryViews;

	UPROPERTY()
	FWorldContainerKey ContainerKey;

	UPROPERTY()
	bool bSupportsQuickLoot = true;

	UPROPERTY()
	bool bSupportsFullOpen = true;

	UPROPERTY()
	bool bFailConsume = false;

	UPROPERTY()
	bool bFailStore = false;

	UPROPERTY()
	int32 NextInstanceId = 1000;

	virtual FText GetContainerDisplayLabel_Implementation() const override
	{
		return DisplayLabel;
	}

	virtual FInventoryContainerView GetContainerView_Implementation() const override
	{
		return ContainerView;
	}

	virtual TArray<FInventoryEntryView> GetContainerEntryViews_Implementation() const override
	{
		return EntryViews;
	}

	virtual FWorldContainerKey GetWorldContainerKey_Implementation() const override
	{
		return ContainerKey;
	}

	virtual bool SupportsContainerSession_Implementation(EContainerSessionMode Mode) const override
	{
		return Mode == EContainerSessionMode::QuickLoot ? bSupportsQuickLoot : bSupportsFullOpen;
	}

	virtual bool TryBeginContainerSession_Implementation(
		AActor* Instigator,
		FGuid SessionId,
		EContainerSessionMode Mode,
		FText& OutError) override
	{
		(void)Instigator;
		OutError = FText::GetEmpty();

		if (!SupportsContainerSession_Implementation(Mode))
		{
			OutError = NSLOCTEXT("WorldContainerSessionTestDouble", "UnsupportedMode", "Mode is not supported.");
			return false;
		}

		if (Mode == EContainerSessionMode::QuickLoot)
		{
			return true;
		}

		if (ActiveFullOpenSessionId.IsValid() && ActiveFullOpenSessionId != SessionId)
		{
			OutError = NSLOCTEXT("WorldContainerSessionTestDouble", "Busy", "Container is busy.");
			return false;
		}

		ActiveFullOpenSessionId = SessionId;
		return true;
	}

	virtual bool ConsumeContainerEntries_Implementation(
		FGuid SessionId,
		const TArray<FContainerEntryTransfer>& Entries,
		FText& OutError) override
	{
		OutError = FText::GetEmpty();

		if (bFailConsume)
		{
			OutError = NSLOCTEXT("WorldContainerSessionTestDouble", "ForcedConsumeFailure", "Forced consume failure.");
			return false;
		}

		if (ActiveFullOpenSessionId.IsValid() && ActiveFullOpenSessionId != SessionId)
		{
			OutError = NSLOCTEXT("WorldContainerSessionTestDouble", "ConsumeBusy", "Container is busy.");
			return false;
		}

		TMap<int32, int32> RequestedByInstanceId;
		for (const FContainerEntryTransfer& Entry : Entries)
		{
			if (Entry.IsValidForConsume())
			{
				RequestedByInstanceId.FindOrAdd(Entry.EntryInstanceId) += Entry.Quantity;
			}
		}

		for (const TPair<int32, int32>& Pair : RequestedByInstanceId)
		{
			const FInventoryEntryView* ExistingEntry = EntryViews.FindByPredicate([&Pair](const FInventoryEntryView& Entry)
			{
				return Entry.InstanceId == Pair.Key;
			});
			if (!ExistingEntry || ExistingEntry->Quantity < Pair.Value)
			{
				OutError = NSLOCTEXT("WorldContainerSessionTestDouble", "MissingConsumeEntry", "Requested entry is unavailable.");
				return false;
			}
		}

		for (int32 Index = EntryViews.Num() - 1; Index >= 0; --Index)
		{
			FInventoryEntryView& Entry = EntryViews[Index];
			if (int32* RequestedQuantity = RequestedByInstanceId.Find(Entry.InstanceId))
			{
				Entry.Quantity -= FMath::Min(Entry.Quantity, *RequestedQuantity);
				if (Entry.Quantity <= 0)
				{
					EntryViews.RemoveAt(Index);
				}
			}
		}

		return true;
	}

	virtual bool CanStoreContainerEntries_Implementation(
		FGuid SessionId,
		const TArray<FContainerEntryTransfer>& Entries,
		FText& OutError) const override
	{
		(void)SessionId;
		OutError = FText::GetEmpty();
		for (const FContainerEntryTransfer& Entry : Entries)
		{
			if (!Entry.IsValidForStore())
			{
				OutError = NSLOCTEXT("WorldContainerSessionTestDouble", "InvalidStoreEntry", "Store payload is invalid.");
				return false;
			}
		}
		return Entries.Num() > 0;
	}

	virtual bool StoreContainerEntries_Implementation(
		FGuid SessionId,
		const TArray<FContainerEntryTransfer>& Entries,
		FText& OutError) override
	{
		OutError = FText::GetEmpty();

		if (bFailStore)
		{
			OutError = NSLOCTEXT("WorldContainerSessionTestDouble", "ForcedStoreFailure", "Forced store failure.");
			return false;
		}

		if (ActiveFullOpenSessionId.IsValid() && ActiveFullOpenSessionId != SessionId)
		{
			OutError = NSLOCTEXT("WorldContainerSessionTestDouble", "StoreBusy", "Container is busy.");
			return false;
		}

		for (const FContainerEntryTransfer& Entry : Entries)
		{
			if (!Entry.IsValidForStore())
			{
				OutError = NSLOCTEXT("WorldContainerSessionTestDouble", "InvalidStorePayload", "Store payload is invalid.");
				return false;
			}

			FInventoryEntryView NewEntry;
			NewEntry.ItemId = Entry.ObjectId;
			NewEntry.Quantity = Entry.Quantity;
			NewEntry.InstanceId = NextInstanceId++;
			NewEntry.ContainerId = ProjectTags::Item_Container_WorldStorage;
			NewEntry.GridPos = Entry.HasPlacement() ? Entry.GridPos : FIntPoint(0, 0);
			NewEntry.bRotated = Entry.bRotated;
			NewEntry.GridSize = FIntPoint(1, 1);
			EntryViews.Add(MoveTemp(NewEntry));
		}

		return true;
	}

	virtual bool EndContainerSession_Implementation(FGuid SessionId) override
	{
		if (!ActiveFullOpenSessionId.IsValid() || ActiveFullOpenSessionId != SessionId)
		{
			return false;
		}

		ActiveFullOpenSessionId.Invalidate();
		return true;
	}

	virtual bool HasActiveFullOpenSession_Implementation() const override
	{
		return ActiveFullOpenSessionId.IsValid();
	}

private:
	FGuid ActiveFullOpenSessionId;
};
