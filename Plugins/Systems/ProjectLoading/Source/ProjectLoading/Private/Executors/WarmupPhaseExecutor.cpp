// Copyright ALIS. All Rights Reserved.

#include "WarmupPhaseExecutor.h"
#include "ProjectLoadingLog.h"
#include "ProjectLoadingSubsystem.h"
#include "ShaderPipelineCache.h"
#include "Engine/World.h"
#include "Engine/LevelStreaming.h"

#define LOCTEXT_NAMESPACE "ProjectLoadPhaseExecutors"

FText FWarmupPhaseExecutor::GetPhaseName() const
{
	return LOCTEXT("WarmupPhase", "Warmup");
}

// NOTE: ShouldSkip() intentionally NOT implemented - see header comment
// We must always run Execute() to release preload handles from Phase 3

FProjectPhaseResult FWarmupPhaseExecutor::Execute(FProjectPhaseContext& Context)
{
	UE_LOG(LogProjectLoading, Display, TEXT("Phase 6: Warmup - Starting"));
	ReportProgress(Context, 0.0f, LOCTEXT("WarmupStart", "Warming up level..."));

	// CRITICAL: Release preload handles FIRST (unconditionally)
	// This must happen even on cancellation/skip paths to prevent memory leaks
	// Map has loaded by this point, WP can take over asset references
	if (Context.Runtime.PreloadHandles.Num() > 0)
	{
		UE_LOG(LogProjectLoading, Display, TEXT("Phase 6: Warmup - Releasing %d preload handle(s) from Phase 3"),
			Context.Runtime.PreloadHandles.Num());
		Context.Runtime.ReleasePreloadHandles();
	}

	// Check cancellation (after handle release)
	if (CheckCancellation(Context))
	{
		UE_LOG(LogProjectLoading, Warning, TEXT("Phase 6: Warmup - Cancelled (handles already released)"));
		return FProjectPhaseResult::Cancelled();
	}

	// Check if warmup is enabled (handles already released above)
	if (!Context.Request.bPerformWarmup)
	{
		UE_LOG(LogProjectLoading, Verbose, TEXT("Phase 6: Warmup - Skipped (disabled in request, handles released)"));
		ReportProgress(Context, 1.0f, LOCTEXT("WarmupSkipped", "Warmup skipped"));
		return FProjectPhaseResult::Skipped();
	}

	// Shader warmup implementation
	UE_LOG(LogProjectLoading, Verbose, TEXT("Phase 6: Warmup - Starting shader precompilation"));

	// Trigger shader precompilation (non-blocking)
	// This queues PSOs (Pipeline State Objects) for precompilation in the background
	FShaderPipelineCache::SetBatchMode(FShaderPipelineCache::BatchMode::Background);

	UE_LOG(LogProjectLoading, Verbose, TEXT("Phase 6: Warmup - Shader precompilation queued"));
	ReportProgress(Context, 0.1f, LOCTEXT("ShaderPrecompileQueued", "Precompiling shaders..."));

	// Poll shader compilation progress with periodic updates
	const float PollInterval = 0.1f; // 100ms between checks
	const float MaxWarmupTime = 30.0f; // Maximum 30 seconds for warmup
	float ElapsedTime = 0.0f;
	int32 LastRemaining = -1;

	while (ElapsedTime < MaxWarmupTime)
	{
		// Check cancellation
		if (CheckCancellation(Context))
		{
			UE_LOG(LogProjectLoading, Warning, TEXT("Phase 6: Warmup - Cancelled during shader warmup"));
			return FProjectPhaseResult::Cancelled();
		}

		// Check if precompilation is complete
		const int32 Remaining = FShaderPipelineCache::NumPrecompilesRemaining();

		// If no shaders remaining, warmup complete
		if (Remaining == 0)
		{
			UE_LOG(LogProjectLoading, Verbose, TEXT("Phase 6: Warmup - All shaders precompiled"));
			break;
		}

		// Log progress if remaining count changed
		if (Remaining != LastRemaining)
		{
			UE_LOG(LogProjectLoading, Verbose, TEXT("Phase 6: Warmup - Shaders remaining: %d"), Remaining);
			LastRemaining = Remaining;
		}

		// Calculate progress (0.1 to 0.9 range, reserve 0.0-0.1 for queue, 0.9-1.0 for finalization)
		// Progress decreases remaining count (more remaining = less progress)
		const float ShaderProgress = 0.1f + (0.8f * FMath::Clamp(1.0f - (Remaining / 1000.0f), 0.0f, 1.0f));
		ReportProgress(Context, ShaderProgress, FText::Format(
			LOCTEXT("ShaderPrecompileProgress", "Precompiling shaders... ({0} remaining)"),
			FText::AsNumber(Remaining)
		));

		// Sleep before next poll
		FPlatformProcess::Sleep(PollInterval);
		ElapsedTime += PollInterval;
	}

	// Check if we timed out
	if (ElapsedTime >= MaxWarmupTime)
	{
		const int32 FinalRemaining = FShaderPipelineCache::NumPrecompilesRemaining();
		UE_LOG(LogProjectLoading, Warning, TEXT("Phase 6: Warmup - Timeout after %.1fs (%d shaders remaining, will continue in background)"),
			ElapsedTime, FinalRemaining);
	}

	UE_LOG(LogProjectLoading, Verbose, TEXT("Phase 6: Warmup - Shader warmup complete (elapsed: %.2fs)"), ElapsedTime);
	ReportProgress(Context, 0.9f, LOCTEXT("ShaderWarmupComplete", "Shader warmup complete"));

	// Level streaming pre-cache implementation (optional - handles already released above)
	// NOTE: WP maps may not have traditional streaming levels - don't use StreamingLevels.Num()==0 as gate
	UE_LOG(LogProjectLoading, Verbose, TEXT("Phase 6: Warmup - Starting level streaming pre-cache"));

	// Get the world for level streaming (optional optimization, not critical)
	UProjectLoadingSubsystem* LoadingSubsystem = Context.Subsystem.Get();
	if (!LoadingSubsystem)
	{
		UE_LOG(LogProjectLoading, Verbose, TEXT("Phase 6: Warmup - LoadingSubsystem unavailable, skipping level pre-cache"));
		ReportProgress(Context, 1.0f, LOCTEXT("WarmupComplete", "Warmup complete"));
		UE_LOG(LogProjectLoading, Display, TEXT("Phase 6: Warmup - Completed successfully"));
		return FProjectPhaseResult::Success();
	}

	UWorld* World = LoadingSubsystem->GetWorld();
	if (!World)
	{
		UE_LOG(LogProjectLoading, Verbose, TEXT("Phase 6: Warmup - World unavailable, skipping level pre-cache"));
		ReportProgress(Context, 1.0f, LOCTEXT("WarmupComplete", "Warmup complete"));
		UE_LOG(LogProjectLoading, Display, TEXT("Phase 6: Warmup - Completed successfully"));
		return FProjectPhaseResult::Success();
	}

	// Get all level streaming objects (traditional level streaming, NOT World Partition)
	// WP maps may have 0 streaming levels - this is normal, not an error
	const TArray<ULevelStreaming*>& StreamingLevels = World->GetStreamingLevels();

	if (StreamingLevels.Num() == 0)
	{
		UE_LOG(LogProjectLoading, Verbose, TEXT("Phase 6: Warmup - No traditional streaming levels (WP map or simple level)"));
	}
	else
	{
		UE_LOG(LogProjectLoading, Verbose, TEXT("Phase 6: Warmup - Found %d streaming level(s) to pre-cache"), StreamingLevels.Num());

		// Pre-cache streaming levels by requesting load without making visible
		int32 LevelsCached = 0;
		for (ULevelStreaming* StreamingLevel : StreamingLevels)
		{
			if (!StreamingLevel)
			{
				continue;
			}

			// Check cancellation
			if (CheckCancellation(Context))
			{
				UE_LOG(LogProjectLoading, Warning, TEXT("Phase 6: Warmup - Cancelled during level streaming pre-cache"));
				return FProjectPhaseResult::Cancelled();
			}

			// Only pre-cache levels that are not already loaded
			if (StreamingLevel->IsLevelLoaded())
			{
				UE_LOG(LogProjectLoading, Verbose, TEXT("Phase 6: Warmup - Level already loaded: %s"),
					*StreamingLevel->GetWorldAssetPackageName());
				continue;
			}

			// Request level load without making it visible
			// This loads the level into memory but doesn't activate it
			StreamingLevel->SetShouldBeLoaded(true);
			StreamingLevel->SetShouldBeVisible(false);
			StreamingLevel->SetPriority(0); // Low priority - background loading

			LevelsCached++;
			UE_LOG(LogProjectLoading, Verbose, TEXT("Phase 6: Warmup - Queued level for pre-cache: %s"),
				*StreamingLevel->GetWorldAssetPackageName());
		}

		if (LevelsCached > 0)
		{
			UE_LOG(LogProjectLoading, Verbose, TEXT("Phase 6: Warmup - Pre-cached %d streaming level(s)"), LevelsCached);
			ReportProgress(Context, 0.95f, FText::Format(
				LOCTEXT("LevelStreamingPrecached", "Pre-cached {0} streaming level(s)"),
				FText::AsNumber(LevelsCached)
			));
		}
	}

	ReportProgress(Context, 1.0f, LOCTEXT("WarmupComplete", "Warmup complete"));
	UE_LOG(LogProjectLoading, Display, TEXT("Phase 6: Warmup - Completed successfully"));

	return FProjectPhaseResult::Success();
}

#undef LOCTEXT_NAMESPACE

