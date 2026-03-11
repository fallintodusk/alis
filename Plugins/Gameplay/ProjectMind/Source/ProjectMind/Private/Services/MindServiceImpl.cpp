// Copyright ALIS. All Rights Reserved.

#include "Services/MindServiceImpl.h"
#include "Interfaces/IDialogueService.h"
#include "Interfaces/IItemDataProvider.h"
#include "Interfaces/IPickupSource.h"
#include "ProjectServiceLocator.h"
#include "Algo/Sort.h"
#include "Components/ActorComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagAssetInterface.h"
#include "GameplayTagContainer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectMindService, Log, All);

namespace
{
const TCHAR* DefaultDialogueThoughtMappingRelativePath = TEXT("Data/Schema/Gameplay/ProjectMind/dialogue_thought_mappings.json");
const TCHAR* DefaultIdleScanRulesRelativePath = TEXT("Data/Schema/Gameplay/ProjectMind/scan_thought_rules.json");

EMindThoughtChannel ParseMindThoughtChannel(const FString& ChannelString)
{
	if (ChannelString.Equals(TEXT("Toast"), ESearchCase::IgnoreCase))
	{
		return EMindThoughtChannel::Toast;
	}

	if (ChannelString.Equals(TEXT("Journal"), ESearchCase::IgnoreCase))
	{
		return EMindThoughtChannel::Journal;
	}

	return EMindThoughtChannel::ToastAndJournal;
}

EMindThoughtSourceType ParseMindThoughtSourceType(const FString& SourceTypeString)
{
	if (SourceTypeString.Equals(TEXT("Dialogue"), ESearchCase::IgnoreCase))
	{
		return EMindThoughtSourceType::Dialogue;
	}

	if (SourceTypeString.Equals(TEXT("Vitals"), ESearchCase::IgnoreCase))
	{
		return EMindThoughtSourceType::Vitals;
	}

	if (SourceTypeString.Equals(TEXT("Inventory"), ESearchCase::IgnoreCase))
	{
		return EMindThoughtSourceType::Inventory;
	}

	if (SourceTypeString.Equals(TEXT("Scan"), ESearchCase::IgnoreCase))
	{
		return EMindThoughtSourceType::Scan;
	}

	if (SourceTypeString.Equals(TEXT("Quest"), ESearchCase::IgnoreCase))
	{
		return EMindThoughtSourceType::Quest;
	}

	if (SourceTypeString.Equals(TEXT("Beacon"), ESearchCase::IgnoreCase))
	{
		return EMindThoughtSourceType::Beacon;
	}

	if (SourceTypeString.Equals(TEXT("System"), ESearchCase::IgnoreCase))
	{
		return EMindThoughtSourceType::System;
	}

	return EMindThoughtSourceType::Unknown;
}

EMindRecordState ParseMindRecordState(const FString& RecordStateString)
{
	if (RecordStateString.Equals(TEXT("Active"), ESearchCase::IgnoreCase))
	{
		return EMindRecordState::Active;
	}

	if (RecordStateString.Equals(TEXT("Resolved"), ESearchCase::IgnoreCase))
	{
		return EMindRecordState::Resolved;
	}

	return EMindRecordState::None;
}

FName ParseOptionalNameField(const TSharedPtr<FJsonObject>& EntryObject, const TCHAR* FieldName)
{
	FString ValueString;
	if (!EntryObject->TryGetStringField(FieldName, ValueString) || ValueString.IsEmpty())
	{
		return NAME_None;
	}

	return FName(*ValueString);
}

FName ParseSignalTagName(const FString& RawSignalTagString)
{
	FString SignalTagString = RawSignalTagString;
	SignalTagString.TrimStartAndEndInline();
	if (SignalTagString.IsEmpty())
	{
		return NAME_None;
	}

	SignalTagString.ReplaceInline(TEXT(" "), TEXT("_"));
	return FName(*SignalTagString);
}

FName BuildDialogueSignalTag(const FName TreeId, const FName NodeId)
{
	if (TreeId.IsNone() || NodeId.IsNone())
	{
		return NAME_None;
	}

	FString TreeIdString = TreeId.ToString();
	FString NodeIdString = NodeId.ToString();
	TreeIdString.ReplaceInline(TEXT(" "), TEXT("_"));
	NodeIdString.ReplaceInline(TEXT(" "), TEXT("_"));

	const FString SignalTagString = FString::Printf(
		TEXT("Dialogue.%s.%s"),
		*TreeIdString,
		*NodeIdString);
	return FName(*SignalTagString);
}

void AddGameplayTagUnique(TArray<FGameplayTag>& InOutTags, const FGameplayTag& Tag)
{
	if (!Tag.IsValid())
	{
		return;
	}

	for (const FGameplayTag& ExistingTag : InOutTags)
	{
		if (ExistingTag == Tag)
		{
			return;
		}
	}

	InOutTags.Add(Tag);
}

void AddGameplayTagContainerUnique(TArray<FGameplayTag>& InOutTags, const FGameplayTagContainer& Container)
{
	for (const FGameplayTag& Tag : Container)
	{
		AddGameplayTagUnique(InOutTags, Tag);
	}
}

void CollectOwnedGameplayTagsFromObject(const UObject* Object, TArray<FGameplayTag>& OutTags)
{
	const IGameplayTagAssetInterface* TagSource = Cast<const IGameplayTagAssetInterface>(Object);
	if (!TagSource)
	{
		return;
	}

	FGameplayTagContainer OwnedTags;
	TagSource->GetOwnedGameplayTags(OwnedTags);
	AddGameplayTagContainerUnique(OutTags, OwnedTags);
}

UObject* FindPickupSourceObject(const AActor& Actor)
{
	if (Actor.Implements<UPickupSource>())
	{
		return const_cast<AActor*>(&Actor);
	}

	TInlineComponentArray<UActorComponent*> Components;
	Actor.GetComponents(Components);

	for (UActorComponent* Component : Components)
	{
		if (Component && Component->Implements<UPickupSource>())
		{
			return Component;
		}
	}

	return nullptr;
}

bool TryResolveItemDataViewForObjectDefinitionId(const FPrimaryAssetId& ObjectDefinitionId, FItemDataView& OutItemData)
{
	OutItemData = FItemDataView();
	if (!ObjectDefinitionId.IsValid())
	{
		return false;
	}

	UAssetManager* AssetManager = UAssetManager::GetIfInitialized();
	if (!AssetManager)
	{
		return false;
	}

	const FSoftObjectPath AssetPath = AssetManager->GetPrimaryAssetPath(ObjectDefinitionId);
	if (AssetPath.IsNull())
	{
		return false;
	}

	UObject* LoadedObject = AssetPath.TryLoad();
	if (!LoadedObject || !LoadedObject->Implements<UItemDataProvider>())
	{
		return false;
	}

	OutItemData = IItemDataProvider::Execute_GetItemDataView(LoadedObject);
	return OutItemData.bIsValid;
}

void CollectActorGameplayTags(
	const AActor& Actor,
	TMap<FPrimaryAssetId, FItemDataView>& PerScanItemDataCache,
	TArray<FGameplayTag>& OutTags,
	FItemDataView* OutPickupItemData)
{
	OutTags.Reset();
	if (OutPickupItemData)
	{
		*OutPickupItemData = FItemDataView();
	}

	OutTags.Reserve(Actor.Tags.Num() + 8);
	for (const FName& ActorTagName : Actor.Tags)
	{
		if (ActorTagName.IsNone())
		{
			continue;
		}

		const FGameplayTag ActorTag = FGameplayTag::RequestGameplayTag(ActorTagName, false);
		AddGameplayTagUnique(OutTags, ActorTag);
	}

	CollectOwnedGameplayTagsFromObject(&Actor, OutTags);

	TInlineComponentArray<UActorComponent*> Components;
	Actor.GetComponents(Components);
	for (const UActorComponent* Component : Components)
	{
		if (!Component)
		{
			continue;
		}

		CollectOwnedGameplayTagsFromObject(Component, OutTags);
	}

	UObject* PickupSourceObject = FindPickupSourceObject(Actor);
	if (!PickupSourceObject)
	{
		return;
	}

	// Bridge pickup capability presence into scan tags even when actor tags are not authored.
	const FGameplayTag PickupTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Item.Pickup")), false);
	AddGameplayTagUnique(OutTags, PickupTag);
	const FGameplayTag InteractableTag = FGameplayTag::RequestGameplayTag(FName(TEXT("World.Interactable")), false);
	AddGameplayTagUnique(OutTags, InteractableTag);

	const FPrimaryAssetId ObjectDefinitionId = IPickupSource::Execute_GetObjectDefinitionId(PickupSourceObject);
	if (!ObjectDefinitionId.IsValid())
	{
		return;
	}

	FItemDataView ItemData;
	if (const FItemDataView* CachedItemData = PerScanItemDataCache.Find(ObjectDefinitionId))
	{
		ItemData = *CachedItemData;
	}
	else
	{
		TryResolveItemDataViewForObjectDefinitionId(ObjectDefinitionId, ItemData);
		PerScanItemDataCache.Add(ObjectDefinitionId, ItemData);
	}

	if (!ItemData.bIsValid)
	{
		return;
	}

	AddGameplayTagContainerUnique(OutTags, ItemData.Tags);
	for (const TPair<FGameplayTag, float>& MagnitudePair : ItemData.Magnitudes)
	{
		AddGameplayTagUnique(OutTags, MagnitudePair.Key);
	}

	if (OutPickupItemData)
	{
		*OutPickupItemData = ItemData;
	}
}

FString SelectScanThoughtTextTemplate(
	const TArray<FString>& TextTemplates,
	const FName RuleId,
	const FString& StableActorSuffix,
	double CooldownSec,
	double NowSec)
{
	if (TextTemplates.Num() == 0)
	{
		return FString();
	}

	const double BucketSizeSec = CooldownSec > 1.0 ? CooldownSec : 30.0;
	const int32 TimeBucket = FMath::FloorToInt(static_cast<float>(NowSec / BucketSizeSec));

	const uint32 BaseHash = HashCombineFast(
		GetTypeHash(RuleId),
		GetTypeHash(StableActorSuffix));
	const uint32 BucketHash = HashCombineFast(BaseHash, GetTypeHash(TimeBucket));
	const int32 VariantIndex = static_cast<int32>(BucketHash % static_cast<uint32>(TextTemplates.Num()));
	return TextTemplates[VariantIndex];
}

bool TryComputeActorScreenAreaRatio(
	const AActor& Actor,
	const FVector& ViewLocation,
	const APlayerController* PlayerController,
	float& OutScreenAreaRatio)
{
	OutScreenAreaRatio = 0.0f;
	if (!PlayerController)
	{
		return false;
	}

	int32 ViewportX = 0;
	int32 ViewportY = 0;
	PlayerController->GetViewportSize(ViewportX, ViewportY);
	if (ViewportX <= 0 || ViewportY <= 0)
	{
		ViewportX = 1920;
		ViewportY = 1080;
	}

	FVector BoundsOrigin = FVector::ZeroVector;
	FVector BoundsExtents = FVector::ZeroVector;
	Actor.GetActorBounds(true, BoundsOrigin, BoundsExtents, true);

	const float BoundingSphereRadiusCm = FMath::Max(5.0f, BoundsExtents.Size());
	const float DistanceCm = FVector::Dist(ViewLocation, BoundsOrigin);
	if (DistanceCm <= KINDA_SMALL_NUMBER)
	{
		OutScreenAreaRatio = 1.0f;
		return true;
	}

	float FovDeg = 90.0f;
	if (const APlayerCameraManager* CameraManager = PlayerController->PlayerCameraManager)
	{
		FovDeg = FMath::Clamp(CameraManager->GetFOVAngle(), 10.0f, 170.0f);
	}

	const float FovRad = FMath::DegreesToRadians(FovDeg);
	const float ProjectionScale = (static_cast<float>(ViewportY) * 0.5f) / FMath::Tan(FovRad * 0.5f);
	const float RadiusPx = (BoundingSphereRadiusCm / DistanceCm) * ProjectionScale;
	const float AreaPx = PI * RadiusPx * RadiusPx;
	const float ViewportAreaPx = static_cast<float>(ViewportX * ViewportY);
	OutScreenAreaRatio = ViewportAreaPx > 0.0f
		? FMath::Clamp(AreaPx / ViewportAreaPx, 0.0f, 1.0f)
		: 0.0f;

	return true;
}

bool IsHydrationScanThoughtId(const FName ThoughtId)
{
	return ThoughtId.ToString().StartsWith(TEXT("Mind.Scan.Hydration"));
}

bool IsCaloriesScanThoughtId(const FName ThoughtId)
{
	return ThoughtId.ToString().StartsWith(TEXT("Mind.Scan.Calories"));
}

bool IsHydrationVitalsThoughtId(const FName ThoughtId)
{
	return ThoughtId.ToString().StartsWith(TEXT("Mind.Vitals.Hydration"));
}

bool IsCaloriesVitalsThoughtId(const FName ThoughtId)
{
	return ThoughtId.ToString().StartsWith(TEXT("Mind.Vitals.Calories"));
}
}

