// Copyright ALIS. All Rights Reserved.

#include "Pipeline/LoadProgressTracker.h"
#include "ProjectLoadingLog.h"
#include "ProjectLoadingSubsystem.h"

void FLoadProgressTracker::Reset(int32 InPhaseCount)
{
	PhaseStates.Reset();
	TrackingStartTime = FPlatformTime::Seconds();
	bProgressCacheDirty = true;
	CachedOverallProgress = 0.0f;

	UE_LOG(LogProjectLoading, Verbose, TEXT("ProgressTracker: Reset for %d phases"), InPhaseCount);
}

void FLoadProgressTracker::Reset(const TArray<ELoadPhase>& Phases)
{
	PhaseStates.Reset();
	PhaseStates.Reserve(Phases.Num());

	for (ELoadPhase Phase : Phases)
	{
		PhaseStates.Emplace(Phase);
	}

	TrackingStartTime = FPlatformTime::Seconds();
	bProgressCacheDirty = true;
	CachedOverallProgress = 0.0f;

	UE_LOG(LogProjectLoading, Verbose, TEXT("ProgressTracker: Reset with %d specific phases"), Phases.Num());
}

void FLoadProgressTracker::UpdatePhaseState(ELoadPhase Phase, EPhaseState State, float Progress, const FText& StatusMessage)
{
	// Find or create phase state
	FLoadPhaseState* PhaseState = GetPhaseStateMutable(Phase);
	if (!PhaseState)
	{
		PhaseStates.Emplace(Phase);
		PhaseState = &PhaseStates.Last();
	}

	// Update state
	PhaseState->State = State;
	PhaseState->Progress = FMath::Clamp(Progress, 0.0f, 1.0f);
	PhaseState->StatusMessage = StatusMessage;

	// Update timing
	if (State == EPhaseState::InProgress && PhaseState->StartTime <= 0.0)
	{
		PhaseState->StartTime = FPlatformTime::Seconds();
	}
	else if (State == EPhaseState::Completed || State == EPhaseState::Failed || State == EPhaseState::Skipped)
	{
		PhaseState->EndTime = FPlatformTime::Seconds();
	}

	// Invalidate progress cache
	bProgressCacheDirty = true;

	UE_LOG(LogProjectLoading, Verbose, TEXT("ProgressTracker: Phase %s -> %s (%.1f%%)"),
		*PhaseState->GetPhaseName(),
		*StaticEnum<EPhaseState>()->GetNameStringByValue(static_cast<int64>(State)),
		Progress * 100.0f);
}

const FLoadPhaseState* FLoadProgressTracker::GetPhaseState(ELoadPhase Phase) const
{
	return PhaseStates.FindByPredicate([Phase](const FLoadPhaseState& State)
	{
		return State.Phase == Phase;
	});
}

FLoadPhaseState* FLoadProgressTracker::GetPhaseStateMutable(ELoadPhase Phase)
{
	return PhaseStates.FindByPredicate([Phase](const FLoadPhaseState& State)
	{
		return State.Phase == Phase;
	});
}

float FLoadProgressTracker::CalculateOverallProgress() const
{
	// Return cached value if valid
	if (!bProgressCacheDirty)
	{
		return CachedOverallProgress;
	}

	// Avoid division by zero
	if (PhaseStates.Num() == 0)
	{
		CachedOverallProgress = 0.0f;
		bProgressCacheDirty = false;
		return CachedOverallProgress;
	}

	// Phase weight mapping (0-90% for pipeline, 90-100% reserved for completion)
	// Maps each phase to a specific progress range based on expected duration/importance
	auto GetPhaseWeightAndBase = [](ELoadPhase Phase) -> TPair<float, float>
	{
		switch (Phase)
		{
		case ELoadPhase::ResolveAssets:
			return TPair<float, float>(0.10f, 0.0f); // 0-10%

		case ELoadPhase::MountContent:
			return TPair<float, float>(0.20f, 0.10f); // 10-30%

		case ELoadPhase::PreloadCriticalAssets:
			return TPair<float, float>(0.50f, 0.30f); // 30-80% (heavy phase with asset loading)

		case ELoadPhase::ActivateFeatures:
			return TPair<float, float>(0.05f, 0.80f); // 80-85% (GameFeature activation)

		case ELoadPhase::Travel:
			return TPair<float, float>(0.05f, 0.85f); // 85-90% (static frame during ServerTravel)

		case ELoadPhase::Warmup:
			return TPair<float, float>(0.0f, 0.90f); // Warmup comes after travel (rarely used)

		case ELoadPhase::Complete:
			return TPair<float, float>(0.10f, 0.90f); // 90-100%

		default:
			return TPair<float, float>(0.0f, 0.0f); // Unknown phases contribute nothing
		}
	};

	// Calculate weighted progress contribution from each phase
	float AccumulatedProgress = 0.0f;
	for (const FLoadPhaseState& State : PhaseStates)
	{
		TPair<float, float> WeightAndBase = GetPhaseWeightAndBase(State.Phase);
		const float PhaseWeight = WeightAndBase.Key;
		const float PhaseBase = WeightAndBase.Value;

		switch (State.State)
		{
		case EPhaseState::Completed:
		case EPhaseState::Skipped:
			// Phase is done - contributes full weight
			AccumulatedProgress = FMath::Max(AccumulatedProgress, PhaseBase + PhaseWeight);
			break;

		case EPhaseState::InProgress:
			{
				// Phase is running - contributes proportional to progress within its range
				const float PhaseProgress = FMath::Clamp(State.Progress, 0.0f, 1.0f);
				AccumulatedProgress = FMath::Max(AccumulatedProgress, PhaseBase + (PhaseWeight * PhaseProgress));
				break;
			}

		default:
			// Pending or failed phases: use base progress from previous completed phases
			break;
		}
	}

	CachedOverallProgress = FMath::Clamp(AccumulatedProgress, 0.0f, 1.0f);
	bProgressCacheDirty = false;

	return CachedOverallProgress;
}

