# Inventory: Consumable Use-Gating

## Problem

`Internal_UseItem()` blindly applies magnitudes and consumes the item regardless of whether any magnitude would actually change anything. Player can waste a medkit at full health with no bleeding, or drink water at full hydration.

## Current Behavior

1. Player uses item
2. All magnitudes applied (even if target attribute is already at max/min)
3. Item consumed if `bConsumeOnUse = true`
4. No feedback that the item was "wasted"

## Expected Behavior

Before applying, check if at least one magnitude would produce a meaningful change:
- Healing: Condition < MaxCondition
- Bleeding cure: Bleeding > 0
- Hydration: Hydration < MaxHydration
- Calories: Calories < MaxCalories

If no magnitude would change anything, block use and optionally show feedback ("No effect").

## Scope

- `ProjectInventory/Private/Components/ProjectInventoryComponent.cpp` - `Internal_UseItem()`
- Needs ASC attribute read access (already available via `GetOwnerASC()`)
- Optional: UI feedback via existing Mind toast or inventory event

## References

- `Internal_UseItem()`: `ProjectInventory/Private/Components/ProjectInventoryComponent.cpp:2928`
- Attribute sets: `ProjectGAS/Public/Attributes/HealthAttributeSet.h`, `StatusAttributeSet.h`, `SurvivalAttributeSet.h`
