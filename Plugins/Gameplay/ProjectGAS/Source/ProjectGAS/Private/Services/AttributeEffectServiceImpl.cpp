// Copyright ALIS. All Rights Reserved.

#include "Services/AttributeEffectServiceImpl.h"
#include "ProjectGASLibrary.h"
#include "AbilitySystemBlueprintLibrary.h"

bool FAttributeEffectServiceImpl::ApplyMagnitudes(
	AActor* TargetActor,
	const TMap<FGameplayTag, float>& Magnitudes)
{
	if (!TargetActor || Magnitudes.IsEmpty())
	{
		return false;
	}

	UAbilitySystemComponent* ASC =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
	if (!ASC)
	{
		return false;
	}

	TArray<FAttributeMagnitude> MagnitudeArray;
	MagnitudeArray.Reserve(Magnitudes.Num());
	for (const auto& Pair : Magnitudes)
	{
		MagnitudeArray.Add(FAttributeMagnitude{Pair.Key, Pair.Value});
	}

	EApplyMagnitudesResult Result = UProjectGASLibrary::ApplyMagnitudes(ASC, MagnitudeArray);
	return Result == EApplyMagnitudesResult::Success;
}
