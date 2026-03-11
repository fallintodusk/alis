# ProjectGAS

GAS (Gameplay Ability System) wrapper providing core primitives for ALIS.

## Purpose

Core GAS primitives that all gameplay systems can depend on. **Does NOT contain feature-specific logic.**

**What ProjectGAS provides:** AttributeSets, AbilitySets, GameplayEffects, helper library.

**What ProjectGAS does NOT do:** Own ASC, create characters, handle init/replication setup, vitals calculations.

**ASC ownership:** Lives in [ProjectCharacter](../ProjectCharacter/README.md) (characters own their ASC).

**Vitals calculations:** Lives in [ProjectVitals](../ProjectVitals/README.md) (drain rates, thresholds, regen gating).

**SOT:** [Character & Vitals Architecture](../ProjectCharacter/docs/design.md)

## What is ASC (AbilitySystemComponent)?

ASC is the "brain" of GAS on each actor:

```
AActor (Character, Vehicle, Turret)
  -> UAbilitySystemComponent (ASC)
       -> AttributeSets[]     # Data: Condition, Stamina, Survival stats
       -> ActiveEffects[]     # Running buffs/debuffs
       -> GrantedAbilities[]  # Actions the actor can perform
```

ASC handles:
- Owning AttributeSets (the data)
- Managing active GameplayEffects (buffs, debuffs, stat changes)
- Tracking granted abilities (what the actor can do)
- Replicating all GAS state to clients

## Dependency Rules

```
Features (Inventory, Combat, Dialogue)
  -> Gameplay (ProjectGAS, ProjectCharacter, ProjectVitals)
    -> Foundation (ProjectCore)
      -> GameplayAbilities (Engine)
```

**Critical:** ProjectGAS must NOT depend on Features. Primitives only.

## AttributeSet Composition Pattern

Each actor composes the AttributeSets it needs. GAS ignores missing attributes.

```
Player Character:
  -> UHealthAttributeSet     (Condition, MaxCondition)
  -> USurvivalAttributeSet   (Hydration, Calories, Fatigue)
  -> UStaminaAttributeSet    (Stamina, MaxStamina)
  -> UStatusAttributeSet     (Bleeding, Poisoned, Radiation)

Animal:
  -> UHealthAttributeSet
  -> USurvivalAttributeSet   (no stamina UI needed)

Vehicle:
  -> UHealthAttributeSet     (durability = condition)
  -> UVehicleAttributeSet    (Fuel, Speed - custom)

Turret:
  -> UHealthAttributeSet     (just condition, nothing else)
```

## Available AttributeSets

| Set | Attributes | Use Case |
|-----|------------|----------|
| UHealthAttributeSet | Condition (100), MaxCondition | Anything damageable |
| USurvivalAttributeSet | Hydration (3.0L), Calories (2500 kcal), Fatigue (0-100%) | Living beings |
| UStaminaAttributeSet | Stamina (100), MaxStamina | Actors that tire |
| UStatusAttributeSet | Bleeding, Poisoned, Radiation | Status effects (combat/environment) |

**Survival model (The Long Dark / DayZ pattern):**
- **Condition**: Primary "life" bar (0 = dead)
- **Hydration**: Water store (0 = dehydrated, max 3.0L)
- **Calories**: Energy from food (0 = starving, max 2500 kcal)
- **Fatigue**: Sleep debt (0 = rested, 100 = exhausted)

**Mental model (timelines, drain rates):** See [Vitals Design Vision](../ProjectVitals/docs/design_vision.md)

## Abilities vs Effects (KISS Rule)

| Type | What It Is | Examples |
|------|------------|----------|
| **Ability** | Executable logic, activated by input/event | FireWeapon, Dash, Interact |
| **Effect** | Data-only state change (numbers/tags) | +10 MaxStamina, Regen +0.2/s |

**Rule:** Just numbers/tags? -> Effect. Needs logic/input? -> Ability.

## AbilitySet (Equipment Pattern)

```cpp
// Equip: grant abilities + passive effects
FProjectAbilitySetHandles Handles;
AbilitySet->GiveToAbilitySystem(ASC, &Handles);

// Unequip: revoke everything
UProjectAbilitySet::TakeFromAbilitySystem(ASC, &Handles);
```

**WARNING:** GrantedEffects in AbilitySet MUST use Infinite duration!
Duration effects expire naturally and won't be removed on unequip.

## GE_GenericInstant + ApplyMagnitudes

One generic GameplayEffect (C++) with SetByCaller for all vital attributes.
Any system can apply attribute changes via the utility function.

```
UGE_GenericInstant (C++ class):
  Condition - SetByCaller.Condition
  Stamina   - SetByCaller.Stamina
  Hydration - SetByCaller.Hydration
  Calories  - SetByCaller.Calories
  Fatigue   - SetByCaller.Fatigue
```

**Usage (preferred):**

