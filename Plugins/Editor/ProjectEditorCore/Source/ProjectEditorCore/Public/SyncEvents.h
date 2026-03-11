// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Single file change in a sync batch.
 */
struct PROJECTEDITORCORE_API FSyncBatchFile
{
	FString FilePath;
	FString ContentHash;
};

/**
 * Delegate for sync batch events.
 *
 * Publishers: ProjectAssetSync and other sync utilities.
 * Subscribers: systems that need to ignore or handle external file edits.
 *
 * @param Origin Identifier for the source of the batch (e.g., "JsonRefSync")
 * @param ChangedFiles Full paths and hashes of files modified by the batch
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSyncBatchApplied, FName /*Origin*/, const TArray<FSyncBatchFile>& /*ChangedFiles*/);

/**
 * Global accessor for sync batch events.
 */
class PROJECTEDITORCORE_API FSyncEvents
{
public:
	/** Get the singleton delegate instance */
	static FOnSyncBatchApplied& OnSyncBatchApplied();

private:
	static FOnSyncBatchApplied SyncBatchAppliedDelegate;
};