FMindServiceImpl::FMindServiceImpl(
	const FString& InDialogueMappingPath,
	const FString& InLegacyVitalsMappingPath,
	const FString& InIdleScanRulePath)
	: DialogueMappingPath(InDialogueMappingPath)
	, LegacyVitalsMappingPath(InLegacyVitalsMappingPath)
	, IdleScanRulePath(InIdleScanRulePath)
{
}

void FMindServiceImpl::Start()
{
	LoadDialogueThoughtMappings();
	LoadIdleScanThoughtRules();
	BindThoughtSources();
	TryBindDialogue(0.0f);
}

void FMindServiceImpl::Stop()
{
	CancelIdleScan();
	UnbindDialogue();
	UnbindThoughtSources();
	LocalPlayerPawn.Reset();
}

#if WITH_DEV_AUTOMATION_TESTS
void FMindServiceImpl::PumpProvidersForTests(bool bIncludeIdleScanCandidates)
{
	EvaluateProviderPipeline(bIncludeIdleScanCandidates);
}

void FMindServiceImpl::SetGlobalEmitCooldownForTests(double InCooldownSec)
{
	GlobalEmitCooldownSec = FMath::Max(0.0, InCooldownSec);
}

int32 FMindServiceImpl::GetIdleScanRuleCountForTests() const
{
	return IdleScanThoughtRules.Num();
}
#endif

void FMindServiceImpl::GetThoughtHistory(TArray<FMindThoughtView>& OutThoughts) const
{
	OutThoughts = ThoughtHistory;
}

bool FMindServiceImpl::GetLatestThought(FMindThoughtView& OutThought) const
{
	if (ThoughtHistory.Num() == 0)
	{
		return false;
	}

	OutThought = ThoughtHistory.Last();
	return true;
}

FOnMindThoughtAdded& FMindServiceImpl::OnThoughtAdded()
{
	return ThoughtAddedDelegate;
}

FOnMindFeedChanged& FMindServiceImpl::OnMindFeedChanged()
{
	return FeedChangedDelegate;
}

void FMindServiceImpl::SetLocalPlayerPawn(APawn* InPawn)
{
	LocalPlayerPawn = InPawn;
	BindThoughtSources();
	TryBindDialogue(0.0f);
}

