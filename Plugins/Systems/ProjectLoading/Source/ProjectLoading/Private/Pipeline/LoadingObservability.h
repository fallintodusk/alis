// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Types/ProjectLoadPhaseState.h"

/**
 * Observability utilities for the loading pipeline.
 * Provides structured logging, telemetry, and tracing for debugging and monitoring.
 *
 * Design principles:
 * - Structured data over free-form text
 * - Correlation IDs for request tracking
 * - Phase-level granularity
 * - Zero overhead when disabled
 */

DECLARE_LOG_CATEGORY_EXTERN(LogLoadingPipeline, Log, All);

namespace LoadingObservability
{
	/**
	 * Trace event types for pipeline operations.
	 */
	enum class ETraceEvent : uint8
	{
		PipelineStarted,
		PipelineCompleted,
		PipelineFailed,
		PipelineCancelled,
		PhaseStarted,
		PhaseCompleted,
		PhaseFailed,
		PhaseSkipped,
		PhaseRetrying,
		ProgressUpdate,
		CancellationRequested
	};

	/**
	 * Convert trace event to string for logging.
	 */
	inline const TCHAR* TraceEventToString(ETraceEvent Event)
	{
		switch (Event)
		{
		case ETraceEvent::PipelineStarted:      return TEXT("PIPELINE_STARTED");
		case ETraceEvent::PipelineCompleted:    return TEXT("PIPELINE_COMPLETED");
		case ETraceEvent::PipelineFailed:       return TEXT("PIPELINE_FAILED");
		case ETraceEvent::PipelineCancelled:    return TEXT("PIPELINE_CANCELLED");
		case ETraceEvent::PhaseStarted:         return TEXT("PHASE_STARTED");
		case ETraceEvent::PhaseCompleted:       return TEXT("PHASE_COMPLETED");
		case ETraceEvent::PhaseFailed:          return TEXT("PHASE_FAILED");
		case ETraceEvent::PhaseSkipped:         return TEXT("PHASE_SKIPPED");
		case ETraceEvent::PhaseRetrying:        return TEXT("PHASE_RETRYING");
		case ETraceEvent::ProgressUpdate:       return TEXT("PROGRESS_UPDATE");
		case ETraceEvent::CancellationRequested: return TEXT("CANCELLATION_REQUESTED");
		default:                                return TEXT("UNKNOWN");
		}
	}

	/**
	 * Structured trace context for correlation.
	 */
	struct FTraceContext
	{
		/** Unique identifier for this load operation */
		FGuid LoadId;

		/** Experience/map being loaded */
		FString Target;

		/** Start time of the load operation */
		double StartTimeSeconds = 0.0;

		/** Generate new context */
		static FTraceContext Generate(const FString& InTarget)
		{
			FTraceContext Ctx;
			Ctx.LoadId = FGuid::NewGuid();
			Ctx.Target = InTarget;
			Ctx.StartTimeSeconds = FPlatformTime::Seconds();
			return Ctx;
		}

		/** Get elapsed time since start */
		double GetElapsedSeconds() const
		{
			return FPlatformTime::Seconds() - StartTimeSeconds;
		}

		/** Get short ID for logging (first 8 chars) */
		FString GetShortId() const
		{
			return LoadId.ToString(EGuidFormats::Short).Left(8);
		}
	};

	/**
	 * Structured phase timing data.
	 */
	struct FPhaseTiming
	{
		ELoadPhase Phase = ELoadPhase::None;
		double StartTimeSeconds = 0.0;
		double EndTimeSeconds = 0.0;
		int32 RetryCount = 0;
		bool bSuccess = false;
		bool bSkipped = false;
		int32 ErrorCode = 0;
		FString ErrorMessage;

		double GetDurationMs() const
		{
			return (EndTimeSeconds - StartTimeSeconds) * 1000.0;
		}
	};

	/**
	 * Pipeline telemetry data collected during execution.
	 */
	struct FPipelineTelemetry
	{
		FTraceContext Context;
		TArray<FPhaseTiming> PhaseTimings;
		double TotalDurationMs = 0.0;
		bool bSuccess = false;
		bool bCancelled = false;
		int32 TotalRetries = 0;
		FString FinalError;

		/** Add phase timing */
		void AddPhaseTiming(const FPhaseTiming& Timing)
		{
			PhaseTimings.Add(Timing);
			TotalRetries += Timing.RetryCount;
		}

		/** Get timing for specific phase */
		const FPhaseTiming* GetPhaseTiming(ELoadPhase Phase) const
		{
			return PhaseTimings.FindByPredicate([Phase](const FPhaseTiming& T) { return T.Phase == Phase; });
		}

