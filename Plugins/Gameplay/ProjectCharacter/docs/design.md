# Character & Vitals Architecture

SOT for vitals mechanics: GAS state, Vitals calculations, Character wiring.

---

## Plugin Responsibilities

| Plugin | Owns | Does NOT Do |
|--------|------|-------------|
| **ProjectGAS** | State + primitives (AttributeSets, Tags, GEs, helper library) | Calculations, rules |
| **ProjectVitals** | Calculations + rules (drain rates, thresholds, regen gating) | ASC lifetime, character types |
| **ProjectCharacter** | Wiring + lifecycle (ASC creation, component attachment) | Math, UI |

---

## Data Flow (One Direction)

```
Vitals -> apply GE to ASC -> attributes/tags replicate -> UI reads
```

**Key rules:**
- GAS never calls back into Vitals
- UI never calls Vitals
- Everything goes through ASC as shared bus

---

## Dependency Graph (No Cycles)

```
ProjectVitals -> ProjectGAS
ProjectCharacter -> ProjectGAS
Features (Inventory etc.) -> ProjectGAS + ProjectCharacter
UI -> ProjectCharacter + ProjectGAS
```

**ProjectVitals depends only on:**
- Engine `IAbilitySystemInterface` (get ASC from any actor)
- ProjectGAS primitives (attributes, tags, helper, generic GEs)

---

## Binding

**ProjectCharacter constructor:**
- Creates ASC (existing)
- Creates `UProjectVitalsComponent` as subobject

**Server flow:**
1. `PossessedBy()` -> `InitAbilityActorInfo()` (existing)
2. Grant startup sets (existing)
3. `VitalsComponent->Start()` (starts timer)

**Client flow:**
- Does NOT simulate vitals
- Receives replicated attributes/tags
- UI binds to ASC delegates

---

## SOT Locations

| Topic | Location |
|-------|----------|
| Vitals rules & calculations | `Plugins/Gameplay/ProjectVitals/README.md` |
| GAS primitives (AttributeSets, Tags) | `Plugins/Gameplay/ProjectGAS/README.md` |
| Character wiring & lifecycle | `Plugins/Gameplay/ProjectCharacter/README.md` |
| Vitals widgets & ViewModel | `Plugins/UI/ProjectVitalsUI/README.md` |
| HUD layout & slots | `Plugins/UI/ProjectHUD/README.md` |
| UI framework (layers, registry, MVVM) | `Plugins/UI/ProjectUI/README.md` |
| Design rationale & philosophy | `Plugins/Gameplay/ProjectVitals/docs/design_vision.md` |

**UI Extension Pattern (Lyra-style):**
- ProjectUI owns layers + extension registry (`UUIExtensionSubsystem`)
- Layer tags (`UI.Layer.*`) are GameplayTags defined in ProjectUI, not per-feature
- ProjectHUD defines slots (e.g., `HUD.Slot.VitalsMini`) - depends on ProjectUI only
- Features register widgets into slots via ProjectUI registry
- All ASC bindings happen in feature widgets (e.g., ProjectVitalsUI), not in ProjectHUD
- No one depends on ProjectHUD directly

---

---

## Modular Character System (Skeletal Assembly Framework)

Two character systems coexist:

| System | Pawn Class | Driven By | Status |
|--------|-----------|-----------|--------|
| Legacy | `AProjectCharacter` / BP_Hero | Blueprint defaults + C++ constructor | Production default |
| Modular | `ADefinitionCharacter` | Hero.json definition + capabilities | Parity-tested, interim |

**Switching:** `project.character.switch <legacy|modular>` (respawn-based, guarded behind !UE_BUILD_SHIPPING)

**What stays legacy for v1:**
- Retarget AnimBPs
- Mutable content graphs
- BP_Hero movement tuning (gait/strafe curves)
- Traversal, foley, smart-object behavior

**Bridged for modular v1:**
- Motion Matching AnimBP -- PostProcess bridge feeds CMC data via reflection (Gait defaults to Run)

**ADefinitionCharacter owns:** capsule, camera, movement, GAS, vitals, input. NO hardcoded mesh subobjects -- meshes come from definition.

**Switch host:** `ProjectSinglePlay` -- owns `ESinglePlayCharacterSystem` enum and `SwitchCharacterSystemRuntime()`.

---

## Cross-References

- [Vitals Design Vision](../ProjectVitals/docs/design_vision.md) - Design rationale, timelines, physiology
- [ProjectVitals README](../ProjectVitals/README.md) - Vitals rules and calculations
- [ProjectVitalsUI README](../../UI/ProjectVitalsUI/README.md) - Vitals UI display
- [Assembly architecture](../../Systems/ProjectSkeletalAssembly/docs/architecture.md) - Framework design
- [Capabilities rationale](../ProjectSkeletalCapabilities/docs/architecture.md) - Adapter dependency isolation
- [Parity testing](../../../../docs/testing/character_parity.md) - Automated capture test
