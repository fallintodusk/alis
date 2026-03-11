// Copyright ALIS. All Rights Reserved.

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
	// Phase 2: Mount Content is now handled by Orchestrator during plugin loading.
	// This phase is reserved/stub - all content mounting (DLC + feature plugins) happens via Orchestrator.
	UE_LOG(LogProjectLoading, Verbose, TEXT("Phase 2: Mount Content - Skipped (handled by Orchestrator)"));
	ReportProgress(Context, 1.0f, LOCTEXT("PhaseHandledByOrchestrator", "Content mounting handled by Orchestrator"));

	return FProjectPhaseResult::Skipped();
}

#undef LOCTEXT_NAMESPACE

