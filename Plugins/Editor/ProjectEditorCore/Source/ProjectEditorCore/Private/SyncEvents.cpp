// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "SyncEvents.h"

FOnSyncBatchApplied FSyncEvents::SyncBatchAppliedDelegate;

FOnSyncBatchApplied& FSyncEvents::OnSyncBatchApplied()
{
	return SyncBatchAppliedDelegate;
}
