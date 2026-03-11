// Copyright ALIS. All Rights Reserved.

#include "ResolveAssetsPhaseExecutor.h"
#include "ProjectLoading.h"
#include "ProjectLoadingLog.h"
#include "ProjectLoadingSubsystem.h"
#include "Engine/AssetManager.h"

#define LOCTEXT_NAMESPACE "ProjectLoadPhaseExecutors"

FText FResolveAssetsPhaseExecutor::GetPhaseName() const
{
	return LOCTEXT("ResolveAssetsPhase", "Resolve Assets");
}

FProjectPhaseResult FResolveAssetsPhaseExecutor::Execute(FProjectPhaseContext& Context)
{
	UE_LOG(LogProjectLoading, Display, TEXT("Phase 1: Resolve Assets - Starting"));
	ReportProgress(Context, 0.0f, LOCTEXT("ResolveAssetsStart", "Resolving asset references..."));

	// Check cancellation before starting (check both context flag and subsystem token)
	UProjectLoadingSubsystem* Subsystem = Context.Subsystem.Get();
	if (CheckCancellation(Context) || (Subsystem && !Subsystem->IsLoadInProgress()))
	{
		UE_LOG(LogProjectLoading, Warning, TEXT("Phase 1: Resolve Assets - Cancelled before execution"));
		return FProjectPhaseResult::Cancelled();
	}

	UAssetManager& AssetManager = UAssetManager::Get();

	// Step 1: Check map validity - either MapAssetId OR MapSoftPath required
	bool bHasValidAssetId = Context.Request.MapAssetId.IsValid();
	const bool bHasValidSoftPath = !Context.Request.MapSoftPath.IsNull();

	// Neither set - fail
	if (!bHasValidAssetId && !bHasValidSoftPath)
	{
		const FText Error = LOCTEXT("NoMapSpecified", "No map specified (neither MapAssetId nor MapSoftPath set)");
		UE_LOG(LogProjectLoading, Error, TEXT("Phase 1: Resolve Assets - %s"), *Error.ToString());
		UE_LOG(LogProjectLoading, Error, TEXT("  Diagnostic: Ensure MapAssetId or MapSoftPath is set in FLoadRequest before calling StartLoad()"));
		return FProjectPhaseResult::Failure(Error, ProjectLoadingErrors::AssetResolutionFailed);
	}

	// Soft-path-only mode: try to resolve a PrimaryAssetId from the soft path before falling back
	if (!bHasValidAssetId && bHasValidSoftPath)
	{
		UE_LOG(LogProjectLoading, Warning, TEXT("Phase 1: Resolve Assets - MapAssetId invalid, attempting Asset Manager resolution from soft path"));
		UE_LOG(LogProjectLoading, Display, TEXT("  MapSoftPath: %s"), *Context.Request.MapSoftPath.ToString());
		UE_LOG(LogProjectLoading, Display, TEXT("  Asset Manager initialized: %s"),
			AssetManager.IsInitialized() ? TEXT("Yes") : TEXT("No"));

		const FPrimaryAssetId ResolvedMapId = AssetManager.GetPrimaryAssetIdForPath(Context.Request.MapSoftPath);
		UE_LOG(LogProjectLoading, Display, TEXT("  GetPrimaryAssetIdForPath returned: %s (Valid=%s)"),
			ResolvedMapId.IsValid() ? *ResolvedMapId.ToString() : TEXT("<invalid>"),
			ResolvedMapId.IsValid() ? TEXT("true") : TEXT("false"));

		if (ResolvedMapId.IsValid())
		{
			Context.Request.MapAssetId = ResolvedMapId;
			bHasValidAssetId = true;
			UE_LOG(LogProjectLoading, Display, TEXT("  MapAssetId resolved from soft path, continuing with Asset Manager validation"));
		}
		else
		{
			// Diagnostic: enumerate known map primary assets (limited to avoid log spam)
			TArray<FPrimaryAssetId> MapAssets;
			AssetManager.GetPrimaryAssetIdList(FPrimaryAssetType(TEXT("Map")), MapAssets);
			UE_LOG(LogProjectLoading, Warning, TEXT("  Diagnostic: Asset Manager map entries: %d found"), MapAssets.Num());
			for (int32 Index = 0; Index < MapAssets.Num() && Index < 10; ++Index)
			{
				UE_LOG(LogProjectLoading, Warning, TEXT("    [%d] %s"), Index, *MapAssets[Index].ToString());
			}
			if (MapAssets.Num() > 10)
			{
				UE_LOG(LogProjectLoading, Warning, TEXT("    (...truncated, more maps registered - use Asset Manager window for full list)"));
			}
		}
	}

	// Soft-path fallback: no PrimaryAssetId available even after resolution attempt
	if (!bHasValidAssetId && bHasValidSoftPath)
	{
		UE_LOG(LogProjectLoading, Warning, TEXT("Phase 1: Resolve Assets - MapAssetId still invalid after resolution attempt, skipping Asset Manager validation"));
		UE_LOG(LogProjectLoading, Display, TEXT("  Using MapSoftPath fallback: %s"), *Context.Request.MapSoftPath.ToString());
		UE_LOG(LogProjectLoading, Display, TEXT("  Travel phase will use soft path directly"));

		ReportProgress(Context, 1.0f, LOCTEXT("ResolveAssetsSkippedSoftPath", "Using direct map path (soft-path mode)"));
		UE_LOG(LogProjectLoading, Display, TEXT("Phase 1: Resolve Assets - Completed (soft-path mode)"));
		return FProjectPhaseResult::Success();
	}

	ReportProgress(Context, 0.25f, LOCTEXT("ValidatingMapAsset", "Validating map asset..."));

	// Step 2: Normal path - verify map asset exists in Asset Manager
	FAssetData MapAssetData;
	if (!AssetManager.GetPrimaryAssetData(Context.Request.MapAssetId, MapAssetData))
	{
		// If we have a soft path fallback, log warning but succeed
		if (bHasValidSoftPath)
		{
			UE_LOG(LogProjectLoading, Warning, TEXT("Phase 1: Resolve Assets - MapAssetId '%s' not found in Asset Manager, will use MapSoftPath fallback"),
				*Context.Request.MapAssetId.ToString());
			UE_LOG(LogProjectLoading, Display, TEXT("  Using MapSoftPath: %s"), *Context.Request.MapSoftPath.ToString());

			ReportProgress(Context, 1.0f, LOCTEXT("ResolveAssetsFallbackSoftPath", "Using fallback map path"));
			UE_LOG(LogProjectLoading, Display, TEXT("Phase 1: Resolve Assets - Completed (fallback to soft-path)"));
			return FProjectPhaseResult::Success();
		}

		// No fallback - fail with detailed diagnostics
		const FText Error = FText::Format(
			LOCTEXT("MapAssetNotFound", "Map asset '{0}' not found in Asset Manager"),
			FText::FromString(Context.Request.MapAssetId.ToString())
		);
		UE_LOG(LogProjectLoading, Error, TEXT("Phase 1: Resolve Assets - %s"), *Error.ToString());

		// Detailed diagnostic logging for asset not found
		UE_LOG(LogProjectLoading, Error, TEXT("  Diagnostic: Requested PrimaryAssetId='%s'"), *Context.Request.MapAssetId.ToString());
		UE_LOG(LogProjectLoading, Error, TEXT("  Diagnostic: Asset Manager initialization complete: %s"),
			AssetManager.IsInitialized() ? TEXT("Yes") : TEXT("No"));

		// List available assets of the same type for debugging
		TArray<FPrimaryAssetId> AvailableAssets;
		AssetManager.GetPrimaryAssetIdList(Context.Request.MapAssetId.PrimaryAssetType, AvailableAssets);
		UE_LOG(LogProjectLoading, Error, TEXT("  Diagnostic: Available assets of type '%s': %d found"),
			*Context.Request.MapAssetId.PrimaryAssetType.ToString(), AvailableAssets.Num());

		if (AvailableAssets.Num() > 0 && AvailableAssets.Num() <= 10)
		{
			// List available assets if not too many
			for (const FPrimaryAssetId& AvailableAsset : AvailableAssets)
			{
				UE_LOG(LogProjectLoading, Error, TEXT("    - %s"), *AvailableAsset.ToString());
			}
		}
		else if (AvailableAssets.Num() > 10)
		{
			UE_LOG(LogProjectLoading, Error, TEXT("    (Too many to list - use Asset Manager to inspect)"));
		}
		else
		{
			UE_LOG(LogProjectLoading, Error, TEXT("    (No assets registered - check Asset Manager configuration)"));
		}

		UE_LOG(LogProjectLoading, Error, TEXT("  Diagnostic: Verify asset path in DefaultGame.ini [/Script/Engine.AssetManagerSettings]"));

		return FProjectPhaseResult::Failure(Error, ProjectLoadingErrors::AssetResolutionFailed);
	}

	UE_LOG(LogProjectLoading, Display, TEXT("Phase 1: Resolve Assets - Map asset found: %s"), *MapAssetData.AssetName.ToString());
	ReportProgress(Context, 0.5f, LOCTEXT("ResolvingExperience", "Resolving experience asset..."));

	// Check cancellation mid-phase
	if (CheckCancellation(Context))
	{
		UE_LOG(LogProjectLoading, Warning, TEXT("Phase 1: Resolve Assets - Cancelled during execution"));
		return FProjectPhaseResult::Cancelled();
	}

	// Step 3: Validate ExperienceAssetId (optional)
	if (Context.Request.ExperienceAssetId.IsValid())
	{
		FAssetData ExperienceAssetData;
		if (!AssetManager.GetPrimaryAssetData(Context.Request.ExperienceAssetId, ExperienceAssetData))
		{
			// Warning only - experience is optional, but add diagnostic context
			UE_LOG(LogProjectLoading, Warning, TEXT("Phase 1: Resolve Assets - Experience asset '%s' not found, proceeding without it"),
				*Context.Request.ExperienceAssetId.ToString());

			// Detailed diagnostic for experience asset resolution failure
			TArray<FPrimaryAssetId> AvailableExperiences;
			AssetManager.GetPrimaryAssetIdList(Context.Request.ExperienceAssetId.PrimaryAssetType, AvailableExperiences);

			UE_LOG(LogProjectLoading, Warning, TEXT("  Diagnostic: Available experience assets of type '%s': %d found"),
				*Context.Request.ExperienceAssetId.PrimaryAssetType.ToString(), AvailableExperiences.Num());

			if (AvailableExperiences.Num() > 0 && AvailableExperiences.Num() <= 10)
			{
				for (const FPrimaryAssetId& AvailableExp : AvailableExperiences)
				{
					UE_LOG(LogProjectLoading, Warning, TEXT("    - %s"), *AvailableExp.ToString());
				}
			}
		}
		else
		{
			UE_LOG(LogProjectLoading, Display, TEXT("Phase 1: Resolve Assets - Experience asset found: %s"), *ExperienceAssetData.AssetName.ToString());
		}
	}

	ReportProgress(Context, 0.75f, LOCTEXT("ValidatingFeatures", "Validating feature dependencies..."));

	// Step 4: Validate feature dependencies (informational for now)
	if (Context.Request.FeaturesToActivate.Num() > 0)
	{
		UE_LOG(LogProjectLoading, Display, TEXT("Phase 1: Resolve Assets - %d features to activate"), Context.Request.FeaturesToActivate.Num());
		for (const FString& FeatureId : Context.Request.FeaturesToActivate)
		{
			UE_LOG(LogProjectLoading, Verbose, TEXT("  - Feature: %s"), *FeatureId);
		}
	}

	// Final cancellation check
	if (CheckCancellation(Context))
	{
		UE_LOG(LogProjectLoading, Warning, TEXT("Phase 1: Resolve Assets - Cancelled before completion"));
		return FProjectPhaseResult::Cancelled();
	}

	ReportProgress(Context, 1.0f, LOCTEXT("ResolveAssetsComplete", "Asset resolution complete"));
	UE_LOG(LogProjectLoading, Display, TEXT("Phase 1: Resolve Assets - Completed successfully"));

	return FProjectPhaseResult::Success();
}

#undef LOCTEXT_NAMESPACE
