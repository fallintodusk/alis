// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "VitalsEnums.generated.h"

/**
 * Shared vitals state enums.
 *
 * Lives in ProjectVitals (the owning Feature plugin) because these are
 * domain types consumed by BOTH the Vitals runtime component (for
 * hysteresis-aware state tracking) AND the Vitals UI plugin (for display
 * bucketing). The UI plugin depends on ProjectVitals for these types;
 * cross-plugin services + events are consumed via interfaces in ProjectCore
 * (see IVitalsEventsSource, FVitalsConfig).
 *
 * ABI note: the serialized script path for these enums is
 *   /Script/ProjectVitals.EVitalState
 *   /Script/ProjectVitals.EFatigueState
 * If a future refactor moves them, add CoreRedirects AND resave affected
 * assets before merge (see docs/editor/class_migration.md).
 */

/**
 * Vital state enum for threshold-based states.
 * Used by UI to show appropriate icons/effects.
 */
UENUM(BlueprintType)
enum class EVitalState : uint8
{
	OK,       // >70%
	Low,      // 40-70%
	Critical, // 20-40%
	Empty     // <=20%
};

/**
 * Fatigue state enum (inverted: 0=good, 100=bad).
 */
UENUM(BlueprintType)
enum class EFatigueState : uint8
{
	Rested,    // <30%
	Tired,     // 30-60%
	Exhausted, // 60-85%
	Critical   // >=85%
};
