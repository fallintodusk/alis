// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "MountContentPhaseExecutor.h"
#include "ProjectLoadingLog.h"
#include "ProjectLoadingSubsystem.h"

#define LOCTEXT_NAMESPACE "ProjectLoadPhaseExecutors"

FText FMountContentPhaseExecutor::GetPhaseName() const
{
	return LOCTEXT("MountContentPhase", "Mount Content");
}

bool FMountContentPhaseExecutor::ShouldSkip(const FLoadRequest& Request) const
{
	// Skip if no content packs need mounting
	return Request.ContentPacksToMount.Num() == 0;
}

FProjectPhaseResult FMountContentPhaseExecutor::Execute(FProjectPhaseContext& Context)
{
	const FText Error = FText::Format(
		LOCTEXT("MountContentUnavailable", "Runtime content mounting is not implemented ({0} requested pack(s))"),
		FText::AsNumber(Context.Request.ContentPacksToMount.Num()));

	UE_LOG(LogProjectLoading, Error, TEXT("Phase 2: Mount Content - %s"), *Error.ToString());
	return FProjectPhaseResult::Failure(Error, ProjectLoadingErrors::MountContentFailed);
}

#undef LOCTEXT_NAMESPACE
