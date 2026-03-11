// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProjectUserSettings.h"
#include "ProjectSettingsDelta.generated.h"

/**
 * Delta between two settings states.
 * Uses TSet<FName> for changed properties + category flags for O(1) batch checks.
 * Immutable after Compute() - no setters.
 *
 * SOLID: Single source of truth is PROJECT_SETTINGS_FIELDS macro.
 * Delta is generated from it, not maintained separately.
 */
USTRUCT(BlueprintType)
struct PROJECTSETTINGSUI_API FProjectSettingsDelta
{
	GENERATED_BODY()

	// Changed property names (uses GET_MEMBER_NAME_CHECKED for compile-time safety)
	TSet<FName> Changed;

	// Category flags - O(1) batch checks (set during Compute)
	uint8 bDisplay : 1;
	uint8 bScalability : 1;
	uint8 bAudio : 1;
	uint8 bGame : 1;

	FProjectSettingsDelta()
		: bDisplay(0)
		, bScalability(0)
		, bAudio(0)
		, bGame(0)
	{
	}

	// Query by property name (compile-time validated via GET_MEMBER_NAME_CHECKED at call site)
	bool IsChanged(FName Property) const { return Changed.Contains(Property); }

	// Category checks - O(1)
	bool HasDisplay() const { return bDisplay; }
	bool HasScalability() const { return bScalability; }
	bool HasAudio() const { return bAudio; }
	bool HasGame() const { return bGame; }
	bool IsEmpty() const { return Changed.Num() == 0; }

	// Logging
	void Log(const TCHAR* Context) const;

	// Factory - uses PROJECT_SETTINGS_FIELDS macro for single source of truth
	static FProjectSettingsDelta Compute(const FProjectUserSettings& Old, const FProjectUserSettings& New);
};
