// Copyright ALIS. All Rights Reserved.

#include "Operations/WorldContainerMoveOp.h"

#include "Interfaces/IWorldContainerSessionSource.h"
#include "Types/ContainerSessionTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogWorldContainerMoveOp, Log, All);

bool FWorldContainerMoveOp::Execute(
	UObject* SourceObject,
	const FGuid& SessionId,
	int32 EntryInstanceId,
	int32 Quantity,
	FIntPoint TargetGridPos,
	bool bTargetRotated,
	FText& OutError)
{
	OutError = FText::GetEmpty();

	if (!SourceObject)
	{
		OutError = NSLOCTEXT("WorldContainerMoveOp", "MissingSource",
			"Container session source is no longer valid.");
		return false;
	}

	if (!SessionId.IsValid())
	{
		OutError = NSLOCTEXT("WorldContainerMoveOp", "InvalidSession",
			"Container session handle is not active.");
		return false;
	}

	if (EntryInstanceId <= 0 || Quantity <= 0)
	{
		OutError = NSLOCTEXT("WorldContainerMoveOp", "InvalidArgs",
			"Invalid move request.");
		return false;
	}

	if (TargetGridPos.X < 0 || TargetGridPos.Y < 0)
	{
		OutError = NSLOCTEXT("WorldContainerMoveOp", "InvalidTarget",
			"Target cell is invalid.");
		return false;
	}

	// Snapshot source entry BEFORE consuming so we can rollback on store failure.
	FContainerEntryTransfer SourceSnapshot;
	{
		const TArray<FInventoryEntryView> CurrentViews =
			IWorldContainerSessionSource::Execute_GetContainerEntryViews(SourceObject);
		const FInventoryEntryView* Found = CurrentViews.FindByPredicate(
			[EntryInstanceId](const FInventoryEntryView& V)
			{
				return V.InstanceId == EntryInstanceId;
			});
		if (!Found)
		{
			OutError = NSLOCTEXT("WorldContainerMoveOp", "SourceMissing",
				"Source entry is no longer in the container.");
			return false;
		}
		SourceSnapshot.EntryInstanceId = Found->InstanceId;
		SourceSnapshot.ObjectId = Found->ItemId;
		SourceSnapshot.Quantity = FMath::Clamp(Quantity, 1, Found->Quantity);
		SourceSnapshot.GridPos = Found->GridPos;
		SourceSnapshot.bRotated = Found->bRotated;
	}

	// Consume first so the Store overlap check does not see the source cell.
	{
		FContainerEntryTransfer ConsumeEntry;
		ConsumeEntry.EntryInstanceId = SourceSnapshot.EntryInstanceId;
		ConsumeEntry.ObjectId = SourceSnapshot.ObjectId;
		ConsumeEntry.Quantity = SourceSnapshot.Quantity;

		TArray<FContainerEntryTransfer> ConsumeList{ ConsumeEntry };
		if (!IWorldContainerSessionSource::Execute_ConsumeContainerEntries(
				SourceObject, SessionId, ConsumeList, OutError))
		{
			UE_LOG(LogWorldContainerMoveOp, Log,
				TEXT("Consume rejected InstanceId=%d x%d - %s"),
				SourceSnapshot.EntryInstanceId, SourceSnapshot.Quantity, *OutError.ToString());
			if (OutError.IsEmpty())
			{
				OutError = NSLOCTEXT("WorldContainerMoveOp", "ConsumeFailed",
					"Failed to lift item from its source cell.");
			}
			return false;
		}
	}

	// Store at target. Rollback to snapshot on failure.
	FContainerEntryTransfer StoreEntry;
	StoreEntry.ObjectId = SourceSnapshot.ObjectId;
	StoreEntry.Quantity = SourceSnapshot.Quantity;
	StoreEntry.GridPos = TargetGridPos;
	StoreEntry.bRotated = bTargetRotated;
	TArray<FContainerEntryTransfer> StoreList{ StoreEntry };
	if (!IWorldContainerSessionSource::Execute_StoreContainerEntries(
			SourceObject, SessionId, StoreList, OutError))
	{
		UE_LOG(LogWorldContainerMoveOp, Log,
			TEXT("Store rejected at (%d,%d) rot:%d - %s; rolling back"),
			TargetGridPos.X, TargetGridPos.Y, bTargetRotated ? 1 : 0, *OutError.ToString());

		FContainerEntryTransfer RollbackEntry;
		RollbackEntry.ObjectId = SourceSnapshot.ObjectId;
		RollbackEntry.Quantity = SourceSnapshot.Quantity;
		RollbackEntry.GridPos = SourceSnapshot.GridPos;
		RollbackEntry.bRotated = SourceSnapshot.bRotated;
		TArray<FContainerEntryTransfer> RollbackList{ RollbackEntry };
		FText RollbackError;
		if (!IWorldContainerSessionSource::Execute_StoreContainerEntries(
				SourceObject, SessionId, RollbackList, RollbackError))
		{
			// Container state may drift; higher layer should close + resync.
			UE_LOG(LogWorldContainerMoveOp, Error,
				TEXT("Rollback ALSO failed: %s. Container state may drift."),
				*RollbackError.ToString());
		}

		if (OutError.IsEmpty())
		{
			OutError = NSLOCTEXT("WorldContainerMoveOp", "StoreFailed",
				"Cannot place item at that cell.");
		}
		return false;
	}

	UE_LOG(LogWorldContainerMoveOp, Log,
		TEXT("Moved InstanceId=%d x%d -> (%d,%d) rot:%d"),
		SourceSnapshot.EntryInstanceId, SourceSnapshot.Quantity,
		TargetGridPos.X, TargetGridPos.Y, bTargetRotated ? 1 : 0);
	return true;
}
