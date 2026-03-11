// Copyright ALIS. All Rights Reserved.

#include "Attributes/StatusAttributeSet.h"
#include "Net/UnrealNetwork.h"

UStatusAttributeSet::UStatusAttributeSet()
{
	// All status effects start at 0 (inactive)
	InitBleeding(0.f);
	InitPoisoned(0.f);
	InitRadiation(0.f);

	// Movement speed multiplier starts at 1.0 (normal speed)
	InitMovementSpeedMultiplier(1.f);
}

void UStatusAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UStatusAttributeSet, Bleeding, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UStatusAttributeSet, Poisoned, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UStatusAttributeSet, Radiation, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UStatusAttributeSet, MovementSpeedMultiplier, COND_None, REPNOTIFY_Always);
}

void UStatusAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	// Status effects cannot go below 0
	if (Attribute == GetBleedingAttribute() ||
		Attribute == GetPoisonedAttribute() ||
		Attribute == GetRadiationAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}

	// Movement speed multiplier cannot go below 0 (can exceed 1.0 for buffs)
	if (Attribute == GetMovementSpeedMultiplierAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
}

void UStatusAttributeSet::OnRep_Bleeding(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UStatusAttributeSet, Bleeding, OldValue);
}

void UStatusAttributeSet::OnRep_Poisoned(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UStatusAttributeSet, Poisoned, OldValue);
}

void UStatusAttributeSet::OnRep_Radiation(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UStatusAttributeSet, Radiation, OldValue);
}

void UStatusAttributeSet::OnRep_MovementSpeedMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UStatusAttributeSet, MovementSpeedMultiplier, OldValue);
}