		/** Generate summary string */
		FString ToSummaryString() const
		{
			TStringBuilder<1024> Builder;
			Builder.Appendf(TEXT("LoadId=%s Target=%s Duration=%.1fms Success=%s"),
				*Context.GetShortId(),
				*Context.Target,
				TotalDurationMs,
				bSuccess ? TEXT("true") : TEXT("false"));

			if (bCancelled)
			{
				Builder.Append(TEXT(" [CANCELLED]"));
			}

			if (TotalRetries > 0)
			{
				Builder.Appendf(TEXT(" Retries=%d"), TotalRetries);
			}

			if (!FinalError.IsEmpty())
			{
				Builder.Appendf(TEXT(" Error=%s"), *FinalError);
			}

			return Builder.ToString();
		}

		/** Generate detailed JSON-like string for structured logging */
		FString ToDetailedString() const
		{
			TStringBuilder<4096> Builder;
			Builder.Appendf(TEXT("{\"load_id\":\"%s\",\"target\":\"%s\",\"duration_ms\":%.1f,\"success\":%s,\"cancelled\":%s,\"total_retries\":%d"),
				*Context.LoadId.ToString(),
				*Context.Target,
				TotalDurationMs,
				bSuccess ? TEXT("true") : TEXT("false"),
				bCancelled ? TEXT("true") : TEXT("false"),
				TotalRetries);

			if (!FinalError.IsEmpty())
			{
				Builder.Appendf(TEXT(",\"error\":\"%s\""), *FinalError.ReplaceCharWithEscapedChar());
			}

			Builder.Append(TEXT(",\"phases\":["));

			for (int32 i = 0; i < PhaseTimings.Num(); ++i)
			{
				const FPhaseTiming& T = PhaseTimings[i];
				if (i > 0) Builder.Append(TEXT(","));

				Builder.Appendf(TEXT("{\"phase\":\"%s\",\"duration_ms\":%.1f,\"success\":%s,\"skipped\":%s,\"retries\":%d"),
					*StaticEnum<ELoadPhase>()->GetNameStringByValue(static_cast<int64>(T.Phase)),
					T.GetDurationMs(),
					T.bSuccess ? TEXT("true") : TEXT("false"),
					T.bSkipped ? TEXT("true") : TEXT("false"),
					T.RetryCount);

				if (!T.ErrorMessage.IsEmpty())
				{
					Builder.Appendf(TEXT(",\"error_code\":%d,\"error\":\"%s\""), T.ErrorCode, *T.ErrorMessage.ReplaceCharWithEscapedChar());
				}

				Builder.Append(TEXT("}"));
			}

			Builder.Append(TEXT("]}"));
			return Builder.ToString();
		}
	};

	/**
	 * Trace helper for structured logging.
	 */
	class FLoadingTracer
	{
	public:
		FLoadingTracer() = default;

		/** Start tracing a new load operation */
		void StartTrace(const FTraceContext& InContext)
		{
			Context = InContext;
			CurrentPhaseTiming = FPhaseTiming();
			Telemetry = FPipelineTelemetry();
			Telemetry.Context = Context;

			LogEvent(ETraceEvent::PipelineStarted, TEXT(""));
		}

		/** Begin phase execution */
		void BeginPhase(ELoadPhase Phase)
		{
			CurrentPhaseTiming = FPhaseTiming();
			CurrentPhaseTiming.Phase = Phase;
			CurrentPhaseTiming.StartTimeSeconds = FPlatformTime::Seconds();

			LogEvent(ETraceEvent::PhaseStarted, StaticEnum<ELoadPhase>()->GetNameStringByValue(static_cast<int64>(Phase)));
		}

		/** Mark phase as completed */
		void EndPhaseSuccess(ELoadPhase Phase)
		{
			CurrentPhaseTiming.EndTimeSeconds = FPlatformTime::Seconds();
			CurrentPhaseTiming.bSuccess = true;
			Telemetry.AddPhaseTiming(CurrentPhaseTiming);

			LogEvent(ETraceEvent::PhaseCompleted,
				FString::Printf(TEXT("%s duration=%.1fms"),
					*StaticEnum<ELoadPhase>()->GetNameStringByValue(static_cast<int64>(Phase)),
					CurrentPhaseTiming.GetDurationMs()));
		}

		/** Mark phase as failed */
		void EndPhaseFailed(ELoadPhase Phase, int32 ErrorCode, const FString& ErrorMessage)
		{
			CurrentPhaseTiming.EndTimeSeconds = FPlatformTime::Seconds();
			CurrentPhaseTiming.bSuccess = false;
			CurrentPhaseTiming.ErrorCode = ErrorCode;
			CurrentPhaseTiming.ErrorMessage = ErrorMessage;
			Telemetry.AddPhaseTiming(CurrentPhaseTiming);

			LogEvent(ETraceEvent::PhaseFailed,
				FString::Printf(TEXT("%s error=%d msg=%s duration=%.1fms"),
					*StaticEnum<ELoadPhase>()->GetNameStringByValue(static_cast<int64>(Phase)),
					ErrorCode,
					*ErrorMessage,
					CurrentPhaseTiming.GetDurationMs()));
		}

