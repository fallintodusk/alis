// Copyright ALIS. All Rights Reserved.

#include "Attributes/SurvivalAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"

USurvivalAttributeSet::USurvivalAttributeSet()
{
	// Hydration: Start fully hydrated (3.0 liters)
	InitHydration(3.f);
	InitMaxHydration(3.f);

	// Calories: Start well-fed (2500 kcal)
	InitCalories(2500.f);
	InitMaxCalories(2500.f);

	// Fatigue: Start rested (0 = no fatigue, 100 = exhausted)
	InitFatigue(0.f);
	InitMaxFatigue(100.f);
}

void USurvivalAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(USurvivalAttributeSet, Hydration, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USurvivalAttributeSet, MaxHydration, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USurvivalAttributeSet, Calories, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USurvivalAttributeSet, MaxCalories, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USurvivalAttributeSet, Fatigue, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USurvivalAttributeSet, MaxFatigue, COND_None, REPNOTIFY_Always);
}

void USurvivalAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	// Clamp current values to [0, Max]
	if (Attribute == GetHydrationAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHydration());
	}
	else if (Attribute == GetCaloriesAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxCalories());
	}
	else if (Attribute == GetFatigueAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxFatigue());
	}
	// Handle Max changes - clamp current if max decreases
	else if (Attribute == GetMaxHydrationAttribute())
	{
		AdjustAttributeForMaxChange(Hydration, MaxHydration, NewValue, GetHydrationAttribute());
	}
	else if (Attribute == GetMaxCaloriesAttribute())
	{
		AdjustAttributeForMaxChange(Calories, MaxCalories, NewValue, GetCaloriesAttribute());
	}
	else if (Attribute == GetMaxFatigueAttribute())
	{
		AdjustAttributeForMaxChange(Fatigue, MaxFatigue, NewValue, GetFatigueAttribute());
	}
}

void USurvivalAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// Final clamp after effect execution
	if (Data.EvaluatedData.Attribute == GetHydrationAttribute())
	{
		SetHydration(FMath::Clamp(GetHydration(), 0.f, GetMaxHydration()));
	}
	else if (Data.EvaluatedData.Attribute == GetCaloriesAttribute())
	{
		SetCalories(FMath::Clamp(GetCalories(), 0.f, GetMaxCalories()));
	}
	else if (Data.EvaluatedData.Attribute == GetFatigueAttribute())
	{
		SetFatigue(FMath::Clamp(GetFatigue(), 0.f, GetMaxFatigue()));
	}
}

void USurvivalAttributeSet::AdjustAttributeForMaxChange(
	const FGameplayAttributeData& AffectedAttribute,
	const FGameplayAttributeData& MaxAttribute,
	float NewMaxValue,
	const FGameplayAttribute& AffectedAttributeProperty) const
{
	UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	const float CurrentValue = AffectedAttribute.GetCurrentValue();
	const float CurrentMax = MaxAttribute.GetCurrentValue();

	// If max is being reduced and current exceeds new max, clamp it
	if (NewMaxValue < CurrentMax && CurrentValue > NewMaxValue)
	{
		ASC->ApplyModToAttribute(AffectedAttributeProperty, EGameplayModOp::Override, NewMaxValue);
	}
}

// === OnRep Functions (client-side replication callbacks) ===

void USurvivalAttributeSet::OnRep_Hydration(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USurvivalAttributeSet, Hydration, OldValue);
}

void USurvivalAttributeSet::OnRep_MaxHydration(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USurvivalAttributeSet, MaxHydration, OldValue);
}

void USurvivalAttributeSet::OnRep_Calories(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USurvivalAttributeSet, Calories, OldValue);
}

void USurvivalAttributeSet::OnRep_MaxCalories(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USurvivalAttributeSet, MaxCalories, OldValue);
}

void USurvivalAttributeSet::OnRep_Fatigue(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USurvivalAttributeSet, Fatigue, OldValue);
}

void USurvivalAttributeSet::OnRep_MaxFatigue(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USurvivalAttributeSet, MaxFatigue, OldValue);
}
