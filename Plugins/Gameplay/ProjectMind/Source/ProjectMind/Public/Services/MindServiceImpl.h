// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "Interfaces/IMindService.h"
#include "Interfaces/IMindThoughtSource.h"
#include "Interfaces/IMindRuntimeControl.h"
#include <cfloat>

class APawn;

/**
 * Local boxed runtime that listens to external gameplay services
 * and emits internal thought feed for UI consumers.
 */
class PROJECTMIND_API FMindServiceImpl : public IMindService, public IMindRuntimeControl
{
public:
	explicit FMindServiceImpl(
		const FString& InDialogueMappingPath = FString(),
		const FString& InLegacyVitalsMappingPath = FString(),
		const FString& InIdleScanRulePath = FString());
	virtual ~FMindServiceImpl() override = default;

	void Start();
	void Stop();

#if WITH_DEV_AUTOMATION_TESTS
	void PumpProvidersForTests(bool bIncludeIdleScanCandidates = false);
	void SetGlobalEmitCooldownForTests(double InCooldownSec);
	int32 GetIdleScanRuleCountForTests() const;
#endif

	// IMindService
	virtual void GetThoughtHistory(TArray<FMindThoughtView>& OutThoughts) const override;
	virtual bool GetLatestThought(FMindThoughtView& OutThought) const override;
	virtual FOnMindThoughtAdded& OnThoughtAdded() override;
	virtual FOnMindFeedChanged& OnMindFeedChanged() override;

	// IMindRuntimeControl
	virtual void SetLocalPlayerPawn(APawn* InPawn) override;
	virtual void ClearLocalPlayerPawn(const APawn* InPawn) override;
	virtual void NotifyPlayerInputActivity() override;

private:
	struct FMindThoughtTemplate
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

	struct FDialogueThoughtMappingEntry
	{
		FMindThoughtTemplate ThoughtTemplate;
	};

	struct FIdleScanThoughtRule
	{
		FName RuleId = NAME_None;
		TArray<FGameplayTag> MatchAnyTags;
		float MinDistanceCm = 0.0f;
		float MaxDistanceCm = 4500.0f;
		float MinViewDot = 0.35f;
		float MinScreenAreaRatio = 0.0f;
		bool bRequireLineOfSight = true;
		FString ThoughtIdPrefix;
		FString TextTemplate;
		TArray<FString> TextTemplates;
		EMindThoughtChannel Channel = EMindThoughtChannel::ToastAndJournal;
		int32 PriorityBase = 0;
		EMindThoughtSourceType SourceType = EMindThoughtSourceType::Unknown;
		double CooldownSec = 8.0;
		double DedupeWindowSec = 4.0;
		float TimeToLiveSec = 6.0f;
		FString DedupeKeyPrefix;
	};

	void LoadDialogueThoughtMappings();
	void LoadIdleScanThoughtRules();
	void BuildDefaultIdleScanThoughtRules();
	bool TryBindDialogue(float DeltaTime);
	void UnbindDialogue();
	void HandleDialogueStateChanged();
	void HandleDialogueSignal(const FName& SignalTag);
	void HandleThoughtSourceChanged();
	void BindThoughtSources();
	void UnbindThoughtSources();

	void EvaluateProviderPipeline(bool bIncludeIdleScanCandidates);
	void CollectThoughtsFromConfiguredSources(TArray<FMindThoughtTemplate>& OutThoughts) const;
	void CollectIdleScanThoughts(TArray<FMindThoughtTemplate>& OutThoughts) const;
	static FMindThoughtTemplate ConvertCandidateToTemplate(const FMindThoughtCandidate& Candidate);

	void RearmIdleScan();
	void CancelIdleScan();
	void HandleIdleScanTriggered();
	UWorld* ResolveRuntimeWorld() const;

	void EmitThought(const FDialogueThoughtMappingEntry& MappingEntry);
	bool TryEmitThought(const FMindThoughtTemplate& ThoughtTemplate);
	bool CanEmitThoughtNow(const FMindThoughtTemplate& ThoughtTemplate, double NowSec) const;
	FName ResolveDedupeKey(const FMindThoughtTemplate& ThoughtTemplate) const;
	FString ResolveDialogueMappingPath() const;
	FString ResolveIdleScanRulePath() const;

private:
	FString DialogueMappingPath;
	FString LegacyVitalsMappingPath;
	FString IdleScanRulePath;
	TMap<FName, FDialogueThoughtMappingEntry> DialogueThoughtMappingsBySignal;
	TArray<FIdleScanThoughtRule> IdleScanThoughtRules;

	TArray<FMindThoughtView> ThoughtHistory;
	int32 MaxThoughtHistory = 200;
	TWeakObjectPtr<APawn> LocalPlayerPawn;

	mutable TMap<FName, double> LastEmitTimeByThought;
	mutable TMap<FName, double> LastEmitTimeByDedupeKey;
	double LastGlobalEmitTimeSec = -DBL_MAX;
	double GlobalEmitCooldownSec = 1.5;
	double LastHydrationResourceSeenSec = -DBL_MAX;
	double LastCaloriesResourceSeenSec = -DBL_MAX;
	double HydrationReminderSuppressAfterResourceSec = 20.0;
	double CaloriesReminderSuppressAfterResourceSec = 20.0;
	int32 ScanPriorityForImportantJournal = 235;

	FOnMindThoughtAdded ThoughtAddedDelegate;
	FOnMindFeedChanged FeedChangedDelegate;

	TSharedPtr<IMindVitalsThoughtSource> VitalsThoughtSource;
	TSharedPtr<IMindInventoryThoughtSource> InventoryThoughtSource;
	TSharedPtr<IMindQuestThoughtSource> QuestThoughtSource;
	FDelegateHandle VitalsThoughtSourceChangedHandle;
	FDelegateHandle InventoryThoughtSourceChangedHandle;
	FDelegateHandle QuestThoughtSourceChangedHandle;

	FDelegateHandle DialogueSignalHandle;
	FTimerHandle IdleScanTimerHandle;
	float IdleScanDelaySec = 3.0f;
};
