// Copyright ALIS. All Rights Reserved.

#include "Cancellation/LoadCancellationManager.h"
#include "ProjectLoadingLog.h"

FLoadCancellationManager::FLoadCancellationManager()
{
}

void FLoadCancellationManager::Reset()
{
	bCancellationRequested.store(false);
	bForceCancellation.store(false);
	CancellationTimeSeconds.store(0.0);

	{
		FScopeLock Lock(&ReasonLock);
		CancellationReason.Empty();
	}

	UE_LOG(LogProjectLoading, Verbose, TEXT("CancellationManager: Reset for new load operation"));
}

void FLoadCancellationManager::RequestCancellation(bool bForce)
{
	// Only process first cancellation request
	const bool bWasRequested = bCancellationRequested.exchange(true);
	if (bWasRequested)
	{
		UE_LOG(LogProjectLoading, Verbose, TEXT("CancellationManager: Cancellation already requested, ignoring duplicate"));
		return;
	}

	bForceCancellation.store(bForce);
	CancellationTimeSeconds.store(FPlatformTime::Seconds());

	{
		FScopeLock Lock(&ReasonLock);
		CancellationReason = bForce ? TEXT("Force cancellation requested") : TEXT("Graceful cancellation requested");
	}

	UE_LOG(LogProjectLoading, Warning, TEXT("CancellationManager: Cancellation requested (Force=%s)"),
		bForce ? TEXT("true") : TEXT("false"));

	// Dump stack trace to identify caller
	UE_LOG(LogProjectLoading, Verbose, TEXT("CancellationManager: Cancellation request stack trace:"));
	FDebug::DumpStackTraceToLog(ELogVerbosity::Verbose);
}

bool FLoadCancellationManager::IsCancellationRequested() const
{
	return bCancellationRequested.load();
}

bool FLoadCancellationManager::IsForceCancellation() const
{
	return bForceCancellation.load();
}

void FLoadCancellationManager::LogCancellationContext(const FString& CurrentPhase, float Progress, const FString& MapPath)
{
	UE_LOG(LogProjectLoading, Warning, TEXT("CancellationManager: Cancellation context:"));
	UE_LOG(LogProjectLoading, Warning, TEXT("  Phase: %s"), *CurrentPhase);
	UE_LOG(LogProjectLoading, Warning, TEXT("  Progress: %.1f%%"), Progress * 100.0f);
	UE_LOG(LogProjectLoading, Warning, TEXT("  Map: %s"), *MapPath);
	UE_LOG(LogProjectLoading, Warning, TEXT("  Force: %s"), IsForceCancellation() ? TEXT("true") : TEXT("false"));

	const double CancelTime = CancellationTimeSeconds.load();
	if (CancelTime > 0.0)
	{
		const double ElapsedSinceCancellation = FPlatformTime::Seconds() - CancelTime;
		UE_LOG(LogProjectLoading, Warning, TEXT("  Time since cancellation: %.2fs"), ElapsedSinceCancellation);
	}

	{
		FScopeLock Lock(&ReasonLock);
		if (!CancellationReason.IsEmpty())
		{
			UE_LOG(LogProjectLoading, Warning, TEXT("  Reason: %s"), *CancellationReason);
		}
	}
}

double FLoadCancellationManager::GetCancellationTime() const
{
	return CancellationTimeSeconds.load();
}

FString FLoadCancellationManager::GetCancellationReason() const
{
	FScopeLock Lock(&ReasonLock);
	return CancellationReason;
}