void FMindServiceImpl::ClearLocalPlayerPawn(const APawn* InPawn)
{
	if (LocalPlayerPawn.Get() == InPawn)
	{
		CancelIdleScan();
		LocalPlayerPawn.Reset();
	}
}

void FMindServiceImpl::NotifyPlayerInputActivity()
{
	BindThoughtSources();
	TryBindDialogue(0.0f);
	RearmIdleScan();
}

void FMindServiceImpl::LoadDialogueThoughtMappings()
{
	DialogueThoughtMappingsBySignal.Reset();

	const FString MappingPath = ResolveDialogueMappingPath();
	FString JsonContent;
	if (!FFileHelper::LoadFileToString(JsonContent, *MappingPath))
	{
		UE_LOG(LogProjectMindService, Warning, TEXT("Dialogue mapping file is missing: %s"), *MappingPath);
		return;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonContent);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		UE_LOG(LogProjectMindService, Warning, TEXT("Failed to parse dialogue mapping file: %s"), *MappingPath);
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("entries"), Entries) || !Entries)
	{
		UE_LOG(LogProjectMindService, Warning, TEXT("Dialogue mapping file has no 'entries' array: %s"), *MappingPath);
		return;
	}

	int32 RegisteredSignalCount = 0;

	for (const TSharedPtr<FJsonValue>& EntryValue : *Entries)
	{
		const TSharedPtr<FJsonObject> EntryObject = EntryValue.IsValid() ? EntryValue->AsObject() : nullptr;
		if (!EntryObject.IsValid())
		{
			continue;
		}

		FString ThoughtIdString;
		FString ThoughtTextString;
		if (!EntryObject->TryGetStringField(TEXT("thought_id"), ThoughtIdString)
			|| !EntryObject->TryGetStringField(TEXT("text"), ThoughtTextString))
		{
			UE_LOG(LogProjectMindService, Warning, TEXT("Skipping invalid dialogue mapping entry in %s"), *MappingPath);
			continue;
		}

		FDialogueThoughtMappingEntry MappingEntry;
		MappingEntry.ThoughtTemplate.ThoughtId = FName(*ThoughtIdString);
		MappingEntry.ThoughtTemplate.ThoughtText = FText::FromString(ThoughtTextString);
		MappingEntry.ThoughtTemplate.SourceType = EMindThoughtSourceType::Dialogue;

		EntryObject->TryGetNumberField(TEXT("time_to_live_sec"), MappingEntry.ThoughtTemplate.TimeToLiveSec);
		EntryObject->TryGetNumberField(TEXT("cooldown_sec"), MappingEntry.ThoughtTemplate.CooldownSec);
		EntryObject->TryGetNumberField(TEXT("dedupe_window_sec"), MappingEntry.ThoughtTemplate.DedupeWindowSec);
		MappingEntry.ThoughtTemplate.DedupeKey = ParseOptionalNameField(EntryObject, TEXT("dedupe_key"));

		double PriorityValue = static_cast<double>(MappingEntry.ThoughtTemplate.Priority);
		if (EntryObject->TryGetNumberField(TEXT("priority"), PriorityValue))
		{
			MappingEntry.ThoughtTemplate.Priority = static_cast<int32>(PriorityValue);
		}

		FString ChannelString;
		if (EntryObject->TryGetStringField(TEXT("channel"), ChannelString))
		{
			MappingEntry.ThoughtTemplate.Channel = ParseMindThoughtChannel(ChannelString);
		}

		FString SourceTypeString;
		if (EntryObject->TryGetStringField(TEXT("source_type"), SourceTypeString))
		{
			MappingEntry.ThoughtTemplate.SourceType = ParseMindThoughtSourceType(SourceTypeString);
		}

		MappingEntry.ThoughtTemplate.RecordId = ParseOptionalNameField(EntryObject, TEXT("record_id"));
		FString RecordStateString;
		if (EntryObject->TryGetStringField(TEXT("record_state"), RecordStateString))
		{
			MappingEntry.ThoughtTemplate.RecordState = ParseMindRecordState(RecordStateString);
		}

		if (MappingEntry.ThoughtTemplate.ThoughtId.IsNone() || MappingEntry.ThoughtTemplate.ThoughtText.IsEmpty())
		{
			continue;
		}

		TArray<FName> SignalTags;
		SignalTags.Reserve(4);

		FString SignalTagString;
		if (EntryObject->TryGetStringField(TEXT("signal_tag"), SignalTagString))
		{
			const FName SignalTag = ParseSignalTagName(SignalTagString);
			if (!SignalTag.IsNone())
			{
				SignalTags.AddUnique(SignalTag);
			}
		}

		const TArray<TSharedPtr<FJsonValue>>* SignalTagValues = nullptr;
		if (EntryObject->TryGetArrayField(TEXT("signal_tags"), SignalTagValues) && SignalTagValues)
		{
			for (const TSharedPtr<FJsonValue>& SignalValue : *SignalTagValues)
			{
				FString SignalTagVariant;
				if (!SignalValue.IsValid() || !SignalValue->TryGetString(SignalTagVariant))
				{
					continue;
				}

				const FName SignalTag = ParseSignalTagName(SignalTagVariant);
				if (!SignalTag.IsNone())
				{
					SignalTags.AddUnique(SignalTag);
				}
			}
		}

		// Legacy fallback for old mapping shape.
		FString TreeIdString;
		FString NodeIdString;
		if (SignalTags.Num() == 0
			&& EntryObject->TryGetStringField(TEXT("tree_id"), TreeIdString)
			&& EntryObject->TryGetStringField(TEXT("node_id"), NodeIdString))
		{
			const FName LegacySignalTag = BuildDialogueSignalTag(FName(*TreeIdString), FName(*NodeIdString));
			if (!LegacySignalTag.IsNone())
			{
				SignalTags.Add(LegacySignalTag);
			}
		}

		if (SignalTags.Num() == 0)
		{
			UE_LOG(
				LogProjectMindService,
				Warning,
				TEXT("Skipping dialogue mapping entry without signal key in %s"),
				*MappingPath);
			continue;
		}

		for (const FName& SignalTag : SignalTags)
		{
			DialogueThoughtMappingsBySignal.Add(SignalTag, MappingEntry);
			++RegisteredSignalCount;
		}
	}

	UE_LOG(
		LogProjectMindService,
		Log,
		TEXT("Loaded %d dialogue thought signal mappings (%d unique keys) from %s"),
		RegisteredSignalCount,
		DialogueThoughtMappingsBySignal.Num(),
		*MappingPath);
}

