// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayTagContainer.h"
#include "ProjectGASLibrary.generated.h"

class UAbilitySystemComponent;
struct FActiveGameplayEffectHandle;

/**
 * Single magnitude entry: Tag + Value pair.
 * Used to specify attribute changes for ApplyMagnitudes().
 *
 * Example:
 *   { SetByCaller.Condition, 20.f }  -> +20 Condition (heal)
 *   { SetByCaller.Calories, 250.f }  -> +250 Calories (food)
 */
USTRUCT(BlueprintType)
struct PROJECTGAS_API FAttributeMagnitude
{
	GENERATED_BODY()

	// SetByCaller tag (e.g., SetByCaller.Condition, SetByCaller.Calories)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS")
	FGameplayTag Tag;

	// Magnitude to apply (positive = increase, negative = decrease)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS")
	float Magnitude = 0.f;

	FAttributeMagnitude() = default;
	FAttributeMagnitude(FGameplayTag InTag, float InMagnitude)
		: Tag(InTag), Magnitude(InMagnitude) {}
};

/**
 * Result of applying magnitudes.
 */
UENUM(BlueprintType)
enum class EApplyMagnitudesResult : uint8
{
	// Effect applied successfully
	Success,

	// Effect was not applied (invalid ASC, no valid magnitudes, etc.)
	Failed
};

/**
 * Utility library for ProjectGAS operations.
 *
 * === Main Function ===
 *
 * ApplyMagnitudes() - One-call solution for any system to modify attributes.
 *
 * === Who Uses This ===
 *
 * - InventoryComponent: Food gives +250 Calories, +0.5 Hydration
 * - WorldComponent: Environmental effects (cold, rain)
 * - CombatComponent: Attack deals -30 Condition
 * - ProjectVitals: Vitals tick (future)
 * - Any system that needs to modify vital attributes
 *
 * === How It Works ===
 *
 * 1. Takes array of FAttributeMagnitude (Tag + Value pairs)
 * 2. Creates spec from UGE_GenericInstant
 * 3. Sets all magnitudes via SetByCaller
 * 4. Applies to ASC
 * 5. Returns result (Success/Failed)
 *
 * === Example ===
 *
 *   TArray<FAttributeMagnitude> Effects;
 *   Effects.Add({ ProjectTags::SetByCaller_Condition, 20.f });
 *   Effects.Add({ ProjectTags::SetByCaller_Calories, 250.f });
 *
 *   EApplyMagnitudesResult Result = UProjectGASLibrary::ApplyMagnitudes(ASC, Effects);
 *
 * SOT: docs/gameplay/character.md, todo/current/gas_ui_mechanics.md
 */
UCLASS()
class PROJECTGAS_API UProjectGASLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Apply attribute magnitudes to an ASC using GE_GenericInstant.
	 *
	 * @param ASC - Target ability system component
	 * @param Magnitudes - Array of Tag+Value pairs to apply
	 * @return Result indicating success or failure
	 *
	 * GAS silently ignores modifiers for missing AttributeSets, so this is safe
	 * to call on any actor regardless of which AttributeSets it has.
	 */
	UFUNCTION(BlueprintCallable, Category = "ProjectGAS")
	static EApplyMagnitudesResult ApplyMagnitudes(
		UAbilitySystemComponent* ASC,
		const TArray<FAttributeMagnitude>& Magnitudes);

	/**
	 * Convenience: Apply a single magnitude.
	 * Equivalent to ApplyMagnitudes with one-element array.
	 */
	UFUNCTION(BlueprintCallable, Category = "ProjectGAS")
	static EApplyMagnitudesResult ApplySingleMagnitude(
		UAbilitySystemComponent* ASC,
		FGameplayTag Tag,
		float Magnitude);
};
