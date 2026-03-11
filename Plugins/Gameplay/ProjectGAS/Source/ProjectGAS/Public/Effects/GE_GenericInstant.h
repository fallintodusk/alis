// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_GenericInstant.generated.h"

/**
 * Generic instant GameplayEffect with SetByCaller modifiers for all vital attributes.
 *
 * === Why This Exists ===
 *
 * GAS requires a UGameplayEffect class to modify attributes. Instead of creating
 * separate GE classes for each item/effect source, we use ONE generic GE with
 * SetByCaller for all attributes. Callers set only the magnitudes they need.
 *
 * === How It Works ===
 *
 * 1. This GE has modifiers for: Condition, Stamina, Hydration, Calories, Fatigue
 * 2. Each modifier uses SetByCaller to get its magnitude at runtime
 * 3. Unused SetByCaller tags default to 0 (no change)
 * 4. GAS silently ignores modifiers for missing AttributeSets
 *
 * === Usage ===
 *
 * Preferred: Use UProjectGASLibrary::ApplyMagnitudes() utility.
 *
 * Manual (if needed):
 *   FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
 *   FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(
 *       UGE_GenericInstant::StaticClass(), 1, Context);
 *   Spec.Data->SetSetByCallerMagnitude(ProjectTags::SetByCaller_Condition, 20.f);
 *   Spec.Data->SetSetByCallerMagnitude(ProjectTags::SetByCaller_Calories, 250.f);
 *   ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
 *
 * === Who Uses This ===
 *
 * - InventoryComponent: consuming food/potions
 * - WorldComponent: environmental effects (rain, cold)
 * - CombatComponent: damage/healing
 * - ProjectVitals: vitals tick (future)
 * - Any system that modifies vital attributes
 *
 * === SetByCaller Tags (defined in ProjectCore) ===
 *
 * - SetByCaller.Condition (health/life)
 * - SetByCaller.Stamina (sprint/melee)
 * - SetByCaller.Hydration (water, 0-3.0L)
 * - SetByCaller.Calories (food energy, 0-2500 kcal)
 * - SetByCaller.Fatigue (sleep debt, 0-100%)
 *
 * SOT: docs/gameplay/character.md, todo/current/gas_ui_mechanics.md
 */
UCLASS()
class PROJECTGAS_API UGE_GenericInstant : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_GenericInstant();
};