void FMindServiceImpl::LoadIdleScanThoughtRules()
{
	IdleScanThoughtRules.Reset();

	const FString RulePath = ResolveIdleScanRulePath();
	FString JsonContent;
	if (!FFileHelper::LoadFileToString(JsonContent, *RulePath))
	{
		UE_LOG(LogProjectMindService, Warning, TEXT("Idle scan rule file is missing: %s"), *RulePath);
		BuildDefaultIdleScanThoughtRules();
		return;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonContent);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		UE_LOG(LogProjectMindService, Warning, TEXT("Failed to parse idle scan rule file: %s"), *RulePath);
		BuildDefaultIdleScanThoughtRules();
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("entries"), Entries) || !Entries)
	{
		UE_LOG(LogProjectMindService, Warning, TEXT("Idle scan rule file has no 'entries' array: %s"), *RulePath);
		BuildDefaultIdleScanThoughtRules();
		return;
	}

	for (const TSharedPtr<FJsonValue>& EntryValue : *Entries)
	{
		const TSharedPtr<FJsonObject> EntryObject = EntryValue.IsValid() ? EntryValue->AsObject() : nullptr;
		if (!EntryObject.IsValid())
		{
			continue;
		}

		FString RuleIdString;
		FString ThoughtIdPrefixString;
		FString TextTemplateString;
		if (!EntryObject->TryGetStringField(TEXT("rule_id"), RuleIdString)
			|| !EntryObject->TryGetStringField(TEXT("thought_id_prefix"), ThoughtIdPrefixString)
			|| !EntryObject->TryGetStringField(TEXT("text_template"), TextTemplateString))
		{
			continue;
		}

		FMindServiceImpl::FIdleScanThoughtRule Rule;
		Rule.RuleId = FName(*RuleIdString);
		Rule.ThoughtIdPrefix = ThoughtIdPrefixString;
		Rule.TextTemplate = TextTemplateString;
		Rule.TextTemplates.Reset();

		const TArray<TSharedPtr<FJsonValue>>* TextTemplateArray = nullptr;
		if (EntryObject->TryGetArrayField(TEXT("text_templates"), TextTemplateArray) && TextTemplateArray)
		{
			for (const TSharedPtr<FJsonValue>& TemplateValue : *TextTemplateArray)
			{
				FString VariantTemplate;
				if (!TemplateValue.IsValid() || !TemplateValue->TryGetString(VariantTemplate) || VariantTemplate.IsEmpty())
				{
					continue;
				}

				Rule.TextTemplates.Add(VariantTemplate);
			}
		}

		if (Rule.TextTemplates.Num() == 0)
		{
			Rule.TextTemplates.Add(Rule.TextTemplate);
		}

		const TArray<TSharedPtr<FJsonValue>>* MatchTags = nullptr;
		if (!EntryObject->TryGetArrayField(TEXT("match_any_tags"), MatchTags) || !MatchTags)
		{
			continue;
		}

		for (const TSharedPtr<FJsonValue>& TagValue : *MatchTags)
		{
			FString TagString;
			if (!TagValue.IsValid() || !TagValue->TryGetString(TagString) || TagString.IsEmpty())
			{
				continue;
			}

			const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagString), false);
			if (Tag.IsValid())
			{
				Rule.MatchAnyTags.Add(Tag);
			}
		}

		if (Rule.RuleId.IsNone() || Rule.MatchAnyTags.Num() == 0 || Rule.ThoughtIdPrefix.IsEmpty() || Rule.TextTemplates.Num() == 0)
		{
			continue;
		}

		const TSharedPtr<FJsonObject>* DistanceObject = nullptr;
		if (EntryObject->TryGetObjectField(TEXT("distance_cm"), DistanceObject) && DistanceObject && DistanceObject->IsValid())
		{
			(*DistanceObject)->TryGetNumberField(TEXT("min"), Rule.MinDistanceCm);
			(*DistanceObject)->TryGetNumberField(TEXT("max"), Rule.MaxDistanceCm);
		}

		EntryObject->TryGetNumberField(TEXT("min_view_dot"), Rule.MinViewDot);
		EntryObject->TryGetNumberField(TEXT("min_screen_area_ratio"), Rule.MinScreenAreaRatio);
		EntryObject->TryGetBoolField(TEXT("los_required"), Rule.bRequireLineOfSight);
		EntryObject->TryGetNumberField(TEXT("cooldown_sec"), Rule.CooldownSec);
		EntryObject->TryGetNumberField(TEXT("dedupe_window_sec"), Rule.DedupeWindowSec);
		EntryObject->TryGetNumberField(TEXT("time_to_live_sec"), Rule.TimeToLiveSec);
		EntryObject->TryGetStringField(TEXT("dedupe_key_prefix"), Rule.DedupeKeyPrefix);

		double PriorityValue = static_cast<double>(Rule.PriorityBase);
		if (EntryObject->TryGetNumberField(TEXT("priority_base"), PriorityValue))
		{
			Rule.PriorityBase = static_cast<int32>(PriorityValue);
		}

		FString ChannelString;
		if (EntryObject->TryGetStringField(TEXT("channel"), ChannelString))
		{
			Rule.Channel = ParseMindThoughtChannel(ChannelString);
		}

		FString SourceTypeString;
		if (EntryObject->TryGetStringField(TEXT("source_type"), SourceTypeString))
		{
			Rule.SourceType = ParseMindThoughtSourceType(SourceTypeString);
		}

		Rule.MinDistanceCm = FMath::Max(0.0f, Rule.MinDistanceCm);
		Rule.MaxDistanceCm = FMath::Max(Rule.MinDistanceCm, Rule.MaxDistanceCm);
		Rule.MinViewDot = FMath::Clamp(Rule.MinViewDot, -1.0f, 1.0f);
		Rule.MinScreenAreaRatio = FMath::Clamp(Rule.MinScreenAreaRatio, 0.0f, 1.0f);
		Rule.CooldownSec = FMath::Max(0.0, Rule.CooldownSec);
		Rule.DedupeWindowSec = FMath::Max(0.0, Rule.DedupeWindowSec);
		Rule.TimeToLiveSec = FMath::Max(0.1f, Rule.TimeToLiveSec);

		IdleScanThoughtRules.Add(MoveTemp(Rule));
	}

	if (IdleScanThoughtRules.Num() == 0)
	{
		UE_LOG(LogProjectMindService, Warning, TEXT("No valid idle scan rules found in: %s"), *RulePath);
		BuildDefaultIdleScanThoughtRules();
		return;
	}

	UE_LOG(LogProjectMindService, Log, TEXT("Loaded %d idle scan rules from %s"),
		IdleScanThoughtRules.Num(), *RulePath);
}

