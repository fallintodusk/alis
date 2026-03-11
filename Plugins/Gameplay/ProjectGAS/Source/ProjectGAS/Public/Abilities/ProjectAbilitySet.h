// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"  // UPrimaryDataAsset is defined here
#include "GameplayAbilitySpec.h"
#include "ActiveGameplayEffectHandle.h"
#include "ProjectAbilitySet.generated.h"

class UAbilitySystemComponent;
class UGameplayAbility;
class UGameplayEffect;

/**
 * Handles granted by an AbilitySet.
 * Store this to later revoke the granted abilities/effects.
 */
USTRUCT(BlueprintType)
struct PROJECTGAS_API FProjectAbilitySetHandles
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FGameplayAbilitySpecHandle> AbilitySpecHandles;

	UPROPERTY()
	TArray<FActiveGameplayEffectHandle> EffectHandles;

	void Reset();
};

/**
 * Data asset that defines a set of abilities and effects to grant together.
 * Used for equipment (e.g., sword grants attack abilities + stat bonuses).
 *
 * === Abilities vs Effects (KISS rule) ===
 *
 * GrantedAbilities = "things you can DO" (executable logic)
 *   - Can be activated (input, event, AI)
 *   - Can play animations, spawn tasks, apply effects
 *   - Examples: FireWeapon, Dash, Interact, Reload, UseItem
 *
 * GrantedEffects = "state changes" (data-only, numbers/tags)
 *   - Modify attributes (Health, Stamina, etc.)
 *   - Add/remove gameplay tags (Poisoned, ColdResist)
 *   - WARNING: Must be INFINITE duration! Duration effects expire naturally
 *     and won't be removed on unequip - causing permanent stat changes.
 *   - Examples: +10 MaxStamina, Regen +0.2/s, ColdResist tag
 *
 * Rule: Just numbers/tags? -> Effect. Needs logic/input? -> Ability.
 *
 * === Usage (Lyra pattern) ===
 *
 * Called from C++ (InventoryComponent, EquipmentComponent, etc.):
 *
 *   FProjectAbilitySetHandles Handles;
 *   AbilitySet->GiveToAbilitySystem(ASC, &Handles);  // Equip
 *   // ... later ...
 *   UProjectAbilitySet::TakeFromAbilitySystem(ASC, &Handles);  // Unequip
 *
 * === Why no UFUNCTION? ===
 *
 * These methods use raw pointers (FProjectAbilitySetHandles*) which UHT
 * rejects for Blueprint exposure. KISS decision: keep C++ only since
 * gameplay code (InventoryComponent, etc.) calls these anyway.
 */
UCLASS(BlueprintType)
class PROJECTGAS_API UProjectAbilitySet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UProjectAbilitySet();

	// Abilities granted by this set (things you can DO)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities")
	TArray<TSubclassOf<UGameplayAbility>> GrantedAbilities;

	// Effects granted by this set (passive stat bonuses, tags)
	// WARNING: Must be INFINITE duration! See class comment.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects")
	TArray<TSubclassOf<UGameplayEffect>> GrantedEffects;

	// Level for granted abilities/effects (default 1)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities")
	int32 AbilityLevel = 1;

	/**
	 * Grant all abilities and effects to the ASC.
	 * @param ASC - Target ability system component
	 * @param OutHandles - Filled with handles for later revocation (optional)
	 *
	 * Not BlueprintCallable: UHT rejects raw pointer params.
	 * Called from C++ components (Inventory, Equipment, etc.)
	 */
	void GiveToAbilitySystem(UAbilitySystemComponent* ASC, FProjectAbilitySetHandles* OutHandles = nullptr) const;

	/**
	 * Revoke previously granted abilities/effects.
	 * @param ASC - Target ability system component
	 * @param Handles - Handles from previous GiveToAbilitySystem call
	 *
	 * Not BlueprintCallable: UHT rejects raw pointer params.
	 * Called from C++ components (Inventory, Equipment, etc.)
	 */
	static void TakeFromAbilitySystem(UAbilitySystemComponent* ASC, FProjectAbilitySetHandles* Handles);

	// UPrimaryDataAsset interface
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
