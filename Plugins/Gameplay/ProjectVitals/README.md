# ProjectVitals

Server-side vitals tick system: metabolism calculations, threshold state management, condition regen/drain.

## Purpose

Owns the periodic vitals simulation on server. **Does NOT own AttributeSets or ASC lifecycle.**

**What ProjectVitals does:**
- Periodic metabolism tick (server-only)
- Apply/remove State.* tags based on thresholds (with hysteresis)
- Apply/remove threshold debuffs that modify MULTIPLIERS (StaminaRegenRate, ConditionRegenRate)
- Compute condition regen/drain inline (single source of truth)

**What ProjectVitals does NOT do:**
- Own AttributeSets (ProjectGAS owns those)
- Create or manage ASC (ProjectCharacter owns that)
- UI display (ProjectVitalsUI owns that)

**SOT:** [ProjectGAS README](../ProjectGAS/README.md) for AttributeSets, tags, and thresholds.

## Config

All tuning values: [Data/vitals_config.json](Data/vitals_config.json) (two systems: `condition` + `stamina`).
Rates derived at runtime from timelines. See `ComputeDerivedRates()` in [VitalsConfig.h](Source/ProjectVitals/Public/VitalsConfig.h).

## Design

**Design vision (rationale, philosophy, timelines):** [docs/design_vision.md](docs/design_vision.md)

Key architecture decisions:
- **Debuffs modify multipliers, TickVitals applies deltas** - avoids periodic GE evaluation gotchas. See [GE_ThresholdDebuffs.h](Source/ProjectVitals/Public/Effects/GE_ThresholdDebuffs.h).
- **Two-layer system** - Layer 1 (metabolism) = normal gameplay, Layer 2 (dying mode) = survival crisis
- **Hysteresis** - 2% buffer prevents threshold flickering. See `ComputeVitalStateWithHysteresis()` in component.

## Data Flow (TickVitals)

```
Timer fires (1s interval)
  1. ApplyMetabolism    - MET-based calorie/hydration drain, fatigue gain
  2. ApplyBleedingDrain - Condition drain from bleeding severity
  3. UpdateStateTags    - State.* tags with hysteresis (replicate to clients)
  4. UpdateThresholdDebuffs - Apply/remove GE multiplier debuffs
  5. ApplyConditionDelta    - Net regen/drain (single source of truth)
```

**Key invariant:** VitalsComponent only reads ASC attributes and writes back via SetByCaller.

## Why ActorComponent (not Subsystem)

Per-actor state (tick timer, cached ASC, debuff handles), server authority check per actor, lifecycle tied to character possession. Uses `IAbilitySystemInterface` - no dependency on ProjectCharacter.

## Component Usage

```cpp
// In ProjectCharacter constructor
VitalsComponent = CreateDefaultSubobject<UProjectVitalsComponent>(TEXT("VitalsComponent"));

// In PossessedBy() after ASC init
VitalsComponent->Start();

// In UnPossessed() / EndPlay()
VitalsComponent->Stop();
```

## Threshold System

| Vital | OK | Low | Critical | Empty |
|-------|-----|-----|----------|-------|
| Condition/Stamina/Calories/Hydration | >70% | 40-70% | 20-40% | <=20% |

| Fatigue (inverted) | Rested | Tired | Exhausted | Critical |
|--------------------|--------|-------|-----------|----------|
| Percent | <30% | 30-60% | 60-85% | >=85% |

**Tag ownership:** ProjectVitals is the ONLY writer of State.* tags. UI only reads.

## Dependency Graph

```
ProjectVitals -> ProjectGAS (AttributeSets, Tags) -> ProjectCore
ProjectCharacter -> ProjectVitals (component) + ProjectGAS (ASC ownership)
```

**Critical:** ProjectVitals does NOT depend on ProjectCharacter (avoids cycle).

## File Structure

```
ProjectVitals/
  README.md                         # Architecture + routing
  Data/vitals_config.json           # All tuning values (SOT)
  docs/design_vision.md             # Design rationale + philosophy
  Source/ProjectVitals/
    Public/
      ProjectVitalsComponent.h      # Main component
      VitalsConfig.h                # Config structs + loader
      Effects/GE_ThresholdDebuffs.h # Debuff GE definitions
    Private/
      ProjectVitalsComponent.cpp
      VitalsConfig.cpp              # JSON parser
```

## References

- [ProjectGAS README](../ProjectGAS/README.md) - AttributeSets, tags, thresholds
- [ProjectVitalsUI README](../../UI/ProjectVitalsUI/README.md) - UI display
- [vitals_config.json](Data/vitals_config.json) - All tuning values
