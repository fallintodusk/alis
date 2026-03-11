// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Dom/JsonObject.h"
#include "Interfaces/IMindThoughtSource.h"

class UAbilitySystemComponent;

/**
 * Built-in vitals source for ProjectMind.
 *
 * Reads state tags from local pawn ASC and maps them to thoughts via
 * data-driven config in Content/Data/Schema/Gameplay/ProjectMind.
 */
class FDefaultVitalsMindThoughtSource : public IMindVitalsThoughtSource
{
public:
	FDefaultVitalsMindThoughtSource();

	virtual void CollectThoughts(
		const FMindThoughtSourceContext& Context,
		TArray<FMindThoughtCandidate>& OutThoughts) const override;
	virtual FOnMindThoughtSourceChanged& OnThoughtSourceChanged() override;

private:
	struct FStateThoughtMapping
	{
		FGameplayTag StateTag;
		FMindThoughtCandidate Thought;
	};

	void LoadMappings();
	FString ResolveMappingPath() const;
	static EMindThoughtChannel ParseMindThoughtChannel(const FString& ChannelString);
	static EMindThoughtSourceType ParseMindThoughtSourceType(const FString& SourceTypeString);
	static FName ParseOptionalNameField(const TSharedPtr<FJsonObject>& EntryObject, const TCHAR* FieldName);

private:
	TArray<FStateThoughtMapping> Mappings;
	FOnMindThoughtSourceChanged SourceChangedDelegate;
};