void FMindServiceImpl::BuildDefaultIdleScanThoughtRules()
{
	IdleScanThoughtRules.Reset();

	auto BuildRule = [](const TCHAR* RuleId, const TArray<const TCHAR*>& Tags) -> FMindServiceImpl::FIdleScanThoughtRule
	{
		FMindServiceImpl::FIdleScanThoughtRule Rule;
		Rule.RuleId = FName(RuleId);
		for (const TCHAR* TagName : Tags)
		{
			const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(TagName), false);
			if (Tag.IsValid())
			{
				Rule.MatchAnyTags.Add(Tag);
			}
		}
		return Rule;
	};

	{
		FIdleScanThoughtRule Rule = BuildRule(TEXT("quest_item_hint"), { TEXT("Item.Type.Quest") });
		Rule.MinDistanceCm = 0.0f;
		Rule.MaxDistanceCm = 100000.0f;
		Rule.MinViewDot = 0.35f;
		Rule.MinScreenAreaRatio = 0.00006f;
		Rule.bRequireLineOfSight = true;
		Rule.ThoughtIdPrefix = TEXT("Mind.Scan.Quest");
		Rule.TextTemplate = TEXT("I should check {actor_label}. It may be important.");
		Rule.TextTemplates = {
			TEXT("I should check {actor_label}. It may be important."),
			TEXT("{actor_label} could matter for what I need."),
			TEXT("I should inspect {actor_label} before moving on.")
		};
		Rule.Channel = EMindThoughtChannel::ToastAndJournal;
		Rule.PriorityBase = 240;
		Rule.SourceType = EMindThoughtSourceType::Quest;
		Rule.CooldownSec = 30.0;
		Rule.DedupeWindowSec = 20.0;
		Rule.TimeToLiveSec = 8.0f;
		Rule.DedupeKeyPrefix = TEXT("Mind.Scan.Quest");
		if (Rule.MatchAnyTags.Num() > 0)
		{
			IdleScanThoughtRules.Add(MoveTemp(Rule));
		}
	}

	{
		FIdleScanThoughtRule Rule = BuildRule(TEXT("hydration_resource"), { TEXT("SetByCaller.Hydration") });
		Rule.MinDistanceCm = 0.0f;
		Rule.MaxDistanceCm = 550.0f;
		Rule.MinViewDot = 0.2f;
		Rule.MinScreenAreaRatio = 0.00012f;
		Rule.bRequireLineOfSight = true;
		Rule.ThoughtIdPrefix = TEXT("Mind.Scan.Hydration");
		Rule.TextTemplate = TEXT("{actor_label} can help with water.");
		Rule.TextTemplates = {
			TEXT("{actor_label} can help with water."),
			TEXT("I should pick up {actor_label} for hydration."),
			TEXT("{actor_label} might keep me hydrated.")
		};
		Rule.Channel = EMindThoughtChannel::Toast;
		Rule.PriorityBase = 220;
		Rule.SourceType = EMindThoughtSourceType::Scan;
		Rule.CooldownSec = 40.0;
		Rule.DedupeWindowSec = 30.0;
		Rule.TimeToLiveSec = 6.0f;
		Rule.DedupeKeyPrefix = TEXT("Mind.Scan.Hydration");
		if (Rule.MatchAnyTags.Num() > 0)
		{
			IdleScanThoughtRules.Add(MoveTemp(Rule));
		}
	}

	{
		FIdleScanThoughtRule Rule = BuildRule(TEXT("calories_resource"), { TEXT("SetByCaller.Calories") });
		Rule.MinDistanceCm = 0.0f;
		Rule.MaxDistanceCm = 550.0f;
		Rule.MinViewDot = 0.2f;
		Rule.MinScreenAreaRatio = 0.00012f;
		Rule.bRequireLineOfSight = true;
		Rule.ThoughtIdPrefix = TEXT("Mind.Scan.Calories");
		Rule.TextTemplate = TEXT("{actor_label} can help with food.");
		Rule.TextTemplates = {
			TEXT("{actor_label} can help with food."),
			TEXT("I should pick up {actor_label} for calories."),
			TEXT("{actor_label} may keep me going.")
		};
		Rule.Channel = EMindThoughtChannel::Toast;
		Rule.PriorityBase = 215;
		Rule.SourceType = EMindThoughtSourceType::Scan;
		Rule.CooldownSec = 40.0;
		Rule.DedupeWindowSec = 30.0;
		Rule.TimeToLiveSec = 6.0f;
		Rule.DedupeKeyPrefix = TEXT("Mind.Scan.Calories");
		if (Rule.MatchAnyTags.Num() > 0)
		{
			IdleScanThoughtRules.Add(MoveTemp(Rule));
		}
	}

	{
		FIdleScanThoughtRule Rule = BuildRule(TEXT("near_interactable"), { TEXT("World.Interactable"), TEXT("Item.Pickup") });
		Rule.MinDistanceCm = 0.0f;
		Rule.MaxDistanceCm = 500.0f;
		Rule.MinViewDot = 0.2f;
		Rule.MinScreenAreaRatio = 0.00010f;
		Rule.bRequireLineOfSight = true;
		Rule.ThoughtIdPrefix = TEXT("Mind.Scan.Near");
		Rule.TextTemplate = TEXT("I can use {actor_label} nearby.");
		Rule.TextTemplates = {
			TEXT("I can use {actor_label} nearby."),
			TEXT("I should check {actor_label} nearby."),
			TEXT("{actor_label} looks useful right now.")
		};
		Rule.Channel = EMindThoughtChannel::Toast;
		Rule.PriorityBase = 180;
		Rule.SourceType = EMindThoughtSourceType::Scan;
		Rule.CooldownSec = 12.0;
		Rule.DedupeWindowSec = 10.0;
		Rule.TimeToLiveSec = 6.0f;
		Rule.DedupeKeyPrefix = TEXT("Mind.Scan.Near");
		if (Rule.MatchAnyTags.Num() > 0)
		{
			IdleScanThoughtRules.Add(MoveTemp(Rule));
		}
	}

	{
		FIdleScanThoughtRule Rule = BuildRule(TEXT("poi_generic"), { TEXT("World.POI") });
		Rule.MinDistanceCm = 500.0f;
		Rule.MaxDistanceCm = 100000.0f;
		Rule.MinViewDot = 0.30f;
		Rule.MinScreenAreaRatio = 0.00020f;
		Rule.bRequireLineOfSight = true;
		Rule.ThoughtIdPrefix = TEXT("Mind.Scan.POI");
		Rule.TextTemplate = TEXT("That place could be useful: {actor_label}.");
		Rule.TextTemplates = {
			TEXT("That place could be useful: {actor_label}."),
			TEXT("{actor_label} may be worth investigating."),
			TEXT("I should keep {actor_label} in mind.")
		};
		Rule.Channel = EMindThoughtChannel::ToastAndJournal;
		Rule.PriorityBase = 130;
		Rule.SourceType = EMindThoughtSourceType::Beacon;
		Rule.CooldownSec = 25.0;
		Rule.DedupeWindowSec = 18.0;
		Rule.TimeToLiveSec = 7.0f;
		Rule.DedupeKeyPrefix = TEXT("Mind.Scan.POI");
		if (Rule.MatchAnyTags.Num() > 0)
		{
			IdleScanThoughtRules.Add(MoveTemp(Rule));
		}
	}

	UE_LOG(LogProjectMindService, Log, TEXT("Using %d built-in idle scan rules"), IdleScanThoughtRules.Num());
}

bool FMindServiceImpl::TryBindDialogue(float /*DeltaTime*/)
{
	if (DialogueSignalHandle.IsValid())
	{
		return false;
	}

	const TSharedPtr<IDialogueService> DialogueService = FProjectServiceLocator::Resolve<IDialogueService>();
	if (!DialogueService.IsValid())
	{
		return true;
	}

	DialogueSignalHandle = DialogueService->OnDialogueSignal().AddRaw(
		this, &FMindServiceImpl::HandleDialogueSignal);

	// Catch current state immediately in case dialogue was already active before binding.
	HandleDialogueStateChanged();
	return false;
}

void FMindServiceImpl::UnbindDialogue()
{
	if (!DialogueSignalHandle.IsValid())
	{
		return;
	}

	const TSharedPtr<IDialogueService> DialogueService = FProjectServiceLocator::Resolve<IDialogueService>();
	if (DialogueService.IsValid())
	{
		DialogueService->OnDialogueSignal().Remove(DialogueSignalHandle);
	}

	DialogueSignalHandle.Reset();
}

void FMindServiceImpl::BindThoughtSources()
{
	const TSharedPtr<IMindVitalsThoughtSource> ResolvedVitalsSource = FProjectServiceLocator::Resolve<IMindVitalsThoughtSource>();
	if (VitalsThoughtSource != ResolvedVitalsSource)
	{
		if (VitalsThoughtSource.IsValid() && VitalsThoughtSourceChangedHandle.IsValid())
		{
			VitalsThoughtSource->OnThoughtSourceChanged().Remove(VitalsThoughtSourceChangedHandle);
			VitalsThoughtSourceChangedHandle.Reset();
		}

		VitalsThoughtSource = ResolvedVitalsSource;
		if (VitalsThoughtSource.IsValid())
		{
			VitalsThoughtSourceChangedHandle = VitalsThoughtSource->OnThoughtSourceChanged().AddRaw(
				this, &FMindServiceImpl::HandleThoughtSourceChanged);
		}
	}

	const TSharedPtr<IMindInventoryThoughtSource> ResolvedInventorySource = FProjectServiceLocator::Resolve<IMindInventoryThoughtSource>();
	if (InventoryThoughtSource != ResolvedInventorySource)
	{
		if (InventoryThoughtSource.IsValid() && InventoryThoughtSourceChangedHandle.IsValid())
		{
			InventoryThoughtSource->OnThoughtSourceChanged().Remove(InventoryThoughtSourceChangedHandle);
			InventoryThoughtSourceChangedHandle.Reset();
		}

		InventoryThoughtSource = ResolvedInventorySource;
		if (InventoryThoughtSource.IsValid())
		{
			InventoryThoughtSourceChangedHandle = InventoryThoughtSource->OnThoughtSourceChanged().AddRaw(
				this, &FMindServiceImpl::HandleThoughtSourceChanged);
		}
	}

	const TSharedPtr<IMindQuestThoughtSource> ResolvedQuestSource = FProjectServiceLocator::Resolve<IMindQuestThoughtSource>();
	if (QuestThoughtSource != ResolvedQuestSource)
	{
		if (QuestThoughtSource.IsValid() && QuestThoughtSourceChangedHandle.IsValid())
		{
			QuestThoughtSource->OnThoughtSourceChanged().Remove(QuestThoughtSourceChangedHandle);
			QuestThoughtSourceChangedHandle.Reset();
		}

		QuestThoughtSource = ResolvedQuestSource;
		if (QuestThoughtSource.IsValid())
		{
			QuestThoughtSourceChangedHandle = QuestThoughtSource->OnThoughtSourceChanged().AddRaw(
				this, &FMindServiceImpl::HandleThoughtSourceChanged);
		}
	}
}

