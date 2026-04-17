# Poison Gas Cloud (Environmental Hazard via GAS)

**SUPERSEDED** by `UEnvironmentEffectComponent` in `ProjectObjectCapabilities`.
See `todo/00_current/city17_implement_hazards_and_death.md`.
Delete this file after the EnvironmentEffect capability is merged and verified.

---

## Problem (original)

Need poisonous fog zones where the player enters a gas cloud (box collision) and dies within 3-5 seconds. Currently the level has visual fog (`BP_FogSheet`) and trigger boxes placed, but no gameplay logic wired — entering the cloud does nothing.

## What Already Exists

| Layer | Asset | Location |
|-------|-------|----------|
| Attribute | `StatusAttributeSet.Poisoned` (0-1.0) | `ProjectGAS/Attributes/StatusAttributeSet.h` |
| SetByCaller Tag | `SetByCaller.Poisoned` | `ProjectCore/ProjectGameplayTags.h` |
| Damage Tag | `Damage.Type.Poison` | `ProjectCore/ProjectGameplayTags.h` |
| GameplayCue Tag | `GameplayCue.Status.Poisoned` | Defined, no BP yet |
| Config Tiers | mild(0.1), moderate(0.3), severe(0.7), lethal(1.0) | `ProjectVitals/Data/vitals_config.json` |
| Bleeding Pattern | `ApplyBleedingDrain()` — exact template for poison | `ProjectVitalsComponent.cpp:582` |
| Generic Effect | `GE_GenericInstant` + `ApplyMagnitudes()` | `ProjectGAS/Effects/` |
| Fog Visual | `BP_FogSheet` with materials/mesh | `Content/Project/Placeables/Environment/FogSheet/` |

## What's Missing

### 1. `ApplyPoisonDrain()` in ProjectVitalsComponent

Mirror of `ApplyBleedingDrain()`. Reads `Poisoned` intensity, drains Condition each tick.

**Files:**
- `Plugins/Gameplay/ProjectVitals/Source/ProjectVitals/Public/ProjectVitalsComponent.h`
- `Plugins/Gameplay/ProjectVitals/Source/ProjectVitals/Private/ProjectVitalsComponent.cpp`

### 2. Trigger Actor (BP_PoisonGasVolume)

Blueprint Actor with BoxCollision:
- `OnBeginOverlap` → `ApplyMagnitudes(ASC, [{SetByCaller.Poisoned, Intensity}])`
- `OnEndOverlap` → `ApplyMagnitudes(ASC, [{SetByCaller.Poisoned, -Intensity}])`
- Exposed property: `Intensity` (float, default 1.0)

### 3. GameplayCue (optional, visual polish)

`GC_Status_Poisoned` in `ProjectGAS/Content/GameplayCues/`:
- Green/yellow post-process tint
- Cough/gasp SFX loop
- Triggered by `GameplayCue.Status.Poisoned`

## Data Flow

```
Player enters box → OnBeginOverlap
  → ApplyMagnitudes(ASC, Poisoned = 1.0)

Every vitals tick (1 sec):
  → ApplyPoisonDrain() reads Poisoned = 1.0
  → Drain = Intensity * PoisonDrainPerSec * DeltaTime
  → Condition -= Drain

Player exits box → OnEndOverlap
  → ApplyMagnitudes(ASC, Poisoned = -1.0)
  → Drain stops, regen can begin
```

## Death Timing Math

Condition = 75 (current config). Target: death in 3-5 sec.

| PoisonDrainPerSec | Lethal (1.0) | Severe (0.7) | Moderate (0.3) |
|--------------------|-------------|--------------|----------------|
| 20.0 | 3.75 sec | 5.4 sec | 12.5 sec |
| 25.0 | 3.0 sec | 4.3 sec | 10.0 sec |
| 15.0 | 5.0 sec | 7.1 sec | 16.7 sec |

Compare: `BleedingDrainPerSecond = 0.5` (much slower, survivable).

**Recommendation:** `PoisonDrainPerSecond = 20.0` for lethal gas clouds.

## Industry Reference

Matches standard approach in Tarkov, The Division 2, Rust:
- **Separate detection from damage** — volume sets/clears status, vitals tick handles drain
- **Intensity-based** — float (0-1) allows different gas concentrations per zone
- **Server-authoritative** — vitals tick is server-only, no client cheating
- **Visual feedback decoupled** — GameplayCue handles presentation independently

## Next Steps

### Step 1 — C++: `ApplyPoisonDrain()` (compile required)

**Header** (`ProjectVitals/Source/ProjectVitals/Public/ProjectVitalsComponent.h`):
- Add `float PoisonDrainPerSecond = 20.f;` property next to `BleedingDrainPerSecond` (line 155)
- Add `void ApplyPoisonDrain(UAbilitySystemComponent* ASC, float DeltaTime);` declaration next to `ApplyBleedingDrain` (line 270)

**Implementation** (`ProjectVitals/Source/ProjectVitals/Private/ProjectVitalsComponent.cpp`):
- Copy `ApplyBleedingDrain()` (line 582-614), rename to `ApplyPoisonDrain()`
- Replace `GetBleeding()` → `GetPoisoned()`, `BleedingDrainPerSecond` → `PoisonDrainPerSecond`
- In `TickVitals()`, add call `ApplyPoisonDrain(ASC, TickInterval);` after line 250 (`ApplyBleedingDrain`)

**Then compile the project.**

### Step 2 — Blueprint: `BP_PoisonGasVolume` (in Unreal Editor)

1. Create new Actor Blueprint: `Content/Project/Placeables/Environment/BP_PoisonGasVolume`
2. Add `BoxCollision` component, size it to match the gas cloud area
3. Add exposed property: `Intensity` (float, default 1.0)
4. `OnComponentBeginOverlap`:
   - Get overlapping actor's ASC via `IAbilitySystemInterface`
   - Call `UProjectGASLibrary::ApplyMagnitudes(ASC, [{SetByCaller.Poisoned, Intensity}])`
5. `OnComponentEndOverlap`:
   - Call `UProjectGASLibrary::ApplyMagnitudes(ASC, [{SetByCaller.Poisoned, -Intensity}])`
6. Place in level at the same position as existing `BP_FogSheet` visual

### Step 3 — Test

- [ ] Walk into gas cloud → Condition bar drops rapidly
- [ ] Death in ~3.75 sec at lethal intensity (1.0)
- [ ] Walk out before death → drain stops, regen begins
- [ ] Check VitalsPanel shows Poisoned status active
- [ ] Verify server-only (no client-side drain)

### Step 4 — Polish (optional, later)

- [ ] Add `poisonDrainPerSecond` to `vitals_config.json` under `condition.status.poisoned` for designer tuning
- [ ] Create `GC_Status_Poisoned` GameplayCue BP in `ProjectGAS/Content/GameplayCues/`
  - Green/yellow post-process screen tint
  - Cough/gasp SFX loop
  - Triggered by `GameplayCue.Status.Poisoned` tag
