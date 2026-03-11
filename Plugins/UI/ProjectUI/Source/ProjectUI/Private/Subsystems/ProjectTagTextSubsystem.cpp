// Copyright ALIS. All Rights Reserved.

#include "Subsystems/ProjectTagTextSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogTagText, Log, All);

bool UProjectTagTextSubsystem::Register(FGameplayTag Tag, FText Text)
{
	if (!Tag.IsValid())
	{
		UE_LOG(LogTagText, Warning, TEXT("Register: Invalid tag, ignoring"));
		return false;
	}

	if (TagTextMap.Contains(Tag))
	{
		UE_LOG(LogTagText, Warning, TEXT("Register: Tag '%s' already registered, ignoring duplicate"), *Tag.ToString());
		return false;
	}

	TagTextMap.Add(Tag, Text);
	UE_LOG(LogTagText, Verbose, TEXT("Register: '%s' -> '%s'"), *Tag.ToString(), *Text.ToString());
	return true;
}

bool UProjectTagTextSubsystem::RegisterOrReplace(FGameplayTag Tag, FText Text)
{
	if (!Tag.IsValid())
	{
		UE_LOG(LogTagText, Warning, TEXT("RegisterOrReplace: Invalid tag, ignoring"));
		return false;
	}

	const bool bReplaced = TagTextMap.Contains(Tag);
	TagTextMap.Add(Tag, Text);

	if (bReplaced)
	{
		UE_LOG(LogTagText, Verbose, TEXT("RegisterOrReplace: '%s' -> '%s' (replaced)"), *Tag.ToString(), *Text.ToString());
	}
	else
	{
		UE_LOG(LogTagText, Verbose, TEXT("RegisterOrReplace: '%s' -> '%s'"), *Tag.ToString(), *Text.ToString());
	}

	return true;
}

bool UProjectTagTextSubsystem::TryGet(FGameplayTag Tag, FText& OutText) const
{
	if (const FText* Found = TagTextMap.Find(Tag))
	{
		OutText = *Found;
		return true;
	}
	return false;
}

FText UProjectTagTextSubsystem::Get(FGameplayTag Tag) const
{
	FText Result;
	TryGet(Tag, Result);
	return Result;
}

bool UProjectTagTextSubsystem::HasText(FGameplayTag Tag) const
{
	return TagTextMap.Contains(Tag);
}

TArray<FText> UProjectTagTextSubsystem::GetTextsForTags(const FGameplayTagContainer& Tags, FGameplayTag ParentTag) const
{
	TArray<FText> Result;
	TArray<TPair<FString, FText>> SortedEntries;
	SortedEntries.Reserve(Tags.Num());

	for (const FGameplayTag& Tag : Tags)
	{
		// If parent filter specified, skip tags that don't match
		if (ParentTag.IsValid() && !Tag.MatchesTag(ParentTag))
		{
			continue;
		}

		FText Text;
		if (TryGet(Tag, Text))
		{
			SortedEntries.Emplace(Tag.ToString(), Text);
		}
	}

	// Stable order by tag string to avoid UI shuffling.
	SortedEntries.Sort([](const TPair<FString, FText>& A, const TPair<FString, FText>& B)
	{
		return A.Key.Compare(B.Key, ESearchCase::IgnoreCase) < 0;
	});

	Result.Reserve(SortedEntries.Num());
	for (const TPair<FString, FText>& Entry : SortedEntries)
	{
		Result.Add(Entry.Value);
	}

	return Result;
}
