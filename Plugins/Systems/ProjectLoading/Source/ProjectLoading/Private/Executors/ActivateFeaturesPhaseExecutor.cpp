// Copyright ALIS. All Rights Reserved.

#include "ActivateFeaturesPhaseExecutor.h"
#include "ProjectLoadingLog.h"
#include "ProjectLoadingSubsystem.h"
#include "ProjectLoadingSettings.h"
#include "Modules/ModuleManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "ProjectLoadPhaseExecutors"

FText FActivateFeaturesPhaseExecutor::GetPhaseName() const
{
	return LOCTEXT("ActivateFeaturesPhase", "Activate Features");
}

bool FActivateFeaturesPhaseExecutor::ShouldSkip(const FLoadRequest& Request) const
{
	// Check if feature activation is delegated to Orchestrator
	const UProjectLoadingSettings* Settings = GetDefault<UProjectLoadingSettings>();
	if (Settings && Settings->bDelegateFeatureActivationToOrchestrator)
	{
		// Orchestrator handles feature activation in the immutable bootloader path
		return true;
	}

	// Legacy path: Skip if no features to activate
	return Request.FeaturesToActivate.Num() == 0;
}

FProjectPhaseResult FActivateFeaturesPhaseExecutor::Execute(FProjectPhaseContext& Context)
{
	UE_LOG(LogProjectLoading, Display, TEXT("Phase 4: Activate Features - Starting"));
	ReportProgress(Context, 0.0f, LOCTEXT("ActivateFeaturesStart", "Validating feature modules..."));

	if (CheckCancellation(Context))
	{
		UE_LOG(LogProjectLoading, Warning, TEXT("Phase 4: Activate Features - Cancelled"));
		return FProjectPhaseResult::Cancelled();
	}

	UProjectLoadingSubsystem* LoadingSubsystem = Context.Subsystem.Get();
	if (!LoadingSubsystem)
	{
		const FText Error = LOCTEXT("NoLoadingSubsystem", "Project loading subsystem unavailable.");
		UE_LOG(LogProjectLoading, Error, TEXT("Phase 4: Activate Features - %s"), *Error.ToString());
		return FProjectPhaseResult::Failure(Error, ProjectLoadingErrors::FeatureActivationFailed);
	}

	// Convert feature IDs to module names (assume feature ID == plugin name == module name for now)
	TArray<FName> FeatureModules;
	for (const FString& FeatureIdString : Context.Request.FeaturesToActivate)
	{
		if (!FeatureIdString.IsEmpty())
		{
			FeatureModules.AddUnique(FName(*FeatureIdString));
		}
	}

	if (FeatureModules.Num() == 0)
	{
		UE_LOG(LogProjectLoading, Verbose, TEXT("Phase 4: Activate Features - No features to validate"));
		ReportProgress(Context, 1.0f, LOCTEXT("NoFeaturesActivate", "No features to validate"));
		return FProjectPhaseResult::Skipped();
	}

	// Validate that required feature modules are loaded (Orchestrator guarantees ordering and load)
	FModuleManager& ModuleManager = FModuleManager::Get();
	TArray<FString> MissingFeatures;

	for (int32 Index = 0; Index < FeatureModules.Num(); ++Index)
	{
		const FName& ModuleName = FeatureModules[Index];
		const float Progress = (float)(Index + 1) / (float)FeatureModules.Num();

		ReportProgress(Context, Progress, FText::Format(
			LOCTEXT("CheckingFeature", "Checking feature: {0}"),
			FText::FromName(ModuleName)));

		if (!ModuleManager.IsModuleLoaded(ModuleName))
		{
			UE_LOG(LogProjectLoading, Error, TEXT("Phase 4: Activate Features - Required feature not loaded: %s"), *ModuleName.ToString());
			MissingFeatures.Add(ModuleName.ToString());
		}
		else
		{
			UE_LOG(LogProjectLoading, Verbose, TEXT("Phase 4: Activate Features - Validated: %s"), *ModuleName.ToString());
		}

		if (CheckCancellation(Context))
		{
			UE_LOG(LogProjectLoading, Warning, TEXT("Phase 4: Activate Features - Cancelled"));
			return FProjectPhaseResult::Cancelled();
		}
	}

	if (MissingFeatures.Num() > 0)
	{
		FString MissingList = FString::Join(MissingFeatures, TEXT(", "));
		const FText Error = FText::Format(
			LOCTEXT("MissingFeatures", "Required feature modules not loaded: {0}"),
			FText::FromString(MissingList));

		UE_LOG(LogProjectLoading, Error, TEXT("Phase 4: Activate Features - %s"), *Error.ToString());
		return FProjectPhaseResult::Failure(Error, ProjectLoadingErrors::FeatureActivationFailed);
	}

	ReportProgress(Context, 1.0f, LOCTEXT("ActivateFeaturesComplete", "Feature validation complete"));
	UE_LOG(LogProjectLoading, Display, TEXT("Phase 4: Activate Features - Completed successfully (%d feature(s))"), FeatureModules.Num());

	return FProjectPhaseResult::Success();
}

#undef LOCTEXT_NAMESPACE

