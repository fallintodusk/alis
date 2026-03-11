// Copyright ALIS. All Rights Reserved.

#include "ProjectGASLibrary.h"
#include "AbilitySystemComponent.h"
#include "Effects/GE_GenericInstant.h"
#include "ProjectGameplayTags.h"

EApplyMagnitudesResult UProjectGASLibrary::ApplyMagnitudes(
	UAbilitySystemComponent* ASC,
	const TArray<FAttributeMagnitude>& Magnitudes)
{
	// Validate inputs
	if (!ASC)
	{
		return EApplyMagnitudesResult::Failed;
	}

	if (Magnitudes.Num() == 0)
	{
		return EApplyMagnitudesResult::Failed;
	}

	// Create effect context and spec from our generic GE
	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(ASC->GetOwner());

	FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(
		UGE_GenericInstant::StaticClass(),
		1,  // Level
		Context);

	if (!SpecHandle.IsValid())
	{
		return EApplyMagnitudesResult::Failed;
	}

	// CRITICAL: Initialize ALL SetByCaller magnitudes to 0 first.
	// GE_GenericInstant has modifiers for all attributes. If we don't set them all,
	// GAS logs errors when trying to read unset SetByCaller values.
	// Setting to 0.0f means "no change" for that attribute.
	SpecHandle.Data->SetSetByCallerMagnitude(ProjectTags::SetByCaller_Condition.GetTag(), 0.0f);
	SpecHandle.Data->SetSetByCallerMagnitude(ProjectTags::SetByCaller_Stamina.GetTag(), 0.0f);
	SpecHandle.Data->SetSetByCallerMagnitude(ProjectTags::SetByCaller_Hydration.GetTag(), 0.0f);
	SpecHandle.Data->SetSetByCallerMagnitude(ProjectTags::SetByCaller_Calories.GetTag(), 0.0f);
	SpecHandle.Data->SetSetByCallerMagnitude(ProjectTags::SetByCaller_Fatigue.GetTag(), 0.0f);

	// Now set the caller's specific magnitudes (overriding the 0 defaults)
	for (const FAttributeMagnitude& Mag : Magnitudes)
	{
		if (Mag.Tag.IsValid())
		{
			SpecHandle.Data->SetSetByCallerMagnitude(Mag.Tag, Mag.Magnitude);
		}
	}

	// Apply the effect
	FActiveGameplayEffectHandle Handle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

	// Check if effect was applied
	// Note: For instant effects, Handle.IsValid() may be false even on success
	// because instant effects don't persist. We trust it worked if we got here.
	return EApplyMagnitudesResult::Success;
}

EApplyMagnitudesResult UProjectGASLibrary::ApplySingleMagnitude(
	UAbilitySystemComponent* ASC,
	FGameplayTag Tag,
	float Magnitude)
{
	TArray<FAttributeMagnitude> Magnitudes;
	Magnitudes.Add(FAttributeMagnitude(Tag, Magnitude));
	return ApplyMagnitudes(ASC, Magnitudes);
}
