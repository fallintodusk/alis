// Copyright ALIS. All Rights Reserved.

#include "Attributes/HealthAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"

UHealthAttributeSet::UHealthAttributeSet()
{
	InitCondition(100.f);
	InitMaxCondition(100.f);
	InitConditionRegenRate(1.f);  // 100% regen rate by default
}

void UHealthAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UHealthAttributeSet, Condition, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UHealthAttributeSet, MaxCondition, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UHealthAttributeSet, ConditionRegenRate, COND_None, REPNOTIFY_Always);
}

void UHealthAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	// Clamp Condition to [0, MaxCondition]
	if (Attribute == GetConditionAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxCondition());
	}
	// When MaxCondition changes, also clamp Condition if needed
	else if (Attribute == GetMaxConditionAttribute())
	{
		AdjustAttributeForMaxChange(Condition, MaxCondition, NewValue, GetConditionAttribute());
	}
}

void UHealthAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetConditionAttribute())
	{
		SetCondition(FMath::Clamp(GetCondition(), 0.f, GetMaxCondition()));
	}
}

void UHealthAttributeSet::AdjustAttributeForMaxChange(
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

void UHealthAttributeSet::OnRep_Condition(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UHealthAttributeSet, Condition, OldValue);
}

void UHealthAttributeSet::OnRep_MaxCondition(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UHealthAttributeSet, MaxCondition, OldValue);
}

void UHealthAttributeSet::OnRep_ConditionRegenRate(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UHealthAttributeSet, ConditionRegenRate, OldValue);
}
