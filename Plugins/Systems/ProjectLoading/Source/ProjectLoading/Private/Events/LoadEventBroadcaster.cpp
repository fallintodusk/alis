// Copyright ALIS. All Rights Reserved.

#include "Events/LoadEventBroadcaster.h"
#include "ProjectLoadingLog.h"
#include "ProjectLoadingSubsystem.h"

void FLoadEventBroadcaster::Initialize(
	void* InOnLoadStarted,
	void* InOnProgress,
	void* InOnPhaseChanged,
	void* InOnCompleted,
	void* InOnFailed)
{
	OnLoadStarted = InOnLoadStarted;
	OnProgress = InOnProgress;
	OnPhaseChanged = InOnPhaseChanged;
	OnCompleted = InOnCompleted;
	OnFailed = InOnFailed;

	UE_LOG(LogProjectLoading, Verbose, TEXT("LoadEventBroadcaster: Initialized with delegate bindings"));
}

void FLoadEventBroadcaster::BroadcastLoadStarted(const FLoadRequest& Request)
{
	UE_LOG(LogProjectLoading, Verbose, TEXT("LoadEventBroadcaster: LoadStarted for %s"),
		*Request.ToString());

	EnsureGameThread([this, Request]()
	{
		if (OnLoadStarted)
		{
			auto* Delegate = static_cast<FProjectLoadStartedSignature*>(OnLoadStarted);
			if (Delegate->IsBound())
			{
				Delegate->Broadcast(Request);
			}
		}
	});
}

void FLoadEventBroadcaster::BroadcastProgress(float NormalizedProgress)
{
	// Only log significant progress changes to avoid spam (10% intervals)
	if (FMath::Abs(NormalizedProgress - LastLoggedProgress) >= 0.1f)
	{
		UE_LOG(LogProjectLoading, Verbose, TEXT("LoadEventBroadcaster: Progress %.1f%%"),
			NormalizedProgress * 100.0f);
		LastLoggedProgress = NormalizedProgress;
	}

	EnsureGameThread([this, NormalizedProgress]()
	{
		if (OnProgress)
		{
			auto* Delegate = static_cast<FProjectLoadProgressSignature*>(OnProgress);
			if (Delegate->IsBound())
			{
				Delegate->Broadcast(NormalizedProgress);
			}
		}
	});
}

void FLoadEventBroadcaster::BroadcastPhaseChanged(const FLoadPhaseState& PhaseState, float OverallProgress)
{
	UE_LOG(LogProjectLoading, Verbose, TEXT("LoadEventBroadcaster: PhaseChanged %s (State=%s, Progress=%.1f%%, Overall=%.1f%%)"),
		*PhaseState.GetPhaseName(),
		*StaticEnum<EPhaseState>()->GetNameStringByValue(static_cast<int64>(PhaseState.State)),
		PhaseState.Progress * 100.0f,
		OverallProgress * 100.0f);

	EnsureGameThread([this, PhaseState, OverallProgress]()
	{
		if (OnPhaseChanged)
		{
			auto* Delegate = static_cast<FProjectLoadPhaseChangedSignature*>(OnPhaseChanged);
			if (Delegate->IsBound())
			{
				Delegate->Broadcast(PhaseState, OverallProgress);
			}
		}
	});
}

void FLoadEventBroadcaster::BroadcastCompleted(const FLoadRequest& Request, const FProjectLoadTelemetry& Telemetry)
{
	UE_LOG(LogProjectLoading, Display, TEXT("LoadEventBroadcaster: Completed for %s (Duration=%.1fs)"),
		*Request.ToString(),
		Telemetry.LoadEndTimeSeconds - Telemetry.LoadStartTimeSeconds);

	EnsureGameThread([this, Request, Telemetry]()
	{
		if (OnCompleted)
		{
			auto* Delegate = static_cast<FProjectLoadCompletedSignature*>(OnCompleted);
			if (Delegate->IsBound())
			{
				Delegate->Broadcast(Request, Telemetry);
			}
		}
	});
}

void FLoadEventBroadcaster::BroadcastFailed(const FLoadRequest& Request, const FText& ErrorMessage, int32 ErrorCode)
{
	UE_LOG(LogProjectLoading, Error, TEXT("LoadEventBroadcaster: Failed for %s (Error=%d: %s)"),
		*Request.ToString(),
		ErrorCode,
		*ErrorMessage.ToString());

	EnsureGameThread([this, Request, ErrorMessage, ErrorCode]()
	{
		if (OnFailed)
		{
			auto* Delegate = static_cast<FProjectLoadFailedSignature*>(OnFailed);
			if (Delegate->IsBound())
			{
				Delegate->Broadcast(Request, ErrorMessage, ErrorCode);
			}
		}
	});
}

void FLoadEventBroadcaster::Reset()
{
	LastLoggedProgress = -1.0f;
	UE_LOG(LogProjectLoading, Verbose, TEXT("LoadEventBroadcaster: Reset for new load"));
}
