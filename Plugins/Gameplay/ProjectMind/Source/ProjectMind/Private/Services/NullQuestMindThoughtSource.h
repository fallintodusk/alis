// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IMindThoughtSource.h"

/**
 * Default quest source stub until dedicated quest plugin provides implementation.
 */
class FNullQuestMindThoughtSource : public IMindQuestThoughtSource
{
public:
	virtual void CollectThoughts(
		const FMindThoughtSourceContext& Context,
		TArray<FMindThoughtCandidate>& OutThoughts) const override;
	virtual FOnMindThoughtSourceChanged& OnThoughtSourceChanged() override;

private:
	FOnMindThoughtSourceChanged SourceChangedDelegate;
};
