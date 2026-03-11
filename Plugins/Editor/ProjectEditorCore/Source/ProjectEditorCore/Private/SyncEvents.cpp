// Copyright ALIS. All Rights Reserved.

#include "SyncEvents.h"

FOnSyncBatchApplied FSyncEvents::SyncBatchAppliedDelegate;

FOnSyncBatchApplied& FSyncEvents::OnSyncBatchApplied()
{
	return SyncBatchAppliedDelegate;
}
