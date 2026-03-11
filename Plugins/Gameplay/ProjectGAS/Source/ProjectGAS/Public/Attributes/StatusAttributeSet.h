// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "ProjectGASAttributeMacros.h"
#include "StatusAttributeSet.generated.h"

/**
 * Status effect attributes for combat/environment effects.
 *
 * === How Status Effects Work ===
 *
 * These are "intensity" values, not percentages:
 *   - 0.0 = Not affected (status inactive)
 *   - >0.0 = Affected (higher = more severe)
 *
 * The actual effect (damage per tick, movement slow, etc.) is handled by
 * GameplayEffects that read these values.
 *
 * === Composition ===
 *
 *   - Player: HealthAttributeSet + SurvivalAttributeSet + StaminaAttributeSet + StatusAttributeSet
 *   - NPC: HealthAttributeSet + StatusAttributeSet (if they can be poisoned/radiated)
 *   - Robot: HealthAttributeSet only (immune to status effects)
 *
 * === SetByCaller Tags ===
 *
 *   - SetByCaller.Bleeding
 *   - SetByCaller.Poisoned
 *   - SetByCaller.Radiation
 *
 * === Example Usage ===
 *
 * Poison apple applies:
 *   GE_Poison (Instant): SetByCaller.Poisoned = 50.0 (add stacks)
 *   GE_PoisonTick (Periodic): If Poisoned > 0, deal damage and reduce Poisoned
 */
UCLASS()
class PROJECTGAS_API UStatusAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UStatusAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	// Bleeding intensity (0 = not bleeding, >0 = bleeding)
	// Higher values = faster health drain
	UPROPERTY(BlueprintReadOnly, Category = "Status", ReplicatedUsing = OnRep_Bleeding)
	FGameplayAttributeData Bleeding;
	ATTRIBUTE_ACCESSORS(UStatusAttributeSet, Bleeding)

	// Poisoned intensity (0 = not poisoned, >0 = poisoned)
	// Higher values = stronger poison effect
	UPROPERTY(BlueprintReadOnly, Category = "Status", ReplicatedUsing = OnRep_Poisoned)
	FGameplayAttributeData Poisoned;
	ATTRIBUTE_ACCESSORS(UStatusAttributeSet, Poisoned)

	// Radiation level (0 = clean, >0 = irradiated)
	// Higher values = faster radiation damage
	UPROPERTY(BlueprintReadOnly, Category = "Status", ReplicatedUsing = OnRep_Radiation)
	FGameplayAttributeData Radiation;
	ATTRIBUTE_ACCESSORS(UStatusAttributeSet, Radiation)

	// Movement speed multiplier (1.0 = normal, 0.5 = half speed)
	// Modified by weight encumbrance, status effects, buffs
	// Character reads this and applies to CMC MaxWalkSpeed
	UPROPERTY(BlueprintReadOnly, Category = "Status", ReplicatedUsing = OnRep_MovementSpeedMultiplier)
	FGameplayAttributeData MovementSpeedMultiplier;
	ATTRIBUTE_ACCESSORS(UStatusAttributeSet, MovementSpeedMultiplier)

protected:
	UFUNCTION()
	void OnRep_Bleeding(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Poisoned(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Radiation(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MovementSpeedMultiplier(const FGameplayAttributeData& OldValue);
};
