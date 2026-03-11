// Copyright ALIS. All Rights Reserved.

#include "Services/NullQuestMindThoughtSource.h"

void FNullQuestMindThoughtSource::CollectThoughts(
	const FMindThoughtSourceContext& Context,
	TArray<FMindThoughtCandidate>& OutThoughts) const
{
	(void)Context;
	(void)OutThoughts;
}

FOnMindThoughtSourceChanged& FNullQuestMindThoughtSource::OnThoughtSourceChanged()
{
	return SourceChangedDelegate;
}
