// Copyright ALIS. All Rights Reserved.

#include "Services/DefaultVitalsMindThoughtSource.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Pawn.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
const TCHAR* DefaultVitalsThoughtMappingRelativePath = TEXT("Data/Schema/Gameplay/ProjectMind/vitals_thought_mappings.json");
}

FDefaultVitalsMindThoughtSource::FDefaultVitalsMindThoughtSource()
{
	LoadMappings();
}

void FDefaultVitalsMindThoughtSource::CollectThoughts(
	const FMindThoughtSourceContext& Context,
	TArray<FMindThoughtCandidate>& OutThoughts) const
{
	APawn* ContextPawn = Context.LocalPlayerPawn.Get();
	if (!ContextPawn)
	{
		return;
	}

	UAbilitySystemComponent* ASC = ContextPawn->FindComponentByClass<UAbilitySystemComponent>();
	if (!ASC)
	{
		return;
	}

	for (const FStateThoughtMapping& Mapping : Mappings)
	{
		if (Mapping.StateTag.IsValid() && ASC->HasMatchingGameplayTag(Mapping.StateTag))
		{
			OutThoughts.Add(Mapping.Thought);
		}
	}
}

FOnMindThoughtSourceChanged& FDefaultVitalsMindThoughtSource::OnThoughtSourceChanged()
{
	return SourceChangedDelegate;
}

void FDefaultVitalsMindThoughtSource::LoadMappings()
{
	Mappings.Reset();

	const FString MappingPath = ResolveMappingPath();
	FString JsonContent;
	if (!FFileHelper::LoadFileToString(JsonContent, *MappingPath))
	{
		return;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonContent);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("entries"), Entries) || !Entries)
	{
		return;
	}

	for (const TSharedPtr<FJsonValue>& EntryValue : *Entries)
	{
		const TSharedPtr<FJsonObject> EntryObject = EntryValue.IsValid() ? EntryValue->AsObject() : nullptr;
		if (!EntryObject.IsValid())
		{
			continue;
		}

		FString StateTagString;
		FString ThoughtIdString;
		FString ThoughtTextString;
		if (!EntryObject->TryGetStringField(TEXT("state_tag"), StateTagString)
			|| !EntryObject->TryGetStringField(TEXT("thought_id"), ThoughtIdString)
			|| !EntryObject->TryGetStringField(TEXT("text"), ThoughtTextString))
		{
			continue;
		}

		FStateThoughtMapping Mapping;
		Mapping.StateTag = FGameplayTag::RequestGameplayTag(FName(*StateTagString), false);
		if (!Mapping.StateTag.IsValid())
		{
			continue;
		}

		Mapping.Thought.ThoughtId = FName(*ThoughtIdString);
		Mapping.Thought.ThoughtText = FText::FromString(ThoughtTextString);
		Mapping.Thought.SourceType = EMindThoughtSourceType::Vitals;
		Mapping.Thought.Channel = EMindThoughtChannel::Toast;

		EntryObject->TryGetNumberField(TEXT("time_to_live_sec"), Mapping.Thought.TimeToLiveSec);
		EntryObject->TryGetNumberField(TEXT("cooldown_sec"), Mapping.Thought.CooldownSec);
		EntryObject->TryGetNumberField(TEXT("dedupe_window_sec"), Mapping.Thought.DedupeWindowSec);
		Mapping.Thought.DedupeKey = ParseOptionalNameField(EntryObject, TEXT("dedupe_key"));

		double PriorityValue = static_cast<double>(Mapping.Thought.Priority);
		if (EntryObject->TryGetNumberField(TEXT("priority"), PriorityValue))
		{
			Mapping.Thought.Priority = static_cast<int32>(PriorityValue);
		}

		FString ChannelString;
		if (EntryObject->TryGetStringField(TEXT("channel"), ChannelString))
		{
			Mapping.Thought.Channel = ParseMindThoughtChannel(ChannelString);
		}

		FString SourceTypeString;
		if (EntryObject->TryGetStringField(TEXT("source_type"), SourceTypeString))
		{
			Mapping.Thought.SourceType = ParseMindThoughtSourceType(SourceTypeString);
		}

		if (Mapping.Thought.ThoughtId.IsNone() || Mapping.Thought.ThoughtText.IsEmpty())
		{
			continue;
		}

		Mappings.Add(Mapping);
	}
}

FString FDefaultVitalsMindThoughtSource::ResolveMappingPath() const
{
	const FString MappingPathToUse = FString(DefaultVitalsThoughtMappingRelativePath);
	if (FPaths::IsRelative(MappingPathToUse))
	{
		return FPaths::ProjectContentDir() / MappingPathToUse;
	}

	return MappingPathToUse;
}

EMindThoughtChannel FDefaultVitalsMindThoughtSource::ParseMindThoughtChannel(const FString& ChannelString)
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

EMindThoughtSourceType FDefaultVitalsMindThoughtSource::ParseMindThoughtSourceType(const FString& SourceTypeString)
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

FName FDefaultVitalsMindThoughtSource::ParseOptionalNameField(const TSharedPtr<FJsonObject>& EntryObject, const TCHAR* FieldName)
{
	FString ValueString;
	if (!EntryObject->TryGetStringField(FieldName, ValueString) || ValueString.IsEmpty())
	{
		return NAME_None;
	}

	return FName(*ValueString);
}