void FMindServiceImpl::UnbindThoughtSources()
{
	if (VitalsThoughtSource.IsValid() && VitalsThoughtSourceChangedHandle.IsValid())
	{
		VitalsThoughtSource->OnThoughtSourceChanged().Remove(VitalsThoughtSourceChangedHandle);
		VitalsThoughtSourceChangedHandle.Reset();
	}

	if (InventoryThoughtSource.IsValid() && InventoryThoughtSourceChangedHandle.IsValid())
	{
		InventoryThoughtSource->OnThoughtSourceChanged().Remove(InventoryThoughtSourceChangedHandle);
		InventoryThoughtSourceChangedHandle.Reset();
	}

	if (QuestThoughtSource.IsValid() && QuestThoughtSourceChangedHandle.IsValid())
	{
		QuestThoughtSource->OnThoughtSourceChanged().Remove(QuestThoughtSourceChangedHandle);
		QuestThoughtSourceChangedHandle.Reset();
	}

	VitalsThoughtSource.Reset();
	InventoryThoughtSource.Reset();
	QuestThoughtSource.Reset();
}

void FMindServiceImpl::HandleThoughtSourceChanged()
{
	EvaluateProviderPipeline(false);
}

void FMindServiceImpl::HandleDialogueStateChanged()
{
	const TSharedPtr<IDialogueService> DialogueService = FProjectServiceLocator::Resolve<IDialogueService>();
	if (!DialogueService.IsValid() || !DialogueService->IsDialogueActive())
	{
		return;
	}

	const FName SignalTag = BuildDialogueSignalTag(
		DialogueService->GetCurrentTreeId(),
		DialogueService->GetCurrentNodeId());
	HandleDialogueSignal(SignalTag);
}

void FMindServiceImpl::HandleDialogueSignal(const FName& SignalTag)
{
	if (SignalTag.IsNone())
	{
		return;
	}

	const FDialogueThoughtMappingEntry* MappingEntry = DialogueThoughtMappingsBySignal.Find(SignalTag);
	if (!MappingEntry)
	{
		UE_LOG(LogProjectMindService, Verbose, TEXT("Dialogue signal ignored (no mapping): %s"), *SignalTag.ToString());
		return;
	}

	UE_LOG(LogProjectMindService, Verbose, TEXT("Dialogue signal matched mapping: %s"), *SignalTag.ToString());
	EmitThought(*MappingEntry);
}

void FMindServiceImpl::EvaluateProviderPipeline(bool bIncludeIdleScanCandidates)
{
	BindThoughtSources();

	TArray<FMindThoughtTemplate> CandidateThoughts;
	CollectThoughtsFromConfiguredSources(CandidateThoughts);
	if (bIncludeIdleScanCandidates)
	{
		CollectIdleScanThoughts(CandidateThoughts);
	}

	if (CandidateThoughts.Num() == 0)
	{
		return;
	}

	Algo::Sort(CandidateThoughts, [](const FMindThoughtTemplate& A, const FMindThoughtTemplate& B)
	{
		return A.Priority > B.Priority;
	});

	for (const FMindThoughtTemplate& Candidate : CandidateThoughts)
	{
		if (TryEmitThought(Candidate))
		{
			break;
		}
	}
}

void FMindServiceImpl::CollectThoughtsFromConfiguredSources(TArray<FMindThoughtTemplate>& OutThoughts) const
{
	FMindThoughtSourceContext SourceContext;
	SourceContext.LocalPlayerPawn = LocalPlayerPawn;

	auto CollectFromSource = [&OutThoughts, &SourceContext](const TSharedPtr<IMindThoughtSource>& Source)
	{
		if (!Source.IsValid())
		{
			return;
		}

		TArray<FMindThoughtCandidate> SourceCandidates;
		Source->CollectThoughts(SourceContext, SourceCandidates);
		for (const FMindThoughtCandidate& Candidate : SourceCandidates)
		{
			OutThoughts.Add(FMindServiceImpl::ConvertCandidateToTemplate(Candidate));
		}
	};

	CollectFromSource(StaticCastSharedPtr<IMindThoughtSource>(VitalsThoughtSource));
	CollectFromSource(StaticCastSharedPtr<IMindThoughtSource>(InventoryThoughtSource));
	CollectFromSource(StaticCastSharedPtr<IMindThoughtSource>(QuestThoughtSource));
}