```cpp
// Any system: Inventory, World, Combat, ProjectVitals, etc.
TArray<FAttributeMagnitude> Effects;
Effects.Add({ ProjectTags::SetByCaller_Condition, 20.f });
Effects.Add({ ProjectTags::SetByCaller_Calories, 250.f });

EApplyMagnitudesResult Result = UProjectGASLibrary::ApplyMagnitudes(ASC, Effects);
```

Unused tags default to 0 (no effect). GAS ignores missing AttributeSets.

## Item Integration (JSON -> GAS)

Items define GAS effects via JSON. The generator creates UObjectDefinition assets (item section) with GAS references.

**Consumable item (JSON):**
```json
{
  "id": "HealthPotion",
  "consumable": {
    "effects": ["/Game/GAS/Effects/GE_GenericInstant"],
    "magnitudes": {
      "SetByCaller.Condition": 50.0
    }
  }
}
```

**When item used:**
1. Inventory calls `UseItem(SlotIndex)`
2. Item's `OnUseEffects` applied to user's ASC
3. `DefaultMagnitudes` passed to GE_GenericInstant (instance overrides apply on top)
4. Condition attribute increases by 50

**Equipment item (JSON):**
```json
{
  "id": "IronSword",
  "equipment": {
    "abilitySet": "/Game/GAS/AbilitySets/AS_IronSword"
  }
}
```

**When item equipped:**
1. Inventory calls `EquipItem(SlotIndex)`
2. `EquipAbilitySet->GiveToAbilitySystem(ASC, &Handles)`
3. Abilities + passive effects granted
4. On unequip: `TakeFromAbilitySystem(ASC, &Handles)`

See [ProjectObject README](../../Resources/ProjectObject/README.md) for JSON schema.

## File Structure

```
ProjectGAS/
  README.md
  Source/ProjectGAS/
    Public/
      ProjectGASLibrary.h         # ApplyMagnitudes utility
      ProjectGASAttributeMacros.h # Shared ATTRIBUTE_ACCESSORS macro
      Attributes/
        HealthAttributeSet.h      # Anything damageable (Condition)
        SurvivalAttributeSet.h    # Living beings (Hydration, Calories, Fatigue)
        StaminaAttributeSet.h     # Actors that tire
        StatusAttributeSet.h      # Status effects (bleeding, poison, radiation)
      Abilities/
        ProjectAbilitySet.h       # Lyra-style grant/revoke
      Effects/
        GE_GenericInstant.h       # Generic SetByCaller GE
    Private/
      ...
```

## Tags Location

Gameplay tags are defined in **ProjectCore** (Foundation), not here.
This keeps tags as a shared vocabulary without GAS compile dependency.

**SetByCaller tags** (for ApplyMagnitudes):
- SetByCaller.Condition, SetByCaller.Stamina
- SetByCaller.Hydration, SetByCaller.Calories, SetByCaller.Fatigue
- SetByCaller.Bleeding, SetByCaller.Poisoned, SetByCaller.Radiation

**State tags** (threshold-based, written by ProjectVitals only):
- State.Condition.OK/Low/Critical/Empty
- State.Stamina.OK/Low/Critical/Empty
- State.Calories.OK/Low/Critical/Empty
- State.Hydration.OK/Low/Critical/Empty
- State.Fatigue.Rested/Tired/Exhausted/Critical (inverted: 0=good, 100=bad)

**Thresholds:**
- Condition/Stamina/Calories/Hydration: OK>70%, Low 40-70%, Critical 20-40%, Empty<=20%
- Fatigue (inverted): Rested<30%, Tired 30-60%, Exhausted 60-85%, Critical>=85%

## GameplayCues

GameplayCue Notifies live in `/ProjectGAS/Content/GameplayCues/`. They bridge GAS events to presentation feedback (audio, VFX, camera).

**Full documentation:** [docs/gameplay_cue.md](docs/gameplay_cue.md)

**Quick example:**
```cpp
// Trigger cue from ability/effect
ASC->ExecuteGameplayCue(FGameplayTag::RequestGameplayTag("GameplayCue.Impact.Physical"), Params);
```

Related plugins:
- [ProjectAudio](../../Resources/ProjectAudio/README.md) - Sound assets referenced by cues
- [ProjectSoundSystem](../../Systems/ProjectSoundSystem/README.md) - Audio playback logic

## References

- [GAS Documentation (GitHub)](https://github.com/tranek/GASDocumentation)
- [Lyra AbilitySet](https://dev.epicgames.com/documentation/en-us/unreal-engine/lyra-sample-game-ability-system)
- [Epic Official Docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/gameplay-attributes-and-attribute-sets-for-the-gameplay-ability-system-in-unreal-engine)
- [Epic GameplayCue Docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/gameplay-cues-in-unreal-engine)
- [Character & Vitals Architecture](../ProjectCharacter/docs/design.md) - SOT for vitals mechanics
