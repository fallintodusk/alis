// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ProjectTagTextSubsystem.generated.h"

/**
 * Tag Text Subsystem - Registry for gameplay tag to localized text mapping.
 *
 * Provides a central registry where feature plugins register human-readable
 * descriptions for their gameplay tags. UI systems query this to display
 * localized text for active states, effects, and conditions.
 *
 * Key features:
 * - Feature plugins register their tag texts in bootstrap subsystems
 * - UI widgets query for display text without hardcoding strings
 * - Warns on duplicate registration (same tag registered twice)
 * - Game-thread read access only (registration expected during init only)
 *
 * Usage:
 * @code
 * // Register (in bootstrap subsystem Initialize)
 * TagTextSub->Register(ProjectTags::State_Calories_Low,
 *     LOCTEXT("State_Calories_Low", "Hungry - stamina regen reduced"));
 *
 * // Query (in ViewModel or widget)
 * FText DisplayText;
 * if (TagTextSub->TryGet(ActiveTag, DisplayText))
 * {
 *     // Show DisplayText to player
 * }
 * @endcode
 *
 * Design notes:
 * - Text should describe the state without hard numbers (avoid drift)
 * - Good: "Hungry - stamina regen reduced"
 * - Bad: "Hungry - stamina regen -20%" (will drift from tuning)
 *
 * SOT: todo/current/gas_ui_mechanics.md (Phase 11)
 */
UCLASS()
class PROJECTUI_API UProjectTagTextSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * Register a localized text for a gameplay tag.
	 *
	 * @param Tag - The gameplay tag to map
	 * @param Text - Localized display text for the tag
	 * @return true if registered, false if tag was already registered
	 */
	UFUNCTION(BlueprintCallable, Category = "UI|TagText")
	bool Register(FGameplayTag Tag, FText Text);

	/**
	 * Register a localized text for a gameplay tag, replacing any existing entry.
	 *
	 * @param Tag - The gameplay tag to map
	 * @param Text - Localized display text for the tag
	 * @return true if registered or replaced, false if tag was invalid
	 */
	UFUNCTION(BlueprintCallable, Category = "UI|TagText")
	bool RegisterOrReplace(FGameplayTag Tag, FText Text);

	/**
	 * Try to get the localized text for a gameplay tag.
	 *
	 * @param Tag - The gameplay tag to look up
	 * @param OutText - Receives the localized text if found
	 * @return true if found, false if tag has no registered text
	 */
	UFUNCTION(BlueprintPure, Category = "UI|TagText")
	bool TryGet(FGameplayTag Tag, FText& OutText) const;

	/**
	 * Get the localized text for a gameplay tag, or empty text if not found.
	 *
	 * @param Tag - The gameplay tag to look up
	 * @return Localized text, or empty FText if not registered
	 */
	UFUNCTION(BlueprintPure, Category = "UI|TagText")
	FText Get(FGameplayTag Tag) const;

	/**
	 * Check if a tag has registered text.
	 *
	 * @param Tag - The gameplay tag to check
	 * @return true if tag has registered text
	 */
	UFUNCTION(BlueprintPure, Category = "UI|TagText")
	bool HasText(FGameplayTag Tag) const;

	/**
	 * Get display texts for all matching tags in a container.
	 * Useful for displaying all active state effects.
	 *
	 * @param Tags - Container of tags to look up
	 * @param ParentTag - Optional filter: only include tags that match this parent (pass empty tag for no filter)
	 * @return Array of localized texts for matching registered tags
	 */
	UFUNCTION(BlueprintPure, Category = "UI|TagText")
	TArray<FText> GetTextsForTags(const FGameplayTagContainer& Tags, FGameplayTag ParentTag) const;

private:
	/** Map of Tag -> Localized Text */
	TMap<FGameplayTag, FText> TagTextMap;
};