void FMindServiceImpl::CollectIdleScanThoughts(TArray<FMindThoughtTemplate>& OutThoughts) const
{
	if (IdleScanThoughtRules.Num() == 0)
	{
		return;
	}

	APawn* Pawn = LocalPlayerPawn.Get();
	if (!Pawn)
	{
		return;
	}

	UWorld* World = Pawn->GetWorld();
	if (!World)
	{
		return;
	}

	FVector ViewLocation = Pawn->GetActorLocation();
	FVector ViewDirection = Pawn->GetActorForwardVector();
	const APlayerController* PlayerController = Cast<APlayerController>(Pawn->GetController());
	if (PlayerController)
	{
		FRotator ViewRotation = Pawn->GetActorRotation();
		PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
		ViewDirection = ViewRotation.Vector();
	}

	float MaxRuleDistanceCm = 0.0f;
	for (const FIdleScanThoughtRule& Rule : IdleScanThoughtRules)
	{
		MaxRuleDistanceCm = FMath::Max(MaxRuleDistanceCm, Rule.MaxDistanceCm);
	}

	const double NowSec = FPlatformTime::Seconds();
	TMap<FPrimaryAssetId, FItemDataView> PerScanItemDataCache;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || Actor == Pawn || Actor->IsHidden() || Actor->IsActorBeingDestroyed())
		{
			continue;
		}

		const FVector ToActor = Actor->GetActorLocation() - ViewLocation;
		const float DistanceCm = ToActor.Size();
		if (DistanceCm <= KINDA_SMALL_NUMBER || DistanceCm > MaxRuleDistanceCm)
		{
			continue;
		}

		const FVector DirectionToActor = ToActor / DistanceCm;
		const float ViewDot = FVector::DotProduct(ViewDirection, DirectionToActor);

		TArray<FGameplayTag> ActorGameplayTags;
		FItemDataView PickupItemData;
		CollectActorGameplayTags(*Actor, PerScanItemDataCache, ActorGameplayTags, &PickupItemData);

		if (ActorGameplayTags.Num() == 0)
		{
			continue;
		}

		FString ActorLabel = Actor->GetActorNameOrLabel();
		if (PickupItemData.bIsValid && !PickupItemData.DisplayName.IsEmpty())
		{
			ActorLabel = PickupItemData.DisplayName.ToString();
		}
		const FString ActorIdSuffix = Actor->GetFName().ToString();
		float ScreenAreaRatio = 0.0f;
		const bool bHasScreenAreaRatio = TryComputeActorScreenAreaRatio(
			*Actor,
			ViewLocation,
			PlayerController,
			ScreenAreaRatio);

		bool bLineOfSightEvaluated = false;
		bool bHasLineOfSight = false;
		FMindThoughtTemplate BestCandidate;
		bool bHasBestCandidate = false;

		for (const FIdleScanThoughtRule& Rule : IdleScanThoughtRules)
		{
			if (DistanceCm < Rule.MinDistanceCm || DistanceCm > Rule.MaxDistanceCm)
			{
				continue;
			}

			if (ViewDot < Rule.MinViewDot)
			{
				continue;
			}

			if (Rule.MinScreenAreaRatio > 0.0f
				&& bHasScreenAreaRatio
				&& ScreenAreaRatio < Rule.MinScreenAreaRatio)
			{
				continue;
			}

			bool bTagMatched = false;
			for (const FGameplayTag& RuleTag : Rule.MatchAnyTags)
			{
				for (const FGameplayTag& ActorTag : ActorGameplayTags)
				{
					if (ActorTag.MatchesTag(RuleTag))
					{
						bTagMatched = true;
						break;
					}
				}

				if (bTagMatched)
				{
					break;
				}
			}

			if (!bTagMatched)
			{
				continue;
			}

			if (Rule.bRequireLineOfSight)
			{
				if (!bLineOfSightEvaluated)
				{
					FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ProjectMindIdleScan), true, Pawn);
					QueryParams.AddIgnoredActor(Pawn);

					FHitResult Hit;
					const bool bHit = World->LineTraceSingleByChannel(
						Hit,
						ViewLocation,
						Actor->GetActorLocation(),
						ECC_Visibility,
						QueryParams);

					bHasLineOfSight = !bHit || Hit.GetActor() == Actor;
					bLineOfSightEvaluated = true;
				}

				// Strict rule: LOS-required hints must not pass through occluders.
				if (!bHasLineOfSight)
				{
					continue;
				}
			}

			FMindThoughtTemplate Candidate;
			Candidate.ThoughtId = FName(*FString::Printf(TEXT("%s.%s"), *Rule.ThoughtIdPrefix, *ActorIdSuffix));

			FFormatNamedArguments TextArguments;
			TextArguments.Add(TEXT("actor_label"), FText::FromString(ActorLabel));
			const FString TemplateText = SelectScanThoughtTextTemplate(
				Rule.TextTemplates,
				Rule.RuleId,
				ActorIdSuffix,
				Rule.CooldownSec,
				NowSec);
			const FString EffectiveTemplate = TemplateText.IsEmpty() ? Rule.TextTemplate : TemplateText;
			Candidate.ThoughtText = FText::Format(FText::FromString(EffectiveTemplate), TextArguments);

			Candidate.Channel = Rule.Channel;
			Candidate.SourceType = Rule.SourceType;
			Candidate.CooldownSec = Rule.CooldownSec;
			Candidate.DedupeWindowSec = Rule.DedupeWindowSec;
			Candidate.TimeToLiveSec = Rule.TimeToLiveSec;
			if (!Rule.DedupeKeyPrefix.IsEmpty())
			{
				Candidate.DedupeKey = FName(*FString::Printf(TEXT("%s.%s"), *Rule.DedupeKeyPrefix, *ActorIdSuffix));
			}

			const float RuleMaxDistance = FMath::Max(1.0f, Rule.MaxDistanceCm);
			const float DistanceScore = FMath::Clamp((1.0f - (DistanceCm / RuleMaxDistance)) * 6.0f, 0.0f, 6.0f);
			const float DirectionScore = FMath::Clamp(ViewDot * 20.0f, 0.0f, 20.0f);
			const float ScreenAreaScore = bHasScreenAreaRatio
				? FMath::Clamp(ScreenAreaRatio * 50000.0f, 0.0f, 35.0f)
				: 0.0f;
			Candidate.Priority = Rule.PriorityBase + static_cast<int32>(DistanceScore + DirectionScore + ScreenAreaScore);

			// Shared importance policy for world notice thoughts:
			// if final signal is strong enough, keep it in durable journal as well.
			if ((Candidate.SourceType == EMindThoughtSourceType::Quest
				|| Candidate.SourceType == EMindThoughtSourceType::Beacon)
				&& Candidate.Channel == EMindThoughtChannel::Toast
				&& Candidate.Priority >= ScanPriorityForImportantJournal)
			{
				Candidate.Channel = EMindThoughtChannel::ToastAndJournal;
			}

			if (!bHasBestCandidate || Candidate.Priority > BestCandidate.Priority)
			{
				BestCandidate = MoveTemp(Candidate);
				bHasBestCandidate = true;
			}
		}

		if (bHasBestCandidate)
		{
			OutThoughts.Add(MoveTemp(BestCandidate));
		}
	}
}

void FMindServiceImpl::RearmIdleScan()
{
	UWorld* RuntimeWorld = ResolveRuntimeWorld();
	if (!RuntimeWorld || !LocalPlayerPawn.IsValid())
	{
		return;
	}

	FTimerManager& TimerManager = RuntimeWorld->GetTimerManager();
	TimerManager.ClearTimer(IdleScanTimerHandle);
	TimerManager.SetTimer(
		IdleScanTimerHandle,
		FTimerDelegate::CreateRaw(this, &FMindServiceImpl::HandleIdleScanTriggered),
		IdleScanDelaySec,
		false);
}

void FMindServiceImpl::CancelIdleScan()
{
	if (UWorld* RuntimeWorld = ResolveRuntimeWorld())
	{
		RuntimeWorld->GetTimerManager().ClearTimer(IdleScanTimerHandle);
	}

	IdleScanTimerHandle.Invalidate();
}

void FMindServiceImpl::HandleIdleScanTriggered()
{
	IdleScanTimerHandle.Invalidate();

	TArray<FMindThoughtTemplate> ScanCandidates;
	CollectIdleScanThoughts(ScanCandidates);

	bool bHydrationResourceSeen = false;
	bool bCaloriesResourceSeen = false;
	for (const FMindThoughtTemplate& Candidate : ScanCandidates)
	{
		if (IsHydrationScanThoughtId(Candidate.ThoughtId))
		{
			bHydrationResourceSeen = true;
		}

		if (IsCaloriesScanThoughtId(Candidate.ThoughtId))
		{
			bCaloriesResourceSeen = true;
		}

		if (bHydrationResourceSeen && bCaloriesResourceSeen)
		{
			break;
		}
	}

	const double NowSec = FPlatformTime::Seconds();
	if (bHydrationResourceSeen)
	{
		LastHydrationResourceSeenSec = NowSec;
	}

	if (bCaloriesResourceSeen)
	{
		LastCaloriesResourceSeenSec = NowSec;
	}

	bool bScanThoughtEmitted = false;
	if (ScanCandidates.Num() > 0)
	{
		Algo::Sort(ScanCandidates, [](const FMindThoughtTemplate& A, const FMindThoughtTemplate& B)
		{
			return A.Priority > B.Priority;
		});

		for (const FMindThoughtTemplate& Candidate : ScanCandidates)
		{
			if (TryEmitThought(Candidate))
			{
				bScanThoughtEmitted = true;
				break;
			}
		}
	}

	UE_LOG(
		LogProjectMindService,
		Log,
		TEXT("Idle scan evaluated: Candidates=%d HydrationSeen=%s CaloriesSeen=%s EmittedScan=%s"),
		ScanCandidates.Num(),
		bHydrationResourceSeen ? TEXT("true") : TEXT("false"),
		bCaloriesResourceSeen ? TEXT("true") : TEXT("false"),
		bScanThoughtEmitted ? TEXT("true") : TEXT("false"));

	if (bScanThoughtEmitted)
	{
		return;
	}

	if (bHydrationResourceSeen || bCaloriesResourceSeen)
	{
		UE_LOG(
			LogProjectMindService,
			Log,
			TEXT("Idle scan detected nearby resources; deferring vitals reminders (hydration %.1fs, calories %.1fs)"),
			HydrationReminderSuppressAfterResourceSec,
			CaloriesReminderSuppressAfterResourceSec);
		return;
	}

	EvaluateProviderPipeline(false);
}