void FLoadProgressTracker::StartPhaseTiming(ELoadPhase Phase)
{
	if (FLoadPhaseState* PhaseState = GetPhaseStateMutable(Phase))
	{
		PhaseState->StartTime = FPlatformTime::Seconds();
		UE_LOG(LogProjectLoading, Verbose, TEXT("ProgressTracker: Started timing for phase %s at %.3fs"),
			*PhaseState->GetPhaseName(), PhaseState->StartTime);
	}
}

void FLoadProgressTracker::StopPhaseTiming(ELoadPhase Phase)
{
	if (FLoadPhaseState* PhaseState = GetPhaseStateMutable(Phase))
	{
		PhaseState->EndTime = FPlatformTime::Seconds();
		const double Duration = PhaseState->GetDuration();
		UE_LOG(LogProjectLoading, Display, TEXT("ProgressTracker: Phase %s completed in %.3fs"),
			*PhaseState->GetPhaseName(), Duration);
	}
}

double FLoadProgressTracker::GetPhaseDuration(ELoadPhase Phase) const
{
	if (const FLoadPhaseState* PhaseState = GetPhaseState(Phase))
	{
		return PhaseState->GetDuration();
	}
	return 0.0;
}

double FLoadProgressTracker::GetTotalElapsedTime() const
{
	return FPlatformTime::Seconds() - TrackingStartTime;
}

void FLoadProgressTracker::FillTelemetry(FProjectLoadTelemetry& OutTelemetry) const
{
	OutTelemetry.LoadStartTimeSeconds = TrackingStartTime;
	OutTelemetry.LoadEndTimeSeconds = FPlatformTime::Seconds();
	OutTelemetry.TotalProgress = CalculateOverallProgress();
}

bool FLoadProgressTracker::AreAllPhasesComplete() const
{
	for (const FLoadPhaseState& State : PhaseStates)
	{
		if (!State.IsComplete())
		{
			return false;
		}
	}
	return PhaseStates.Num() > 0;
}

bool FLoadProgressTracker::HasAnyPhaseFailed() const
{
	for (const FLoadPhaseState& State : PhaseStates)
	{
		if (State.IsFailed())
		{
			return true;
		}
	}
	return false;
}

ELoadPhase FLoadProgressTracker::GetCurrentActivePhase() const
{
	for (const FLoadPhaseState& State : PhaseStates)
	{
		if (State.IsInProgress())
		{
			return State.Phase;
		}
	}
	return ELoadPhase::None;
}

FString FLoadProgressTracker::ToDebugString() const
{
	TStringBuilder<1024> Builder;
	Builder.Appendf(TEXT("ProgressTracker: Overall=%.1f%% Phases=%d Elapsed=%.2fs\n"),
		CalculateOverallProgress() * 100.0f,
		PhaseStates.Num(),
		GetTotalElapsedTime());

	for (const FLoadPhaseState& State : PhaseStates)
	{
		Builder.Appendf(TEXT("  %s: %s %.1f%% (%.3fs)\n"),
			*State.GetPhaseName(),
			*StaticEnum<EPhaseState>()->GetNameStringByValue(static_cast<int64>(State.State)),
			State.Progress * 100.0f,
			State.GetDuration());
	}

	return Builder.ToString();
}
