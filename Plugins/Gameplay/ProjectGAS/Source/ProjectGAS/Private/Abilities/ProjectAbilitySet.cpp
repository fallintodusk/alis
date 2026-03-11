// Copyright ALIS. All Rights Reserved.

#include "Abilities/ProjectAbilitySet.h"
#include "AbilitySystemComponent.h"
#include "GameplayAbilitySpec.h"

void FProjectAbilitySetHandles::Reset()
{
	AbilitySpecHandles.Reset();
	EffectHandles.Reset();
}

UProjectAbilitySet::UProjectAbilitySet()
{
}

void UProjectAbilitySet::GiveToAbilitySystem(UAbilitySystemComponent* ASC, FProjectAbilitySetHandles* OutHandles) const
{
	if (!ASC)
	{
		return;
	}

	if (OutHandles)
	{
		OutHandles->Reset();
	}

	// Grant abilities
	for (const TSubclassOf<UGameplayAbility>& AbilityClass : GrantedAbilities)
	{
		if (!AbilityClass)
		{
			continue;
		}

		FGameplayAbilitySpec AbilitySpec(AbilityClass, AbilityLevel);
		FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(AbilitySpec);

		if (OutHandles)
		{
			OutHandles->AbilitySpecHandles.Add(Handle);
		}
	}

	// Grant effects (infinite duration - removed on unequip)
	for (const TSubclassOf<UGameplayEffect>& EffectClass : GrantedEffects)
	{
		if (!EffectClass)
		{
			continue;
		}

		FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
		FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(EffectClass, AbilityLevel, ContextHandle);

		if (SpecHandle.IsValid())
		{
			FActiveGameplayEffectHandle EffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

			if (OutHandles && EffectHandle.IsValid())
			{
				OutHandles->EffectHandles.Add(EffectHandle);
			}
		}
	}
}

void UProjectAbilitySet::TakeFromAbilitySystem(UAbilitySystemComponent* ASC, FProjectAbilitySetHandles* Handles)
{
	if (!ASC || !Handles)
	{
		return;
	}

	// Remove abilities
	for (const FGameplayAbilitySpecHandle& Handle : Handles->AbilitySpecHandles)
	{
		if (Handle.IsValid())
		{
			ASC->ClearAbility(Handle);
		}
	}

	// Remove effects
	for (const FActiveGameplayEffectHandle& Handle : Handles->EffectHandles)
	{
		if (Handle.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(Handle);
		}
	}

	Handles->Reset();
}

FPrimaryAssetId UProjectAbilitySet::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("AbilitySet"), GetFName());
}