UWorld* FMindServiceImpl::ResolveRuntimeWorld() const
{
	if (APawn* Pawn = LocalPlayerPawn.Get())
	{
		return Pawn->GetWorld();
	}

	if (!GEngine)
	{
		return nullptr;
	}

	for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
	{
		UWorld* World = WorldContext.World();
		if (!World)
		{
			continue;
		}

		if (WorldContext.WorldType == EWorldType::Game || WorldContext.WorldType == EWorldType::PIE)
		{
			return World;
		}
	}

	return nullptr;
}

FMindServiceImpl::FMindThoughtTemplate FMindServiceImpl::ConvertCandidateToTemplate(const FMindThoughtCandidate& Candidate)
{
	FMindThoughtTemplate Converted;
	Converted.ThoughtId = Candidate.ThoughtId;
	Converted.ThoughtText = Candidate.ThoughtText;
	Converted.TimeToLiveSec = Candidate.TimeToLiveSec;
	Converted.CooldownSec = Candidate.CooldownSec;
	Converted.DedupeWindowSec = Candidate.DedupeWindowSec;
	Converted.DedupeKey = Candidate.DedupeKey;
	Converted.Channel = Candidate.Channel;
	Converted.Priority = Candidate.Priority;
	Converted.SourceType = Candidate.SourceType;
	Converted.RecordId = Candidate.RecordId;
	Converted.RecordState = Candidate.RecordState;
	return Converted;
}

void FMindServiceImpl::EmitThought(const FDialogueThoughtMappingEntry& MappingEntry)
{
	TryEmitThought(MappingEntry.ThoughtTemplate);
}

bool FMindServiceImpl::TryEmitThought(const FMindThoughtTemplate& ThoughtTemplate)
{
	const double NowSec = FPlatformTime::Seconds();
	if (!CanEmitThoughtNow(ThoughtTemplate, NowSec))
	{
		return false;
	}

	LastEmitTimeByThought.Add(ThoughtTemplate.ThoughtId, NowSec);

	const FName DedupeKey = ResolveDedupeKey(ThoughtTemplate);
	if (!DedupeKey.IsNone() && ThoughtTemplate.DedupeWindowSec > 0.0)
	{
		LastEmitTimeByDedupeKey.Add(DedupeKey, NowSec);
	}

	LastGlobalEmitTimeSec = NowSec;

	FMindThoughtView Thought;
	Thought.ThoughtId = ThoughtTemplate.ThoughtId;
	Thought.Channel = ThoughtTemplate.Channel;
	Thought.Priority = ThoughtTemplate.Priority;
	Thought.SourceType = ThoughtTemplate.SourceType;
	Thought.RecordId = ThoughtTemplate.RecordId;
	Thought.RecordState = ThoughtTemplate.RecordState;
	Thought.Text = ThoughtTemplate.ThoughtText;
	Thought.TimeToLiveSec = ThoughtTemplate.TimeToLiveSec;
	Thought.CreatedAtUtc = FDateTime::UtcNow();
	Thought.CreatedAtSec = static_cast<float>(NowSec);

	ThoughtHistory.Add(Thought);
	if (ThoughtHistory.Num() > MaxThoughtHistory)
	{
		const int32 NumToRemove = ThoughtHistory.Num() - MaxThoughtHistory;
		ThoughtHistory.RemoveAt(0, NumToRemove, EAllowShrinking::No);
	}

	ThoughtAddedDelegate.Broadcast(Thought);
	FeedChangedDelegate.Broadcast();

	UE_LOG(
		LogProjectMindService,
		Log,
		TEXT("Mind thought emitted: Id=%s Source=%s Priority=%d RecordId=%s RecordState=%s Text=\"%s\""),
		*Thought.ThoughtId.ToString(),
		*StaticEnum<EMindThoughtSourceType>()->GetNameStringByValue(static_cast<int64>(Thought.SourceType)),
		Thought.Priority,
		*Thought.RecordId.ToString(),
		*StaticEnum<EMindRecordState>()->GetNameStringByValue(static_cast<int64>(Thought.RecordState)),
		*Thought.Text.ToString());

	return true;
}

bool FMindServiceImpl::CanEmitThoughtNow(const FMindThoughtTemplate& ThoughtTemplate, double NowSec) const
{
	if (ThoughtTemplate.ThoughtId.IsNone() || ThoughtTemplate.ThoughtText.IsEmpty())
	{
		return false;
	}

	if (ThoughtTemplate.SourceType == EMindThoughtSourceType::Vitals
		&& IsHydrationVitalsThoughtId(ThoughtTemplate.ThoughtId))
	{
		const double SinceHydrationResourceSeenSec = NowSec - LastHydrationResourceSeenSec;
		if (SinceHydrationResourceSeenSec >= 0.0
			&& SinceHydrationResourceSeenSec < HydrationReminderSuppressAfterResourceSec)
		{
			return false;
		}
	}

	if (ThoughtTemplate.SourceType == EMindThoughtSourceType::Vitals
		&& IsCaloriesVitalsThoughtId(ThoughtTemplate.ThoughtId))
	{
		const double SinceCaloriesResourceSeenSec = NowSec - LastCaloriesResourceSeenSec;
		if (SinceCaloriesResourceSeenSec >= 0.0
			&& SinceCaloriesResourceSeenSec < CaloriesReminderSuppressAfterResourceSec)
		{
			return false;
		}
	}

	if (const double* LastEmitSec = LastEmitTimeByThought.Find(ThoughtTemplate.ThoughtId))
	{
		const double SinceLastSec = NowSec - *LastEmitSec;
		if (SinceLastSec < ThoughtTemplate.CooldownSec)
		{
			return false;
		}
	}

	const FName DedupeKey = ResolveDedupeKey(ThoughtTemplate);
	if (!DedupeKey.IsNone() && ThoughtTemplate.DedupeWindowSec > 0.0)
	{
		if (const double* LastDedupeEmitSec = LastEmitTimeByDedupeKey.Find(DedupeKey))
		{
			const double SinceLastDedupeSec = NowSec - *LastDedupeEmitSec;
			if (SinceLastDedupeSec < ThoughtTemplate.DedupeWindowSec)
			{
				return false;
			}
		}
	}

	if ((NowSec - LastGlobalEmitTimeSec) < GlobalEmitCooldownSec)
	{
		return false;
	}

	return true;
}

FName FMindServiceImpl::ResolveDedupeKey(const FMindThoughtTemplate& ThoughtTemplate) const
{
	return ThoughtTemplate.DedupeKey.IsNone()
		? ThoughtTemplate.ThoughtId
		: ThoughtTemplate.DedupeKey;
}

FString FMindServiceImpl::ResolveDialogueMappingPath() const
{
	const FString MappingPathToUse = DialogueMappingPath.IsEmpty()
		? FString(DefaultDialogueThoughtMappingRelativePath)
		: DialogueMappingPath;

	if (FPaths::IsRelative(MappingPathToUse))
	{
		return FPaths::ProjectContentDir() / MappingPathToUse;
	}

	return MappingPathToUse;
}

FString FMindServiceImpl::ResolveIdleScanRulePath() const
{
	const FString MappingPathToUse = IdleScanRulePath.IsEmpty()
		? FString(DefaultIdleScanRulesRelativePath)
		: IdleScanRulePath;
	if (FPaths::IsRelative(MappingPathToUse))
	{
		return FPaths::ProjectContentDir() / MappingPathToUse;
	}

	return MappingPathToUse;
}
