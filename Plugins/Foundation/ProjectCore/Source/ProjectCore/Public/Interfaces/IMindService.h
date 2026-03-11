// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/DateTime.h"
#include "IMindService.generated.h"

UENUM(BlueprintType)
enum class EMindThoughtChannel : uint8
{
	Toast UMETA(DisplayName = "Toast"),
	Journal UMETA(DisplayName = "Journal"),
	ToastAndJournal UMETA(DisplayName = "ToastAndJournal")
};

UENUM(BlueprintType)
enum class EMindThoughtSourceType : uint8
{
	Unknown UMETA(DisplayName = "Unknown"),
	Dialogue UMETA(DisplayName = "Dialogue"),
	Vitals UMETA(DisplayName = "Vitals"),
	Inventory UMETA(DisplayName = "Inventory"),
	Scan UMETA(DisplayName = "Scan"),
	Quest UMETA(DisplayName = "Quest"),
	Beacon UMETA(DisplayName = "Beacon"),
	System UMETA(DisplayName = "System")
};

UENUM(BlueprintType)
enum class EMindRecordState : uint8
{
	None UMETA(DisplayName = "None"),
	Active UMETA(DisplayName = "Active"),
	Resolved UMETA(DisplayName = "Resolved")
};

/**
 * Lightweight thought view consumed by UI/view models.
 * Runtime ownership stays in ProjectMind.
 */
USTRUCT(BlueprintType)
struct PROJECTCORE_API FMindThoughtView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Mind")
	FName ThoughtId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Mind")
	EMindThoughtChannel Channel = EMindThoughtChannel::Toast;

	UPROPERTY(BlueprintReadOnly, Category = "Mind")
	int32 Priority = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Mind")
	EMindThoughtSourceType SourceType = EMindThoughtSourceType::Unknown;

	/** Stable record key for durable journal lifecycle (optional). */
	UPROPERTY(BlueprintReadOnly, Category = "Mind")
	FName RecordId = NAME_None;

	/** Record lifecycle state for durable journal entries. */
	UPROPERTY(BlueprintReadOnly, Category = "Mind")
	EMindRecordState RecordState = EMindRecordState::None;

	UPROPERTY(BlueprintReadOnly, Category = "Mind")
	FText Text;

	UPROPERTY(BlueprintReadOnly, Category = "Mind")
	float TimeToLiveSec = 6.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Mind")
	FDateTime CreatedAtUtc;

	/**
	 * Monotonic timestamp used for in-session TTL decay.
	 * Kept for runtime/UI timing stability.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Mind")
	float CreatedAtSec = 0.0f;
};

/** Fired when a new thought is emitted by the local mind runtime. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMindThoughtAdded, const FMindThoughtView&);

/** Fired when thought feed snapshot changed (append/prune/reset). */
DECLARE_MULTICAST_DELEGATE(FOnMindFeedChanged);

/**
 * Read-only feed contract for consumers that only need thought stream access.
 */
class PROJECTCORE_API IMindThoughtFeed
{
public:
	virtual ~IMindThoughtFeed() = default;

	/** Snapshot of current in-memory thought history (ordered oldest -> newest). */
	virtual void GetThoughtHistory(TArray<FMindThoughtView>& OutThoughts) const = 0;

	/** Latest thought convenience helper; false when feed is empty. */
	virtual bool GetLatestThought(FMindThoughtView& OutThought) const = 0;

	/** Event stream for toast-like consumers. */
	virtual FOnMindThoughtAdded& OnThoughtAdded() = 0;

	/** Event stream for list/journal consumers. */
	virtual FOnMindFeedChanged& OnMindFeedChanged() = 0;
};

/**
 * Read-only mind service contract for UI consumers.
 * Implementation lives in ProjectMind runtime plugin.
 */
class PROJECTCORE_API IMindService : public IMindThoughtFeed, public TSharedFromThis<IMindService>
{
public:
	virtual ~IMindService() = default;

	static FName ServiceKey() { return FName(TEXT("IMindService")); }
};
