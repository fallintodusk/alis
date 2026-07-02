// Copyright ALIS. All Rights Reserved.

#include "Data/LootProfileDefinition.h"

namespace
{
int32 ResolveLootProfilePickCount(const ULootProfileDefinition& Profile, FRandomStream& RandomStream)
{
	const int32 MaxPicks = FMath::Max(Profile.PickCountMin, Profile.PickCountMax);
	if (MaxPicks <= 0)
	{
		return 0;
	}

	const int32 MinPicks = FMath::Clamp(Profile.PickCountMin, 0, MaxPicks);
	if (MinPicks >= MaxPicks)
	{
		return MaxPicks;
	}

	return RandomStream.RandRange(MinPicks, MaxPicks);
}
}

void ULootProfileDefinition::BuildLootEntries(TArray<FLootEntryView>& OutEntries, FRandomStream* RandomStream) const
{
	if (!HasEntries())
	{
		return;
	}

	TArray<const FLootProfileEntry*> ValidEntries;
	ValidEntries.Reserve(Entries.Num());
	for (const FLootProfileEntry& Entry : Entries)
	{
		if (Entry.IsValid())
		{
			ValidEntries.Add(&Entry);
		}
	}

	if (ValidEntries.Num() <= 0)
	{
		return;
	}

	FRandomStream LocalRandomStream;
	if (!RandomStream)
	{
		LocalRandomStream.Initialize(FMath::Rand());
		RandomStream = &LocalRandomStream;
	}

	const int32 DesiredPicks = FMath::Clamp(
		ResolveLootProfilePickCount(*this, *RandomStream),
		0,
		ValidEntries.Num());

	OutEntries.Reserve(OutEntries.Num() + DesiredPicks);
	for (int32 PickIndex = 0; PickIndex < DesiredPicks; ++PickIndex)
	{
		const int32 CandidateIndex = RandomStream->RandRange(0, ValidEntries.Num() - 1);
		const FLootProfileEntry* PickedEntry = ValidEntries[CandidateIndex];
		if (PickedEntry)
		{
			OutEntries.Add(PickedEntry->ToLootEntryView());
		}
		ValidEntries.RemoveAtSwap(CandidateIndex, 1, EAllowShrinking::No);
	}
}

FPrimaryAssetId ULootProfileDefinition::GetPrimaryAssetId() const
{
	if (ProfileId.IsNone())
	{
		return FPrimaryAssetId();
	}

	return FPrimaryAssetId(FPrimaryAssetType(TEXT("LootProfileDefinition")), ProfileId);
}
