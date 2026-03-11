// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IMindService.h"

class APawn;

/**
 * Source context passed from ProjectMind runtime to source adapters.
 */
struct FMindThoughtSourceContext
{
	/** Local player pawn context. Can be null during bootstrap/transitions. */
	TWeakObjectPtr<APawn> LocalPlayerPawn;
};

/**
 * Candidate thought produced by an external source adapter.
 * ProjectMind runtime applies final aggregator policy (rate, dedupe, cooldown).
 */
struct FMindThoughtCandidate
{
	FName ThoughtId = NAME_None;
	FText ThoughtText;
	float TimeToLiveSec = 6.0f;
	double CooldownSec = 8.0;
	double DedupeWindowSec = 4.0;
	FName DedupeKey = NAME_None;
	EMindThoughtChannel Channel = EMindThoughtChannel::ToastAndJournal;
	int32 Priority = 0;
	EMindThoughtSourceType SourceType = EMindThoughtSourceType::Unknown;
	FName RecordId = NAME_None;
	EMindRecordState RecordState = EMindRecordState::None;
};

/** Broadcast by source adapters when source state changed and runtime should re-evaluate. */
DECLARE_MULTICAST_DELEGATE(FOnMindThoughtSourceChanged);

/**
 * Base source abstraction for external adapters.
 *
 * Default implementations live in ProjectMind; optional gameplay plugins can
 * provide overrides by registering the same service contract.
 */
class IMindThoughtSource
{
public:
	virtual ~IMindThoughtSource() = default;

	virtual void CollectThoughts(
		const FMindThoughtSourceContext& Context,
		TArray<FMindThoughtCandidate>& OutThoughts) const = 0;

	virtual FOnMindThoughtSourceChanged& OnThoughtSourceChanged() = 0;
};

/** Vitals adapter contract. */
class IMindVitalsThoughtSource : public IMindThoughtSource
{
public:
	virtual ~IMindVitalsThoughtSource() = default;
	static FName ServiceKey() { return FName(TEXT("IMindVitalsThoughtSource")); }
};

/** Inventory adapter contract. */
class IMindInventoryThoughtSource : public IMindThoughtSource
{
public:
	virtual ~IMindInventoryThoughtSource() = default;
	static FName ServiceKey() { return FName(TEXT("IMindInventoryThoughtSource")); }
};

/** Quest adapter contract (implemented by quest/progression plugin when available). */
class IMindQuestThoughtSource : public IMindThoughtSource
{
public:
	virtual ~IMindQuestThoughtSource() = default;
	static FName ServiceKey() { return FName(TEXT("IMindQuestThoughtSource")); }
};
