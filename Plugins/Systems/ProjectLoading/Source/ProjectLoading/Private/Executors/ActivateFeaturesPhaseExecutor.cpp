// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ActivateFeaturesPhaseExecutor.h"
#include "Interfaces/IOrchestratorRegistry.h"
#include "ProjectLoadingLog.h"

#define LOCTEXT_NAMESPACE "ProjectLoadPhaseExecutors"

FText FActivateFeaturesPhaseExecutor::GetPhaseName() const
{
	return LOCTEXT("ActivateFeaturesPhase", "Activate Features");
}

bool FActivateFeaturesPhaseExecutor::ShouldSkip(const FLoadRequest& Request) const
{
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

	IOrchestratorRegistry* Registry = GetOrchestratorRegistry();
	if (!Registry)
	{
		const FText Error = LOCTEXT("NoOrchestratorRegistry", "Feature readiness authority unavailable.");
		UE_LOG(LogProjectLoading, Error, TEXT("Phase 4: Activate Features - %s"), *Error.ToString());
		return FProjectPhaseResult::Failure(Error, ProjectLoadingErrors::FeatureActivationFailed);
	}

	TArray<FName> RequestedFeatures;
	for (const FString& FeatureIdString : Context.Request.FeaturesToActivate)
	{
		if (!FeatureIdString.IsEmpty())
		{
			RequestedFeatures.AddUnique(FName(*FeatureIdString));
		}
	}

	if (RequestedFeatures.Num() == 0)
	{
		UE_LOG(LogProjectLoading, Verbose, TEXT("Phase 4: Activate Features - No features to validate"));
		ReportProgress(Context, 1.0f, LOCTEXT("NoFeaturesActivate", "No features to validate"));
		return FProjectPhaseResult::Skipped();
	}

	TArray<FString> MissingFeatures;

	for (int32 Index = 0; Index < RequestedFeatures.Num(); ++Index)
	{
		const FName& FeatureName = RequestedFeatures[Index];
		const float Progress = static_cast<float>(Index + 1) / static_cast<float>(RequestedFeatures.Num());

		ReportProgress(Context, Progress, FText::Format(
			LOCTEXT("CheckingFeature", "Checking feature: {0}"),
			FText::FromName(FeatureName)));

		if (!Registry->IsFeatureAvailable(FeatureName))
		{
			UE_LOG(LogProjectLoading, Error, TEXT("Phase 4: Activate Features - Required feature unavailable: %s"), *FeatureName.ToString());
			MissingFeatures.Add(FeatureName.ToString());
		}
		else
		{
			UE_LOG(LogProjectLoading, Verbose, TEXT("Phase 4: Activate Features - Validated: %s"), *FeatureName.ToString());
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
			LOCTEXT("MissingFeatures", "Required features unavailable: {0}"),
			FText::FromString(MissingList));

		UE_LOG(LogProjectLoading, Error, TEXT("Phase 4: Activate Features - %s"), *Error.ToString());
		return FProjectPhaseResult::Failure(Error, ProjectLoadingErrors::FeatureActivationFailed);
	}

	ReportProgress(Context, 1.0f, LOCTEXT("ActivateFeaturesComplete", "Feature validation complete"));
	UE_LOG(LogProjectLoading, Display, TEXT("Phase 4: Activate Features - Completed successfully (%d feature(s))"), RequestedFeatures.Num());

	return FProjectPhaseResult::Success();
}

#undef LOCTEXT_NAMESPACE
