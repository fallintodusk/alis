// Copyright ALIS. All Rights Reserved.

#include "PreloadCriticalAssetsPhaseExecutor.h"
#include "ProjectLoadingLog.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Algo/Unique.h"
#include "UObject/UObjectGlobals.h" // For ProcessAsyncLoading

#define LOCTEXT_NAMESPACE "ProjectLoadPhaseExecutors"

FText FPreloadCriticalAssetsPhaseExecutor::GetPhaseName() const
{
	return LOCTEXT("PreloadCriticalAssetsPhase", "Preload Critical Assets");
}

FProjectPhaseResult FPreloadCriticalAssetsPhaseExecutor::Execute(FProjectPhaseContext& Context)
{
	UE_LOG(LogProjectLoading, Display, TEXT("Phase 3: Preload Critical Assets - Starting"));
	ReportProgress(Context, 0.0f, LOCTEXT("PreloadStart", "Preloading critical assets..."));

	// Check cancellation
	if (CheckCancellation(Context))
	{
		UE_LOG(LogProjectLoading, Warning, TEXT("Phase 3: Preload Critical Assets - Cancelled"));
		return FProjectPhaseResult::Cancelled();
	}

	UAssetManager& AssetManager = UAssetManager::Get();
	FStreamableManager& StreamableManager = AssetManager.GetStreamableManager();

	// Build list of assets to preload
	TArray<FSoftObjectPath> AssetsToLoad;

	// Log input state
	UE_LOG(LogProjectLoading, Display, TEXT("Phase 3: Input - MapAssetId=%s (Valid=%s)"),
		*Context.Request.MapAssetId.ToString(),
		Context.Request.MapAssetId.IsValid() ? TEXT("YES") : TEXT("NO"));
	UE_LOG(LogProjectLoading, Display, TEXT("Phase 3: Input - MapSoftPath=%s"),
		*Context.Request.MapSoftPath.ToString());
	UE_LOG(LogProjectLoading, Display, TEXT("Phase 3: Input - ExperienceAssetId=%s (Valid=%s)"),
		*Context.Request.ExperienceAssetId.ToString(),
		Context.Request.ExperienceAssetId.IsValid() ? TEXT("YES") : TEXT("NO"));

	// NOTE: Do NOT preload map (UWorld) here - map loading must happen in Travel phase via ServerTravel.
	// StreamableManager cannot properly load UWorld assets (requires World Partition handling, level streaming, etc.)
	// Phase 3 is for preloading OTHER critical assets: meshes, textures, materials, data assets.
	if (Context.Request.MapAssetId.IsValid())
	{
		UE_LOG(LogProjectLoading, Display, TEXT("Phase 3: MapAssetId=%s is valid - will load during Travel phase"),
			*Context.Request.MapAssetId.ToString());
	}
	else
	{
		UE_LOG(LogProjectLoading, Warning, TEXT("Phase 3: MapAssetId is INVALID - Travel will use MapSoftPath fallback"));
	}

	// Load CriticalAssetIds (meshes, textures, materials, data assets - NOT map)
	UE_LOG(LogProjectLoading, Display, TEXT("Phase 3: CriticalAssetIds count = %d"), Context.Request.CriticalAssetIds.Num());
	for (const FPrimaryAssetId& AssetId : Context.Request.CriticalAssetIds)
	{
		FAssetData AssetData;
		if (AssetManager.GetPrimaryAssetData(AssetId, AssetData))
		{
			AssetsToLoad.Add(AssetData.ToSoftObjectPath());
			UE_LOG(LogProjectLoading, Display, TEXT("Phase 3: QUEUED critical asset: %s"), *AssetData.GetSoftObjectPath().ToString());
		}
		else
		{
			UE_LOG(LogProjectLoading, Warning, TEXT("Phase 3: GetPrimaryAssetData FAILED for CriticalAssetId=%s"), *AssetId.ToString());
		}
	}

	// Load CriticalSoftPaths (auto-discovered shared assets - filtered to ~20 via reference counting)
	UE_LOG(LogProjectLoading, Display, TEXT("Phase 3: CriticalSoftPaths count = %d"), Context.Request.CriticalSoftPaths.Num());
	for (const FSoftObjectPath& SoftPath : Context.Request.CriticalSoftPaths)
	{
		AssetsToLoad.Add(SoftPath);
	}
	if (Context.Request.CriticalSoftPaths.Num() > 0)
	{
		UE_LOG(LogProjectLoading, Display, TEXT("Phase 3: QUEUED %d shared assets from CriticalSoftPaths"), Context.Request.CriticalSoftPaths.Num());
	}

	// Add experience manifest asset if present
	if (Context.Request.ExperienceAssetId.IsValid())
	{
		FAssetData ExperienceAssetData;
		if (AssetManager.GetPrimaryAssetData(Context.Request.ExperienceAssetId, ExperienceAssetData))
		{
			const FSoftObjectPath ExpPath = ExperienceAssetData.ToSoftObjectPath();
			AssetsToLoad.Add(ExpPath);
			UE_LOG(LogProjectLoading, Display, TEXT("Phase 3: QUEUED experience asset: %s"), *ExpPath.ToString());
		}
		else
		{
			UE_LOG(LogProjectLoading, Warning, TEXT("Phase 3: GetPrimaryAssetData FAILED for ExperienceAssetId=%s"),
				*Context.Request.ExperienceAssetId.ToString());
		}
	}

	// Deduplicate preload list (Epic's recommended pattern - avoids accidental growth from redirects/duplicates)
	const int32 PreDedupeCount = AssetsToLoad.Num();
	AssetsToLoad.Sort([](const FSoftObjectPath& A, const FSoftObjectPath& B) { return A.ToString() < B.ToString(); });
	AssetsToLoad.SetNum(Algo::Unique(AssetsToLoad));
	const int32 DupesRemoved = PreDedupeCount - AssetsToLoad.Num();
	if (DupesRemoved > 0)
	{
		UE_LOG(LogProjectLoading, Display, TEXT("Phase 3: Deduplicated preload list: %d -> %d assets (%d duplicates removed)"),
			PreDedupeCount, AssetsToLoad.Num(), DupesRemoved);
	}

	// If no assets to load, skip phase
	if (AssetsToLoad.Num() == 0)
	{
		UE_LOG(LogProjectLoading, Warning, TEXT("Phase 3: Preload Critical Assets - NO ASSETS TO PRELOAD"));
		UE_LOG(LogProjectLoading, Warning, TEXT("Phase 3: This means map will load during Travel phase (causes freeze)"));
		ReportProgress(Context, 1.0f, LOCTEXT("PreloadSkipped", "No assets to preload"));
		return FProjectPhaseResult::Skipped();
	}

	ReportProgress(Context, 0.25f, FText::Format(
		LOCTEXT("PreloadQueued", "Preloading {0} asset(s)..."),
		FText::AsNumber(AssetsToLoad.Num())
	));

	UE_LOG(LogProjectLoading, Display, TEXT("Phase 3: Preload Critical Assets - Loading %d asset(s)"), AssetsToLoad.Num());

	// Log each asset being requested (for debugging stuck loads)
	for (int32 i = 0; i < AssetsToLoad.Num(); ++i)
	{
		UE_LOG(LogProjectLoading, Display, TEXT("Phase 3: [%d/%d] Requesting: %s"),
			i + 1, AssetsToLoad.Num(), *AssetsToLoad[i].ToString());
	}

	// Start async loading
	// Use bManageActiveHandle=true so we control handle lifetime
	// Store handle in Context.Runtime.PreloadHandles to keep assets pinned through Travel
	// Without this, assets become unloadable before Travel even starts!
	TSharedPtr<FStreamableHandle> Handle = StreamableManager.RequestAsyncLoad(
		AssetsToLoad,
		FStreamableDelegate(),
		FStreamableManager::AsyncLoadHighPriority,
		true,  // Manage active handle - we control lifetime via Context.Runtime
		false, // Don't start stalled
		TEXT("ProjectLoading_CriticalPreload")
	);

	if (!Handle.IsValid())
	{
		const FText Error = LOCTEXT("PreloadFailed", "Failed to start asset preload");
		UE_LOG(LogProjectLoading, Error, TEXT("Phase 3: Preload Critical Assets - %s"), *Error.ToString());
		return FProjectPhaseResult::Failure(Error, ProjectLoadingErrors::PreloadFailed);
	}

	// Wait for loading with Slate pumping so loading widget can update!
	// CRITICAL: We pump FSlateApplication each iteration to let the loading screen render.
	// Without this, the widget would freeze because the game thread is blocked.
	const double StartTime = FPlatformTime::Seconds();
	const double TimeoutSeconds = 60.0;
	float LastLoggedProgress = -1.0f;

	// Log whether frame pump callback is bound (critical for UI updates)
	const bool bHasPumpCallback = Context.OnPumpFrame != nullptr;
	UE_LOG(LogProjectLoading, Display, TEXT("Phase 3: Waiting for async load - OnPumpFrame callback %s"),
		bHasPumpCallback ? TEXT("BOUND (UI will animate)") : TEXT("NOT BOUND (UI will freeze!)"));

	while (!Handle->HasLoadCompleted())
	{
		const double ElapsedTime = FPlatformTime::Seconds() - StartTime;

		// Check timeout
		if (ElapsedTime > TimeoutSeconds)
		{
			const FText Error = LOCTEXT("PreloadTimeout", "Asset preload timed out");
			UE_LOG(LogProjectLoading, Error, TEXT("Phase 3: Preload Critical Assets - Timeout after %.1f seconds"), ElapsedTime);

			// Log which assets are stuck
			UE_LOG(LogProjectLoading, Error, TEXT("Phase 3: Asset status at timeout:"));
			for (const FSoftObjectPath& AssetPath : AssetsToLoad)
			{
				const bool bComplete = StreamableManager.IsAsyncLoadComplete(AssetPath);
				UE_LOG(LogProjectLoading, Error, TEXT("  [%s] %s"),
					bComplete ? TEXT("DONE") : TEXT("STUCK"),
					*AssetPath.ToString());
			}

			Handle->CancelHandle();
			Handle->ReleaseHandle();
			return FProjectPhaseResult::Failure(Error, ProjectLoadingErrors::Timeout);
		}

		// Check cancellation
		if (CheckCancellation(Context))
		{
			UE_LOG(LogProjectLoading, Warning, TEXT("Phase 3: Preload Critical Assets - Cancelled during loading"));
			Handle->CancelHandle();
			Handle->ReleaseHandle();
			return FProjectPhaseResult::Cancelled();
		}

		// Process async loading to allow IO and callbacks to complete
		// NOTE: Don't use ProcessThreadUntilIdle - causes recursion when already in a task
		// ProcessAsyncLoading() is the proper way to tick the async loading system
		// Args: bUseTimeLimit, bUseFullTimeLimit, TimeLimit (seconds)
		ProcessAsyncLoading(true, false, 0.016);

		// CRITICAL: Pump frame to let loading widget update!
		// This delegate is injected by UI layer - Systems doesn't depend on Slate.
		if (Context.OnPumpFrame)
		{
			Context.OnPumpFrame();
		}

		// Report progress (25% base + 50% for loading progress)
		const float LoadProgress = Handle->GetProgress();
		const float OverallProgress = 0.25f + (LoadProgress * 0.5f);
		ReportProgress(Context, OverallProgress, FText::Format(
			LOCTEXT("PreloadProgress", "Loading assets... {0}%"),
			FText::AsNumber(FMath::RoundToInt(LoadProgress * 100.0f))
		));

		// Log progress every 10% change
		if (FMath::Abs(LoadProgress - LastLoggedProgress) >= 0.1f)
		{
			UE_LOG(LogProjectLoading, Display, TEXT("Phase 3: Async load progress: %.0f%% (elapsed: %.1fs)"),
				LoadProgress * 100.0f, ElapsedTime);
			LastLoggedProgress = LoadProgress;
		}

		// Small delay to avoid busy spin (but Slate pump above is the key)
		FPlatformProcess::Sleep(0.016f); // ~60 FPS
	}

	// Loading complete - get results
	TArray<UObject*> LoadedAssets;
	Handle->GetLoadedAssets(LoadedAssets);

	const double LoadDuration = FPlatformTime::Seconds() - StartTime;

	// Store handle in Runtime to keep assets pinned through Travel + Warmup phases
	// Handle will be released in Warmup phase after WP streaming has started
	Context.Runtime.PreloadHandles.Add(Handle);

	UE_LOG(LogProjectLoading, Display, TEXT("Phase 3: Preload Critical Assets - COMPLETED in %.2fs"), LoadDuration);
	UE_LOG(LogProjectLoading, Display, TEXT("Phase 3: Successfully preloaded %d asset(s):"), LoadedAssets.Num());
	for (UObject* Asset : LoadedAssets)
	{
		if (Asset)
		{
			UE_LOG(LogProjectLoading, Display, TEXT("  - %s (%s)"),
				*Asset->GetName(), *Asset->GetClass()->GetName());
		}
	}
	UE_LOG(LogProjectLoading, Display, TEXT("Phase 3: Stored preload handle (total handles: %d) - will release in Warmup phase"),
		Context.Runtime.PreloadHandles.Num());

	ReportProgress(Context, 1.0f, LOCTEXT("PreloadComplete", "Asset preload complete"));
	return FProjectPhaseResult::Success();
}

#undef LOCTEXT_NAMESPACE

