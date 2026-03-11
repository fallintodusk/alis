# Vitals Design Vision

Design rationale for the survival system. Values and formulas live in
[vitals_config.json](../Data/vitals_config.json) and code - this doc explains **why**.

---

## Core Philosophy: Real-World Physiology

Players should be able to use real-world survival knowledge. "I need ~2L water per day" -
accurate here. "Sprinting burns 8x more energy than resting" - just like real life.

- **MET values** - standard exercise science (idle 1.3, sprint 10.0)
- **Calorie formula** - body weight x activity level (used in nutrition science)
- **Hydration loss** - 1 liter per 1000 kcal (realistic sweat/respiration rate)
- **1:1 time scale** - 1 game hour = 1 real hour, no time dilation

## Death Timelines (Medical Reality vs Gameplay)

Timelines are compressed from medical reality for gameplay urgency:

| Vital | Medical reality | Gameplay | Ratio |
|-------|----------------|----------|-------|
| Dehydration | ~3 days | 2 days | 0.67x |
| Starvation | 30-70 days | 14 days | ~0.3x |
| Exhaustion | Never kills | 5 days | n/a |

Total = vital drain time + `conditionDeathDays` from config.
Example: Dehydration = 1 day hydration drain + 1 day condition drain = 2 days total.

## Two-Layer System

**Layer 1 - Metabolism (Normal Gameplay):** Resources drain by activity. Player is functional.
Manage food, water, rest as part of exploration loop.

**Layer 2 - Dying Mode (Survival Crisis):** Triggered when resources hit 0%.
Critical state **MUST feel constraining** - player can't "power through" starvation.
Severe debuffs limit movement, stamina, actions. Condition drains over days as grace period.

```
Healthy (>70%) -> resource management, active gameplay
  -> Low (40-70%) -> warning debuffs, find resources soon
    -> Critical (0-20%) -> DYING MODE: barely functional
      -> Condition draining -> days to find help or die
```

## Bleeding Design

- `Drain = Severity x 0.5 HP/s` (see `BleedingDrainPerSecond` in component)
- Tiers in `condition.status.bleeding.tiers` (config)
- Light bleeding lasts hours (time to find bandage), severe is minutes (urgent)
- **Bandage stops bleeding ONLY** - sets Bleeding to 0, does NOT restore Condition
- Condition regenerates normally after bleeding stops (if other gates pass)

## Condition Regen

Auto-regen ONLY when ALL gates pass:
- No bleeding
- All vitals above critical (0% Calories/Hydration, <95% Fatigue)
- ConditionRegenRate > 0 (not suppressed by debuffs)
- Condition < Max

Rate from `baseRegenDays` in config. Drains from multiple critical vitals stack additively.

## References

- Config (all values): [vitals_config.json](../Data/vitals_config.json)
- Component (tick logic): [ProjectVitalsComponent.h](../Source/ProjectVitals/Public/ProjectVitalsComponent.h)
- Debuffs (multipliers): [GE_ThresholdDebuffs.h](../Source/ProjectVitals/Public/Effects/GE_ThresholdDebuffs.h)
- UI display: [ProjectVitalsUI README](../../../UI/ProjectVitalsUI/README.md)