		/** Mark phase as skipped */
		void EndPhaseSkipped(ELoadPhase Phase)
		{
			CurrentPhaseTiming.EndTimeSeconds = FPlatformTime::Seconds();
			CurrentPhaseTiming.bSuccess = true;
			CurrentPhaseTiming.bSkipped = true;
			Telemetry.AddPhaseTiming(CurrentPhaseTiming);

			LogEvent(ETraceEvent::PhaseSkipped,
				StaticEnum<ELoadPhase>()->GetNameStringByValue(static_cast<int64>(Phase)));
		}

		/** Record retry attempt */
		void RecordRetry(ELoadPhase Phase, int32 Attempt, float DelaySeconds)
		{
			CurrentPhaseTiming.RetryCount = Attempt;

			LogEvent(ETraceEvent::PhaseRetrying,
				FString::Printf(TEXT("%s attempt=%d delay=%.1fs"),
					*StaticEnum<ELoadPhase>()->GetNameStringByValue(static_cast<int64>(Phase)),
					Attempt,
					DelaySeconds));
		}

		/** Record progress update */
		void RecordProgress(float Progress, const FString& StatusMessage)
		{
			// Only log significant progress changes (every 10%) to avoid spam
			const int32 ProgressBucket = FMath::FloorToInt(Progress * 10.0f);
			if (ProgressBucket != LastProgressBucket)
			{
				LastProgressBucket = ProgressBucket;
				LogEvent(ETraceEvent::ProgressUpdate,
					FString::Printf(TEXT("%.0f%% %s"), Progress * 100.0f, *StatusMessage));
			}
		}

		/** Record cancellation request */
		void RecordCancellation(bool bForce, const FString& CurrentPhase)
		{
			LogEvent(ETraceEvent::CancellationRequested,
				FString::Printf(TEXT("force=%s phase=%s"), bForce ? TEXT("true") : TEXT("false"), *CurrentPhase));
		}

		/** Complete trace with success */
		void CompleteSuccess()
		{
			Telemetry.bSuccess = true;
			Telemetry.TotalDurationMs = Context.GetElapsedSeconds() * 1000.0;

			LogEvent(ETraceEvent::PipelineCompleted,
				FString::Printf(TEXT("duration=%.1fms"), Telemetry.TotalDurationMs));

			LogTelemetrySummary();
		}

		/** Complete trace with failure */
		void CompleteFailed(const FString& ErrorMessage)
		{
			Telemetry.bSuccess = false;
			Telemetry.FinalError = ErrorMessage;
			Telemetry.TotalDurationMs = Context.GetElapsedSeconds() * 1000.0;

			LogEvent(ETraceEvent::PipelineFailed,
				FString::Printf(TEXT("duration=%.1fms error=%s"), Telemetry.TotalDurationMs, *ErrorMessage));

			LogTelemetrySummary();
		}

		/** Complete trace with cancellation */
		void CompleteCancelled()
		{
			Telemetry.bSuccess = false;
			Telemetry.bCancelled = true;
			Telemetry.TotalDurationMs = Context.GetElapsedSeconds() * 1000.0;

			LogEvent(ETraceEvent::PipelineCancelled,
				FString::Printf(TEXT("duration=%.1fms"), Telemetry.TotalDurationMs));

			LogTelemetrySummary();
		}

		/** Get collected telemetry */
		const FPipelineTelemetry& GetTelemetry() const { return Telemetry; }

		/** Get trace context */
		const FTraceContext& GetContext() const { return Context; }

	private:
		void LogEvent(ETraceEvent Event, const FString& Details)
		{
			UE_LOG(LogLoadingPipeline, Log, TEXT("[%s] %s %s %s"),
				*Context.GetShortId(),
				TraceEventToString(Event),
				*Context.Target,
				*Details);
		}

		void LogTelemetrySummary()
		{
			UE_LOG(LogLoadingPipeline, Display, TEXT("Pipeline Summary: %s"), *Telemetry.ToSummaryString());
			UE_LOG(LogLoadingPipeline, Verbose, TEXT("Pipeline Telemetry: %s"), *Telemetry.ToDetailedString());
		}

		FTraceContext Context;
		FPhaseTiming CurrentPhaseTiming;
		FPipelineTelemetry Telemetry;
		int32 LastProgressBucket = -1;
	};

} // namespace LoadingObservability
