# ProjectGAS TODO

See [README.md](README.md) for design rationale.

---

## Phase 1: AttributeSets (Composition Pattern) - DONE

Split into focused sets for mix-and-match composition:

### 1.1 UHealthAttributeSet - DONE
- [x] Health, MaxHealth
- [x] Handles "Max reduced below current" (clamps Health when MaxHealth decreases)

### 1.2 USurvivalAttributeSet - DONE
- [x] Thirst, Hunger, Energy, Sleep + Max versions
- [x] All with proper clamping and max-change handling

### 1.3 UStaminaAttributeSet - DONE
- [x] Stamina, MaxStamina
- [x] Separate from Energy (short-term vs long-term fatigue)

**Composition examples:**
```
Player:  Health + Survival + Stamina
Animal:  Health + Survival
Vehicle: Health only (or + custom VehicleAttributeSet)
Turret:  Health only
```

---

## Phase 2: Generic Effects - DONE

### 2.1 UGE_GenericInstant (C++ class) - DONE

C++ GameplayEffect with SetByCaller modifiers for all vital attributes.
Configured in constructor - no editor asset needed.

```
UGE_GenericInstant:
  DurationPolicy: Instant
  Modifiers:
    Health  - Add - SetByCaller (SetByCaller.Health)
    Stamina - Add - SetByCaller (SetByCaller.Stamina)
    Thirst  - Add - SetByCaller (SetByCaller.Thirst)
    Hunger  - Add - SetByCaller (SetByCaller.Hunger)
    Energy  - Add - SetByCaller (SetByCaller.Energy)
    Sleep   - Add - SetByCaller (SetByCaller.Sleep)
```

- [x] Created UGE_GenericInstant in Effects/
- [x] SetByCaller tags defined in ProjectCore

### 2.2 UProjectGASLibrary - DONE

Utility function for any system to apply magnitudes:

```cpp
TArray<FAttributeMagnitude> Effects;
Effects.Add({ ProjectTags::SetByCaller_Health, 20.f });
UProjectGASLibrary::ApplyMagnitudes(ASC, Effects);
```

- [x] FAttributeMagnitude struct (Tag + float)
- [x] ApplyMagnitudes() function
- [x] ApplySingleMagnitude() convenience function

---

## Phase 3: AbilitySet - DONE

### 3.1 UProjectAbilitySet - DONE
- [x] GiveToAbilitySystem() / TakeFromAbilitySystem()
- [x] Documented: GrantedEffects MUST be Infinite duration
- [x] Not BlueprintCallable (UHT rejects raw pointer params)
- [x] Fixed include: Engine/PrimaryDataAsset.h

---

## Phase 4: Module Setup - DONE

- [x] ProjectGAS.uplugin
- [x] ProjectGAS.Build.cs
- [x] ProjectGASModule.h/cpp
- [x] ProjectGASAttributeMacros.h (shared macro, no duplication)

---

## Validation Checklist

- [x] ProjectGAS does NOT depend on any Features plugin
- [x] Gameplay tags defined in ProjectCore (SetByCaller.Health, etc.)
- [x] AttributeSets replicate correctly (DOREPLIFETIME_CONDITION_NOTIFY)
- [x] Max-change clamps current value (Health clamped when MaxHealth decreases)
- [x] UGE_GenericInstant with SetByCaller modifiers (C++)
- [x] ApplyMagnitudes utility for any system to use
