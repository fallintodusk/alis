// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Analytics event data for session milestone tracking.
 * Queued for batch upload to analytics backend.
 */
struct FProjectAnalyticsEvent
{
	/** Event category (e.g., "Session", "Loading", "Gameplay") */
	FString Category;

	/** Event action (e.g., "Start", "Complete", "Transition") */
	FString Action;

	/** Event label (e.g., milestone ID, map name) */
	FString Label;

	/** Optional numeric value (e.g., duration in seconds, count) */
	float Value = 0.0f;

	/** Timestamp of event */
	FDateTime Timestamp;

	/** Additional context as key-value pairs */
	TMap<FString, FString> CustomData;
};

/**
 * Interface for analytics service.
 * Concrete implementation will integrate with backend (e.g., Google Analytics, custom telemetry).
 *
 * Usage:
 *   IProjectAnalyticsService* Analytics = FProjectServiceLocator::Get().GetAnalyticsService();
 *   if (Analytics && Analytics->IsEnabled())
 *   {
 *       Analytics->RecordEvent(Event);
 *   }
 *
 * Implementation Notes:
 * - Events are queued and uploaded in batches to reduce network overhead
 * - Analytics can be disabled via project settings or user preferences
 * - All data should be anonymized per privacy policies
 *
 * See: todo/create_architecture.md Phase 10 (Telemetry & Analytics)
 */
class IProjectAnalyticsService
{
public:
	virtual ~IProjectAnalyticsService() = default;

	/**
	 * Check if analytics is enabled.
	 * @return True if analytics collection is active
	 */
	virtual bool IsEnabled() const = 0;

	/**
	 * Enable or disable analytics collection.
	 * @param bEnabled True to enable, false to disable
	 */
	virtual void SetEnabled(bool bEnabled) = 0;

	/**
	 * Record an analytics event.
	 * Event will be queued for batch upload to backend.
	 * @param Event Event data to record
	 */
	virtual void RecordEvent(const FProjectAnalyticsEvent& Event) = 0;

	/**
	 * Flush queued events to backend immediately.
	 * Normally events are uploaded in batches, but this forces immediate upload.
	 * Use for critical events or before application shutdown.
	 */
	virtual void FlushEvents() = 0;

	/**
	 * Get the number of queued events pending upload.
	 * @return Count of events in queue
	 */
	virtual int32 GetQueuedEventCount() const = 0;

	/**
	 * Start a new session for analytics tracking.
	 * Call when user begins gameplay session.
	 * @param SessionId Unique identifier for this session
	 */
	virtual void StartSession(const FString& SessionId) = 0;

	/**
	 * End the current analytics session.
	 * Call when user exits gameplay session.
	 */
	virtual void EndSession() = 0;
};

/**
 * Stub implementation for development/testing.
 * Logs events to console but does not upload to backend.
 */
class FProjectAnalyticsServiceStub : public IProjectAnalyticsService
{
public:
	FProjectAnalyticsServiceStub() : bEnabled(false) {}

	virtual bool IsEnabled() const override { return bEnabled; }
	virtual void SetEnabled(bool bInEnabled) override { bEnabled = bInEnabled; }

	virtual void RecordEvent(const FProjectAnalyticsEvent& Event) override
	{
		if (!bEnabled)
		{
			return;
		}

		UE_LOG(LogCore, Verbose, TEXT("[Analytics Stub] Event: %s/%s/%s (Value: %.2f, Timestamp: %s)"),
			*Event.Category, *Event.Action, *Event.Label, Event.Value, *Event.Timestamp.ToString());

		// Log custom data if present
		if (Event.CustomData.Num() > 0)
		{
			for (const TPair<FString, FString>& Pair : Event.CustomData)
			{
				UE_LOG(LogCore, VeryVerbose, TEXT("[Analytics Stub]   %s: %s"), *Pair.Key, *Pair.Value);
			}
		}

		QueuedEvents.Add(Event);
	}

	virtual void FlushEvents() override
	{
		if (QueuedEvents.Num() > 0)
		{
			UE_LOG(LogCore, Log, TEXT("[Analytics Stub] Flushing %d queued events (no-op)"), QueuedEvents.Num());
			QueuedEvents.Empty();
		}
	}

	virtual int32 GetQueuedEventCount() const override
	{
		return QueuedEvents.Num();
	}

	virtual void StartSession(const FString& SessionId) override
	{
		UE_LOG(LogCore, Log, TEXT("[Analytics Stub] StartSession: %s"), *SessionId);
	}

	virtual void EndSession() override
	{
		UE_LOG(LogCore, Log, TEXT("[Analytics Stub] EndSession"));
		FlushEvents();
	}

private:
	bool bEnabled;
	TArray<FProjectAnalyticsEvent> QueuedEvents;
};
