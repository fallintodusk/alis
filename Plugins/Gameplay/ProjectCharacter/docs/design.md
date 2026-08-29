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

## Definition-Driven Character System (Skeletal Assembly Framework)

The runtime character path is definition-driven:

| Pawn Class | Driven By | Status |
|------------|-----------|--------|
| `ADefinitionCharacter` | Hero.json definition + capabilities | Production runtime |

Motion Matching AnimBPs and Mutable content graphs remain asset-driven capabilities.
The PostProcess bridge feeds CMC data into the Motion Matching AnimBP.

**ADefinitionCharacter owns:** capsule, camera, movement, GAS, vitals, input. NO hardcoded mesh subobjects -- meshes come from definition.

**Spawn host:** `ProjectSinglePlay` resolves `CharacterDefinition` and spawns through `ProjectObjectSpawn`.

---

## Mode-Aware Traversal Input

`ADefinitionCharacter` keeps one Enhanced Input mapping context and reuses its
existing actions according to native Character Movement mode:

| Input | Grounded/default | Flying |
|-------|------------------|--------|
| WASD | Yaw-relative planar movement | Yaw-relative horizontal movement |
| Space | Jump | Held ascend |
| Ctrl | Crouch | Held descend |
| Shift | Sprint | Held five-times overview boost |

Flying input never invokes jump or crouch at the same time. Collision remains
owned by Character Movement; preview flight does not teleport or directly mutate
the pawn transform. The normal performance route uses `MaxFlySpeed=12000 cm/s`
and `BrakingDecelerationFlying=6000 cm/s2`. While flying, held Shift temporarily
multiplies maximum speed, acceleration, and flight braking by five so manual
whole-territory inspection reaches useful speed and stops in bounded time. Release
or leaving flight restores the exact prior values. Default grounded sprint/walk,
GAS, rotation policy, camera, animation, and body ownership remain unchanged.

---

## Cross-References

- [Vitals Design Vision](../ProjectVitals/docs/design_vision.md) - Design rationale, timelines, physiology
- [ProjectVitals README](../ProjectVitals/README.md) - Vitals rules and calculations
- [ProjectVitalsUI README](../../UI/ProjectVitalsUI/README.md) - Vitals UI display
- [Assembly architecture](../../Systems/ProjectSkeletalAssembly/docs/architecture.md) - Framework design
- [Capabilities rationale](../ProjectSkeletalCapabilities/docs/architecture.md) - Adapter dependency isolation
- [Parity testing](../../../../docs/testing/character_parity.md) - Automated capture test
